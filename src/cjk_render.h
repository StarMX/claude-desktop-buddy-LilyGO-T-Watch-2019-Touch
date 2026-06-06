// 基于 Sprite 的 ASCII + GBK 混合字形渲染器（CJK 构建变体）。
//
// 本模块针对公共 sprite API（drawPixel / fillRect）重新实现了字形 blit 路径，
// 以兼容 TFT_eSPI，不依赖 M5Display 的 writeHzk()/writeHzkAsc()/writeHzkGbk()。
//
// 字体来源：
//   - ASCII 字节（< 0x80）                  → ASC12（6x12，Fusion Pixel Font，OFL）
//   - GBK 双字节（b1 在 0xA1-0xFE，b2 同理）→ GB2312_L1.h（区号 16-55，3760 字形）
//
// 仅在定义了 `CC_BUDDY_CJK_DISPLAY` 时编译。

#pragma once

#ifdef CC_BUDDY_CJK_DISPLAY

#include <Arduino.h>

class TFT_eSprite;

// 在 `spr` 上从 (x, y) 开始绘制 NUL 结尾的字节字符串。
// 逐字节遍历：ASCII（1 字节）渲染 6x12；GBK 双字节（均 >= 0xA1）渲染 12x12。
// 超出 sprite 边界的像素会被静默裁剪。
void cjkDrawMixed(TFT_eSprite* spr, const char* text, int x, int y,
                  uint16_t color, uint16_t bgcolor);

// 若对给定文本调用 `cjkDrawMixed` 所需的像素宽度。适用于水平居中 / 溢出检查。
int  cjkMeasureMixed(const char* text);

// 每行折行输出的最大 NUL 结尾字节数。
// 屏幕宽 240px，x=4 起始，可用 236px：纯 ASCII 39 字符 × 1 字节 = 39；
// 纯 GBK 19 字符 × 2 字节 = 38。加 NUL 和余量取 44。
#define CJK_ROW_CAP 44

// 像素与 GBK 感知的折行。将 `in` 拆分为每行 ≤ `maxPx` 像素的行，
// 绝不在 GBK 双字节对中间断开。`out` 中每行以 NUL 结尾。
// 返回实际写入的行数。ASCII 字符计 6 像素宽，GBK 双字节计 12 像素宽。
// 硬折行：无词感知逻辑——在恰好放不下的字形处断行。
uint8_t cjkWrapInto(const char* in, char out[][CJK_ROW_CAP],
                    uint8_t maxRows, int maxPx);

#endif
