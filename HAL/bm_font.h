/**
 * bm_font.h — 裸机 UI 共享字体（凤凰点阵体 16px 位图）
 *
 * 全部字形 16×16（每字 32 字节，16 行 × 2 字节），整数缩放 scale；
 * 中文 fix=1 固定 1:1（16px），拉丁/数字按 scale 放大。位图格式：逐行，MSB 在左。
 */
#ifndef BM_FONT_H
#define BM_FONT_H

#include <stdint.h>

typedef struct {
    const uint8_t *data;  /* 16×16 位图（32B），MSB 在左 */
    uint8_t w;            /* 内容宽（ASCII 按边界裁剪，中文 16） */
    uint8_t h;            /* 高度（16） */
    uint8_t ox;           /* 内容在 16×16 网格中的左偏移 */
    uint8_t fix;          /* 1 = 固定 1:1（中文 16×16），忽略 scale 放大 */
} glyph_t;

/** 取字形：0-9/0x20-0x7E → 16×16 凤凰点阵体；其余 → 占位方块 */
glyph_t bm_font_glyph(uint16_t ch);

/** 取窄字形：0-9 用 16×16 凤凰数字（主页时钟） */
glyph_t bm_font_glyph_narrow(uint16_t ch);

/** UTF-8 逐字解码取字形：ASCII → 16×16；中文 3 字节 → 16×16 字库（fix=1） */
glyph_t bm_font_glyph_utf8(const char **sp);

/** 文本像素宽度（含字距 scale） */
uint16_t bm_text_width(const char *s, uint8_t scale);

#endif /* BM_FONT_H */
