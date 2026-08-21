/**
 * bm_font.h �?KLB UI 共享字体（抗锯齿 4-bit coverage，v0.7�? *
 * 字形�?-bit 覆盖率（2px/字节，高 nibble 在前），按角色使用原生分辨率网格�? *   16px base ASCII : Azeret Mono Regular 400（小�?表达式）
 *   12px label ASCII: Azeret Mono Regular 400（标�?芯片/品牌�? *   16px 中文       : Noto Sans CJK SC Regular�?9 字）
 *   32px 时钟数字   : Saira Light 300�?-9 : 空格�? *   40px 结果数字   : Saira Light 300�?-9 , . - 空格�? * stride = (grid+1)/2 字节/行；绘制时按 coverage �?fg/bg 混合（非点阵 1-bit）�? * 生成：tools/gen_klb_font.ps1（GDI+ AntiAlias 4x�?x4 盒式降采样）�? */
#ifndef BM_FONT_H
#define BM_FONT_H

#include <stdint.h>

typedef struct {
    const uint8_t *data;  /* 4-bit coverage, 2px/byte, �?nibble 在前 */
    uint8_t w;            /* 内容�?px */
    uint8_t h;            /* 网格�?px�?6/9/32/40�?*/
    uint8_t ox;           /* 内容左偏�?px */
    uint8_t stride;       /* 每行字节�?= (grid+1)/2 */
} glyph_t;

/** 16px base ASCII�?-9/0x20-0x7E）；其余 �?占位 */
glyph_t bm_font_glyph(uint16_t ch);

/** 9px 标签 ASCII�?-9/0x20-0x7E）；其余回退 16px */
glyph_t bm_font_glyph_micro(uint16_t ch);
glyph_t bm_font_glyph_label(uint16_t ch);

/** UTF-8 逐字解码：ASCII �?16px base；中�?3 字节 �?16px Noto */
glyph_t bm_font_glyph_utf8(const char **sp);
glyph_t bm_font_glyph_label_utf8(const char **sp);

/** 32px 时钟数字/符号�?-9 : 空格）；其余回退 16px base */
glyph_t bm_font_glyph_expr(uint16_t ch);
glyph_t bm_font_glyph_mode(uint16_t ch);
glyph_t bm_font_glyph_clock(uint16_t ch);

/** 40px 结果数字/符号�?-9 , . - 空格）；其余回退 16px base */
glyph_t bm_font_glyph_result(uint16_t ch);
glyph_t bm_font_glyph_date12(uint16_t ch);

/** 16px utf8 文本宽度（含 1px 字距�?*/
uint16_t bm_text_width(const char *s);

/** 9px 标签文本宽度 */
uint16_t bm_text_width_micro(const char *s);
uint16_t bm_text_width_label(const char *s);
uint16_t bm_text_width_label_utf8(const char *s);

/** 32px 时钟文本宽度 */
uint16_t bm_text_width_expr(const char *s);
uint16_t bm_text_width_mode(const char *s);
uint16_t bm_text_width_clock(const char *s);

/** 40px 结果文本宽度 */
uint16_t bm_text_width_result(const char *s);
uint16_t bm_text_width_date12(const char *s);

#endif /* BM_FONT_H */
