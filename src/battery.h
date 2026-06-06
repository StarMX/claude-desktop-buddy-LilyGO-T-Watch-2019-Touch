#pragma once
#include "hal.h"

// T-Watch 2019 电池电量（SoC）估算模块，内置约 300 mAh LiPo 电池。
//
// 纯电压估算 SoC 在此场景下本质上不可靠：
//
//   - 1S LiPo 的 OCV-SOC 曲线在 30–80% 区间非常平坦，
//     几 mV 的测量噪声就对应数个百分点的偏差。
//   - BLE 发射和屏幕刷新会通过内阻将测量电压拉低数十到
//     上百 mV，相同实际 SoC 在忙碌时读数系统性偏低。
//   - **最关键的问题**：充电时测量的电压因充电极化而被抬高，
//     拔掉 USB 后电压回落 50–100 mV。充电时的百分比和
//     刚拔线时的百分比描述的是同一个物理电池状态，
//     但读数相差 10–15 个百分点。
//
// 本模块通过从纯电压切换到**库仑计数**来修正拔线跳变：
// AXP202 内置硬件库仑计数器，我们读取其累积的 mAh 进出量
// 并换算为 SoC%。
//
//   1. 开机时，取前 30 个电压采样的中值作为 OCV 锚点
//      （启动时负载低，是 OCV 读数噪声最小的时刻）。
//   2. 在同一时刻锚定库仑计数器的值。
//   3. SoC_now = anchor_pct + (coulomb_now - coulomb_anchor) / 300 mAh × 100
//   4. 每当检测到"充满"（V > 4.10 且充电电流 < 30 mA）时重新锚定，
//      以便在日常使用中修正漂移。
//
// 额外平滑：对推导出的 SoC 值使用小型环形缓冲区取中值。
// 能捕获 AXP202 寄存器的瞬态毛刺，而无需长电压窗口
// 做噪声抑制（库仑计数器已经承担了这部分工作）。

namespace battery {

constexpr uint32_t SAMPLE_INTERVAL_MS  = 1000;   // 1 Hz 采样间隔
constexpr uint8_t  V_WINDOW            = 30;     // OCV 锚点使用的电压采样数
constexpr uint8_t  PCT_WINDOW          = 8;      // SoC 平滑窗口大小
constexpr float    BAT_CAPACITY_MAH    = 300.0f; // T-Watch 2019 内置电池

// 重新锚定阈值：电压稳定、已充满、低电流 → 电池确实已满。
constexpr float    FULL_VOLTAGE        = 4.10f;
constexpr float    FULL_CURRENT_MA     = 30.0f;

inline float    _vSamples[V_WINDOW];
inline uint8_t  _vCount = 0;
inline uint8_t  _vHead  = 0;

inline int      _pctSamples[PCT_WINDOW];
inline uint8_t  _pctCount = 0;
inline uint8_t  _pctHead  = 0;

inline uint32_t _nextSampleMs = 0;

inline bool     _anchored      = false;
inline float    _anchorPct     = 50.0f;     // 锚定时刻的 SoC
inline float    _anchorCoulomb = 0.0f;      // 锚定时刻的 GetCoulombData() 值

// LiPo OCV-SOC 分段查找表（仅在 OCV 锚定时使用）。
struct _Pt { float v; int pct; };
static const _Pt _OCV[] = {
    {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.90f, 70},
    {3.80f, 60},  {3.75f, 50}, {3.70f, 40}, {3.65f, 30},
    {3.60f, 20},  {3.50f, 10}, {3.30f,  5}, {3.00f,  0},
};
inline int _ocvToPct(float v) {
    const int N = sizeof(_OCV) / sizeof(_OCV[0]);
    if (v >= _OCV[0].v) return _OCV[0].pct;
    if (v <= _OCV[N-1].v) return _OCV[N-1].pct;
    for (int i = 0; i < N - 1; i++) {
        if (v <= _OCV[i].v && v >= _OCV[i+1].v) {
            float span_v = _OCV[i].v   - _OCV[i+1].v;
            float span_p = (float)(_OCV[i].pct - _OCV[i+1].pct);
            float frac   = (v - _OCV[i+1].v) / span_v;
            int pct = (int)(_OCV[i+1].pct + frac * span_p + 0.5f);
            if (pct < 0) return 0;
            if (pct > 100) return 100;
            return pct;
        }
    }
    return 0;
}

inline float _vMedian() {
    if (_vCount == 0) return 4.20f;
    float buf[V_WINDOW];
    for (uint8_t i = 0; i < _vCount; i++) buf[i] = _vSamples[i];
    for (uint8_t i = 1; i < _vCount; i++) {
        float k = buf[i]; int j = i - 1;
        while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; }
        buf[j+1] = k;
    }
    return buf[_vCount / 2];
}

