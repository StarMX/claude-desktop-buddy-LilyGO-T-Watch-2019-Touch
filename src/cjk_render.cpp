#ifdef CC_BUDDY_CJK_DISPLAY

#include "cjk_render.h"
// TFT_eSprite 通过 hal.h 间接引入（hal.h 包含 TFT_eSPI 库头文件）。
#include "hal.h"
#include "Fonts/ASC12.h"        // 6x12 ASCII（Fusion Pixel Font，OFL）
#include "Fonts/GB2312_L1.h"    // 12x12 GB2312（Fusion Pixel Font，OFL）

// 每种字形族的宽度/高度。ASCII 字符宽度为 CJK 字符的一半；两者共享 12 像素行高，
// 使光标计算保持简单，并将 HUD 区域缩减至约 52 像素（相比之前 16 像素变体的 68 像素）。
static const int ASC_W = 6;
static const int GBK_W = 12;
static const int GLYPH_H = 12;

// 判断从 `p` 开始的两个字节是否构成有效的 GBK 对（均在 0xA1..0xFE 范围内）。
// GB2312 一级汉字均在此范围内；1-9 区的符号和拼音也在此范围，
// 但我们仅提供 16-55 区的字形，其余会回退到 '?' 替换。
static inline bool isGbkLead(const uint8_t* p) {
  const uint8_t b1 = p[0], b2 = p[1];
  return b1 >= 0xA1 && b1 <= 0xFE && b2 >= 0xA1 && b2 <= 0xFE;
}

// 将位图字形 blit 到 sprite 上。`glyph` 为行优先格式：每行 `bytes_per_row` 字节，
// 每字节内 MSB 在前。超出字形可见宽度的位被忽略。超出 sprite 边界的像素
// 由 TFT_eSprite::drawPixel 静默裁剪。
static void blitGlyph(TFT_eSprite* spr, const uint8_t* glyph,
                      int x, int y,
                      int w_px, int h_px, int bytes_per_row,
                      uint16_t color, uint16_t bgcolor) {
  const bool fillBg = (color != bgcolor);
  for (int row = 0; row < h_px; row++) {
    for (int b = 0; b < bytes_per_row; b++) {
      const uint8_t bits = pgm_read_byte(glyph + row * bytes_per_row + b);
      uint8_t mask = 0x80;
      for (int bit = 0; bit < 8; bit++) {
        const int col = b * 8 + bit;
        if (col >= w_px) break;
        const bool on = (bits & mask) != 0;
        if (on) {
          spr->drawPixel(x + col, y + row, color);
        } else if (fillBg) {
          spr->drawPixel(x + col, y + row, bgcolor);
        }
        mask >>= 1;
      }
    }
  }
}

static void drawAsc12(TFT_eSprite* spr, uint8_t c, int x, int y,
                      uint16_t color, uint16_t bgcolor) {
  // ASC12 索引 0..127（0..0x1F 槽位填零，使不可打印码位渲染为空白）。
  // 每个字形：12 行 × 1 字节。
  if (c >= ASC12_GLYPH_COUNT) return;
  const uint8_t* glyph = ASC12 + (uint32_t)c * ASC12_BYTES_PER_GLYPH;
  blitGlyph(spr, glyph, x, y, ASC_W, GLYPH_H,
            ASC12_BYTES_PER_ROW, color, bgcolor);
}

static void drawGb2312(TFT_eSprite* spr, uint8_t b1, uint8_t b2, int x, int y,
                       uint16_t color, uint16_t bgcolor) {
  const uint8_t* glyph = gb2312_l1_glyph(b1, b2);
  if (glyph == nullptr) {
    // 超出我们提供的区号范围（二级汉字、用户定义区或无效对）——
    // 回退为两个 '?' 字符以标记空缺，避免留下空白槽位。
    drawAsc12(spr, '?', x,         y, color, bgcolor);
    drawAsc12(spr, '?', x + ASC_W, y, color, bgcolor);
    return;
  }
  blitGlyph(spr, glyph, x, y, GBK_W, GLYPH_H,
            GB2312_L1_BYTES_PER_ROW, color, bgcolor);
}

void cjkDrawMixed(TFT_eSprite* spr, const char* text, int x, int y,
                  uint16_t color, uint16_t bgcolor) {
  if (spr == nullptr || text == nullptr) return;
  // 当下一个字形水平放不下时停止渲染。需要多行布局的调用者应先通过 cjkWrapInto 预折行。
  const int max_right = spr->width();
  const uint8_t* p = reinterpret_cast<const uint8_t*>(text);
  int cx = x;
  while (*p) {
    int glyph_w;
    int advance;
    bool is_gbk = isGbkLead(p);
    if (is_gbk) {
      glyph_w = GBK_W;
      advance = 2;
    } else if (*p == '\n' || *p == '\r') {
      p++;
      continue;
    } else {
      glyph_w = ASC_W;
      advance = 1;
    }
    if (cx + glyph_w > max_right) break;
    if (is_gbk) drawGb2312(spr, p[0], p[1], cx, y, color, bgcolor);
    else        drawAsc12(spr, *p, cx, y, color, bgcolor);
    cx += glyph_w;
    p  += advance;
  }
}


uint8_t cjkWrapInto(const char* in, char out[][CJK_ROW_CAP],
                    uint8_t maxRows, int maxPx) {
  if (in == nullptr || maxRows == 0) return 0;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(in);
  uint8_t row = 0;
  uint16_t col_bytes = 0;
  int cx = 0;
  while (*p && row < maxRows) {
    int glyph_w;
    int advance;
    if (isGbkLead(p)) {
      glyph_w = GBK_W;
      advance = 2;
    } else if (*p == '\n' || *p == '\r') {
      // 换行符立即强制折行，然后跳过该字面字节。
      out[row][col_bytes] = 0;
      row++;
      if (row >= maxRows) return row;
      col_bytes = 0;
      cx = 0;
      p++;
      continue;
    } else {
      glyph_w = ASC_W;
      advance = 1;
    }
    // 两种中断条件：像素溢出，或输出缓冲区溢出。两者都会结束当前行
    // 但不消耗当前输入字形——该字形会放到下一行。
    if (cx + glyph_w > maxPx ||
        (uint16_t)(col_bytes + advance) >= (uint16_t)(CJK_ROW_CAP - 1)) {
      out[row][col_bytes] = 0;
      row++;
      if (row >= maxRows) return row;
      col_bytes = 0;
      cx = 0;
      // 边界情况：单个字形宽度超过整个可用宽度。直接丢弃以避免无限循环。
      // 实际上不太可能（任何合理的 HUD 区域 maxPx ≥ 16），但保留此防护。
      if (cx + glyph_w > maxPx) {
        p += advance;
        continue;
      }
    }
    for (int i = 0; i < advance; i++) {
      out[row][col_bytes++] = (char)p[i];
    }
    cx += glyph_w;
    p += advance;
  }
  if (col_bytes > 0 && row < maxRows) {
    out[row][col_bytes] = 0;
    row++;
  }
  return row;
}

int cjkMeasureMixed(const char* text) {
  if (text == nullptr) return 0;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(text);
  int w = 0;
  while (*p) {
    if (isGbkLead(p)) { w += GBK_W; p += 2; }
    else if (*p == '\n' || *p == '\r') { p++; }
    else { w += ASC_W; p += 1; }
  }
  return w;
}

#endif // CC_BUDDY_CJK_DISPLAY
