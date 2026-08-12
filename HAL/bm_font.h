/**
 * bm_font.h — 裸机 UI 共享字体（8×8 数字 + 5×7 拉丁/符号）
 *
 * 8×8 数字用于时钟/计算结果（整数缩放 scale=5/6/7 → 40/48/56px）；
 * 5×7 拉丁用于标签（scale=1/2 → 7/14px）。位图格式：逐行，MSB 在左。
 */
#ifndef BM_FONT_H
#define BM_FONT_H

#include <stdint.h>

typedef struct {
    const uint8_t *data;  /* 逐行位图，每行 w 像素，MSB 在左 */
    uint8_t w;
    uint8_t h;
} glyph_t;

/** 取字形：0-9 → 8×8；0x20-0x7E → 5×7；其余 → 占位方块 */
glyph_t bm_font_glyph(uint16_t ch);

/** 取窄字形：0-9 强制用 5×7（主页时钟，避免宽点阵侵入状态簇） */
glyph_t bm_font_glyph_narrow(uint16_t ch);

/** UTF-8 逐字解码取字形：ASCII 1 字节 → 5×7/8×8；中文 3 字节 → 16×16 字库 */
glyph_t bm_font_glyph_utf8(const char **sp);

/** 文本像素宽度（含字距 scale） */
uint16_t bm_text_width(const char *s, uint8_t scale);

#endif /* BM_FONT_H */