inline int _pctMedian() {
    if (_pctCount == 0) return 50;
    int buf[PCT_WINDOW];
    for (uint8_t i = 0; i < _pctCount; i++) buf[i] = _pctSamples[i];
    for (uint8_t i = 1; i < _pctCount; i++) {
        int k = buf[i]; int j = i - 1;
        while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; }
        buf[j+1] = k;
    }
    return buf[_pctCount / 2];
}

inline void _pushVoltage(float v) {
    _vSamples[_vHead] = v;
    _vHead = (_vHead + 1) % V_WINDOW;
    if (_vCount < V_WINDOW) _vCount++;
}
inline void _pushPct(int pct) {
    _pctSamples[_pctHead] = pct;
    _pctHead = (_pctHead + 1) % PCT_WINDOW;
    if (_pctCount < PCT_WINDOW) _pctCount++;
}

inline void _setAnchor(float pct) {
    _anchorPct     = pct;
    _anchorCoulomb = axp.getCoulombData();
    _anchored      = true;
}

// 若已锚定则通过库仑计数计算 SoC，否则回退到 OCV 查表。
inline int _computeSoC() {
    if (_anchored) {
        float delta_mAh = axp.getCoulombData() - _anchorCoulomb;
        float pct = _anchorPct + (delta_mAh / BAT_CAPACITY_MAH) * 100.0f;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return (int)(pct + 0.5f);
    }
    return _ocvToPct(_vMedian());
}

// 每次主循环调用一次。内部 1 Hz 节流确保开销可忽略。
inline void poll() {
    uint32_t now = millis();
    if (_nextSampleMs != 0 && (int32_t)(now - _nextSampleMs) < 0) return;
    _nextSampleMs = now + SAMPLE_INTERVAL_MS;

    // getBattVoltage() 返回毫伏，转换为伏特
    float v = axp.getBattVoltage() / 1000.0f;
    _pushVoltage(v);

    // 首次锚定：采集满电压窗口后，取中值作为 OCV 读数，
    // 并将 SoC 与库仑计数器绑定。
    if (!_anchored && _vCount >= V_WINDOW) {
        _setAnchor((float)_ocvToPct(_vMedian()));
    }

    // 电池确实充满时重新锚定（修正库仑计数器长期累积的漂移）。
    // getBatteryChargeCurrent() 返回充电电流 mA（放电为负）
    float current = axp.getBatteryChargeCurrent();
    if (v >= FULL_VOLTAGE && fabsf(current) < FULL_CURRENT_MA) {
        _setAnchor(100.0f);
    }

    _pushPct(_computeSoC());
}

inline int percent() {
    if (_pctCount == 0) {
        // 采样前的回退值；调用方通常不应这么早查询。
        return _ocvToPct(axp.getBattVoltage() / 1000.0f);
    }
    return _pctMedian();
}

inline void begin() {
    // 库仑计数器已在 halInit() 中启用，此处无需操作。
}

} // namespace battery
