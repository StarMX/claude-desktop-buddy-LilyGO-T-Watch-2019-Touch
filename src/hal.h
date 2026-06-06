#pragma once

#define XPOWERS_CHIP_AXP202
#include "XPowersLib.h"
#include "SensorBMA423.hpp"
#include "SensorPCF8563.hpp"
#include "TouchDrv.hpp"
#include <TFT_eSPI.h>
#include <Wire.h>

// ============================================================
// LilyGO T-Watch 2019 硬件抽象层
// ============================================================
// 提供统一的硬件初始化与访问接口，屏蔽底层引脚和器件细节。
// 所有外设共享两个 I2C 总线：
//   Wire1 (SDA=21, SCL=22) — 传感器总线（AXP202, BMA423, PCF8563）
//   Wire  (SDA=23, SCL=32) — 触摸总线（FT6236）
//
// TFT 引脚（MOSI/SCLK/DC/CS/BL/RST）由 platformio.ini 的
// build_flags 通过 -D 宏传递给 TFT_eSPI，此处不再重复定义。

// ---- 传感器 I2C 引脚（AXP202 / BMA423 / PCF8563 共享） ----
constexpr int8_t SENSOR_SDA = 21;
constexpr int8_t SENSOR_SCL = 22;

// ---- 触摸 I2C 引脚（FT6236） ----
constexpr int8_t TOUCH_SDA = 23;
constexpr int8_t TOUCH_SCL = 32;

// ---- 中断引脚 ----
constexpr int8_t IRQ_BMA423  = 39;
constexpr int8_t IRQ_FT6236  = 38;
constexpr int8_t IRQ_PCF8563 = 37;
constexpr int8_t IRQ_BUTTON  = 36;
constexpr int8_t IRQ_AXP202  = 35;

// ---- 扬声器引脚（标准底板） ----
constexpr int8_t SPEAKER_PIN = 25;

// ---- 全局外设对象 ----
inline XPowersAXP202   axp;
inline SensorBMA423    bma;
inline SensorPCF8563   rtc;
inline TouchDrvFT6X36  ts;
inline TFT_eSPI        tft;

// ---- 背光 PWM 相关常量 ----
constexpr uint32_t BL_PWM_FREQ = 5000; // 背光 PWM 频率 5 kHz
constexpr uint8_t  BL_PWM_BITS = 8;    // 背光 PWM 分辨率 8 位

// ---- 蜂鸣器 PWM 相关常量 ----
constexpr uint32_t BEEP_PWM_FREQ_DEFAULT = 2000; // 蜂鸣器默认频率

// ---- 亮度等级到 PWM 值的映射 ----
static constexpr uint8_t _brightnessPwm[] = {0, 50, 100, 150, 200};
static constexpr uint8_t _brightnessLevels = sizeof(_brightnessPwm) / sizeof(_brightnessPwm[0]);

// 当前背光 PWM 值（供 halBacklightOn 恢复使用）
inline uint8_t _currentBrightnessPwm = 200;

// ============================================================
// halInit — 一次性硬件初始化
// ============================================================
inline bool halInit() {
    // ---- 初始化 I2C 总线 ----
    Wire1.begin(SENSOR_SDA, SENSOR_SCL); // 传感器总线
    Wire.begin(TOUCH_SDA, TOUCH_SCL);    // 触摸总线

    // ---- 初始化 AXP202 电源管理 ----
    if (axp.init(Wire1, SENSOR_SDA, SENSOR_SCL)) {
        // 启用 DC3（内核供电）、LDO2（TFT 背光）、LDO3（外设供电）
        axp.enableDC3();
        axp.enableLDO2();
        axp.enableLDO3();
        // LDO2 电压设为 3.3 V（TFT 背光供电）
        axp.setLDO2Voltage(3300);
        // 启用电池充电
        axp.enableCharge();
        // 启用库仑计数器
        axp.enableCoulomb();
        // 使能电源按键中断
        axp.enableIRQ(XPOWERS_AXP202_PKEY_SHORT_IRQ |
                      XPOWERS_AXP202_PKEY_LONG_IRQ);
    }

    // ---- 初始化 BMA423 加速度计 ----
    if (bma.begin(Wire1, BMA4XX_I2C_ADDR_SDO_HIGH)) {
        // 配置加速度计量程 ±2g，输出数据速率 100 Hz
        bma.configAccelerometer(
            OperationMode::NORMAL,
            AccelFullScaleRange::FS_2G,
            100.0f,
            AccelBandwidth::NORMAL_AVG4,
            AccelPerfMode::CONTINUOUS_MODE);
    }

    // ---- 初始化 PCF8563 实时时钟 ----
    rtc.begin(Wire1);

    // ---- 初始化 FT6236 触摸屏 ----
    ts.setPins(-1, IRQ_FT6236); // RST=-1（无硬件复位），IRQ=GPIO38
    if (ts.begin(Wire, FT6X36_SLAVE_ADDRESS)) {
        ts.setThreshold(30); // 触摸灵敏度阈值
    }

    // ---- 初始化 TFT 显示屏 ----
    tft.init();
    tft.setRotation(0);           // 竖屏方向
    tft.fillScreen(TFT_BLACK);    // 清屏

    // ---- 初始化背光 PWM（ESP32 Arduino Core 3.x API） ----
    ledcAttach(TFT_BL, BL_PWM_FREQ, BL_PWM_BITS);
    ledcWrite(TFT_BL, 200); // 默认亮度

    // ---- 初始化扬声器引脚 ----
    pinMode(SPEAKER_PIN, OUTPUT);

    // ---- 初始化物理按键（GPIO36，低电平有效） ----
    pinMode(IRQ_BUTTON, INPUT_PULLUP);

    return true;
}

// ============================================================
// 辅助函数
// ============================================================

// 读取 BMA423 加速度计三轴数据（单位：g，内部 m/s² 除以 9.80665f 转换）
inline void halGetAccel(float &ax, float &ay, float &az) {
    AccelerometerData data;
    if (bma.readData(data)) {
        constexpr float MPS2_TO_G = 1.0f / 9.80665f;
        ax = data.mps2.x * MPS2_TO_G;
        ay = data.mps2.y * MPS2_TO_G;
        az = data.mps2.z * MPS2_TO_G;
    }
}

// 设置背光亮度等级（0-4），映射到预设 PWM 值
inline void halSetBrightness(uint8_t level) {
    if (level >= _brightnessLevels) level = _brightnessLevels - 1;
    _currentBrightnessPwm = _brightnessPwm[level];
    ledcWrite(TFT_BL, _currentBrightnessPwm);
}

// 打开背光（恢复到当前亮度等级对应的 PWM 值）
inline void halBacklightOn() {
    ledcWrite(TFT_BL, _currentBrightnessPwm);
}

// 关闭背光（PWM 置零）
inline void halBacklightOff() {
    ledcWrite(TFT_BL, 0);
}

// 蜂鸣器鸣响：以指定频率和持续时间在 GPIO25 上输出方波
inline void halBeep(uint16_t freq, uint16_t dur) {
    ledcAttach(SPEAKER_PIN, freq, 8);
    ledcWrite(SPEAKER_PIN, 128); // 50% 占空比
    delay(dur);
    ledcWrite(SPEAKER_PIN, 0);
    ledcDetach(SPEAKER_PIN);
    pinMode(SPEAKER_PIN, OUTPUT); // 恢复为普通输出
}

// 关机（通过 AXP202 切断主电源）
inline void halPowerOff() {
    axp.shutdown();
}

// 检测 USB 是否连接
inline bool halIsUSB() {
    return axp.isVbusIn();
}

// ---- 物理按键（GPIO36，低电平有效） ----
// 读取物理按键状态：按下时返回 true（低电平）
inline bool halButtonPressed() {
    return digitalRead(IRQ_BUTTON) == LOW;
}

// 检测 AXP202 电源按键短按
inline bool halPekShortPress() {
    if (axp.isPekeyShortPressIrq()) {
        axp.clearIrqStatus();
        return true;
    }
    return false;
}
