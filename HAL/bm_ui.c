/**
 * bm_ui.c �?裸机 UI（无 LVGL�?
 *
 * 设计规范：C:\ClaudeProject\tft_NV3007\brand-spec.md
 *   - 六主题（像素/极简/黑客 × 双色），设置页循�?
 *   - 三页面（主页 / 计算�?/ 设置），底部三点导航
 *   - 428×142 横屏，无全帧缓冲，直接窗口直�?
 * 驱动：复�?HAL/NV3007.c（API 统一为 NV3007_*，逻辑横屏 428×142，行=物理列转置）�?
 * 公共 API �?LVGL �?numpad_ui.h 同名，按键路由零改动�?
 */
#ifndef BM_SIM
#include "config.h"
#include "CH58x_timer.h"
#include "CH58x_clk.h"
#include "ISP583.h"   /* EEPROM_READ/WRITE for custom text (UI_TEXT_ADDR) */
#endif
#include "NV3007.h"
#include "bm_ui.h"
#include "bm_font.h"
#include <string.h>
#ifdef BM_SIM
#include <time.h>
#include <stdlib.h>
void DelayMs(uint16_t ms);          /* provided by the SDL simulator backend */
#endif

#if UI_BM_EN

/* B0.8 debug: 0 = normal UI; 1 = solid RED direct flush (bypasses dirty
 * bitmap / content renderers); 2 = boot sequence of diagnostic frames. */
#define BM_UI_BG_ONLY 0

/* 逻辑横屏尺寸（对�?HAL/NV3007.h�?*/
#define TFT_W NV3007_WIDTH
#define TFT_H NV3007_HEIGHT

/* ══════════════�?1ms tick（TMR0；LVGL 停用时归本模块） ══════════════�?*/
volatile uint32_t g_bm_tick_ms;

#ifndef BM_SIM
__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
    if (TMR0_GetITFlag(TMR0_3_IT_CYC_END)) {
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
        g_bm_tick_ms++;
    }
}
#endif

/* ══════════════�?颜色 / 六主题调色板 ══════════════�?*/
typedef struct { uint8_t r, g, b; } scolor;
typedef struct {
    scolor bg, card, border, fg, muted, pressed, active, active_fg, soft;
} theme_palette;

typedef enum {
    THEME_KLB = 0,        /* 深色（默认，克莱因蓝） */
    THEME_KLB_LIGHT,      /* 浅色（冰白） */
    THEME_COUNT
} bm_theme_t;

/* 色值换算见 Reference/vial-pad-klb-ui.md 附录 A（sRGB，绘制时 bm_565 转 RGB565）。
 * border：深色=fg 26% over card；浅色=mid-blue 50% over bg。
 * pressed：深色=card +6% L；浅色=card -4% L。soft：active 12% 叠加。 */
static const theme_palette PALETTES[THEME_COUNT] = {
    /* 克莱因蓝·深（默认） */
    { {0x0A,0x12,0x35}, {0x24,0x32,0x6B}, {0x5A,0x65,0x90}, {0xF2,0xF5,0xFB},
      {0x96,0xAC,0xD1}, {0x30,0x3E,0x7A}, {0x99,0xD7,0xFF}, {0x07,0x0F,0x2C}, {0x32,0x46,0x7D} },
    /* 克莱因蓝·浅 */
    { {0xF0,0xF4,0xF9}, {0xFC,0xFE,0xFF}, {0xA7,0xB2,0xD0}, {0x1A,0x26,0x4B},
      {0x5D,0x6F,0xA6}, {0xF2,0xF4,0xF5}, {0x00,0x2F,0xA7}, {0xEE,0xF2,0xF7}, {0xDE,0xE5,0xF4} },
};

/* ══════════════�?16×16 图标位图 ══════════════�?*/
static const uint8_t bm_icon_bt[32] = {
    0x01,0x80,
    0x01,0xC0,
    0x01,0xE0,
    0x01,0xF0,
    0x0D,0xF0,
    0x0F,0xF0,
    0x07,0xE0,
    0x03,0xC0,
    0x03,0xC0,
    0x07,0xE0,
    0x0F,0xF0,
    0x0D,0xF0,
    0x01,0xF0,
    0x01,0xE0,
    0x01,0xC0,
    0x01,0x80,
};
static const uint8_t bm_icon_sun[32] = {
    0x00,0x00,
    0x01,0x80,
    0x01,0x80,
    0x11,0x88,
    0x09,0x90,
    0x03,0xC0,
    0x06,0x60,
    0x7C,0x3E,
    0x7C,0x3E,
    0x06,0x60,
    0x03,0xC0,
    0x09,0x90,
    0x11,0x88,
    0x01,0x80,
    0x01,0x80,
    0x00,0x00,
};
static const uint8_t bm_icon_clock[32] = {
    0x00,0x00,
    0x03,0xC0,
    0x0F,0xF0,
    0x18,0x18,
    0x31,0x8C,
    0x21,0x84,
    0x61,0x86,
    0x61,0x86,
    0x60,0xC6,
    0x60,0x66,
    0x20,0x04,
    0x30,0x0C,
    0x18,0x18,
    0x0F,0xF0,
    0x03,0xC0,
    0x00,0x00,
};
static const uint8_t bm_icon_half[32] = {
    0x00,0x00,
    0x03,0xC0,
    0x0F,0xF0,
    0x1B,0x98,
    0x37,0x8C,
    0x2F,0x84,
    0x7F,0x86,
    0x7F,0x86,
    0x7F,0x86,
    0x7F,0x8E,
    0x3F,0x9C,
    0x3F,0xBC,
    0x1F,0xF8,
    0x0F,0xF0,
    0x03,0xC0,
    0x00,0x00,
};
static const uint8_t bm_icon_reset[32] = {
    0x00,0x00,
    0x00,0x00,
    0x01,0xCC,
    0x0F,0xFC,
    0x1C,0x3C,
    0x18,0x38,
    0x30,0x08,
    0x30,0x0C,
    0x30,0x0C,
    0x10,0x0C,
    0x18,0x18,
    0x1C,0x38,
    0x0F,0xF0,
    0x03,0x80,
    0x00,0x00,
    0x00,0x00,
};


/* ══════════════�?UI 状�?══════════════�?*/
typedef struct {
    uint8_t  page;         /* UI_PAGE_* */
    uint8_t  theme;        /* bm_theme_t */
    uint8_t  mode;         /* UI_MODE_* */
    uint8_t  battery;      /* 0..100 */
    uint8_t  brightness;   /* 20/40/60/80/100 */
    uint8_t  sleep_index;  /* 0..3 -> 10s/30s/60s/永不 */
    uint8_t  sett_sub;     /* 0=SETT-01 磁贴页, 1=SETT-02 主题选择 */
    uint8_t  ready;
    uint8_t  dirty;        /* 按键/状态变更后置位，主循环整页重绘 */
    uint8_t  partial;      /* 局部刷新请求（UI_PART_*），主循环优先处理 */
    uint32_t reset_t;      /* "已重�? 反馈时间�?*/
    struct { char expr[32]; double result; uint8_t finalized;
             struct { char expr[20]; char res[16]; } hist[3]; uint8_t hist_n; } calc;
    struct { uint8_t hh, mm; uint16_t year; uint8_t mon, day; } clock;
} bm_ui_t;

static bm_ui_t g_ui;
static const uint8_t g_sleep_opts[4] = { 10, 30, 60, 0 };

/* Custom display text (set via raw HID 0xE2, persisted at UI_TEXT_ADDR). */
#define UI_TEXT_ADDR   0x3F10
#define UI_TEXT_MAX    15
static char bm_custom_text[UI_TEXT_MAX + 1];
static uint8_t bm_custom_loaded;

static const theme_palette *bm_pal(void);

/* ══════════════�?渲染原语（行缓冲合成 + 整行直写�?══════════════�?
 * 每页先合成为 428×2B 行缓冲，结束时按“自检纯色同款”的整列窗口写面�?
 * （每逻辑行一�?SetWindow + 428 像素），避免成百上千次小窗口 RAMWR
 * �?NV3007 上引起条�?亮度不均，同时大幅提升刷新速度�?*/
static uint16_t g_line[TFT_W];               /* 428×2B 行缓�?*/
static uint8_t  g_dirty[(TFT_H + 7) / 8];    /* 脏行位图 18B */

/* RGB565 ??:5-6-5 ????(vial-pad-klb-ui.md ?? A ??) */
static inline uint16_t bm_565(scolor c)
{
    uint8_t r5 = (uint8_t)(((uint16_t)c.r * 31u + 127u) / 255u);
    uint8_t g6 = (uint8_t)(((uint16_t)c.g * 63u + 127u) / 255u);
    uint8_t b5 = (uint8_t)(((uint16_t)c.b * 31u + 127u) / 255u);
    return (uint16_t)(((uint16_t)r5 << 11) | ((uint16_t)g6 << 5) | b5);
}

static void bm_dirty_row(uint16_t y)
{
    if (y < TFT_H) g_dirty[y >> 3] |= (uint8_t)(1u << (y & 7));
}

static void bm_page_begin(void)
{
    memset(g_dirty, 0, sizeof(g_dirty));
}

static void bm_flush_row(uint16_t y)
{
    if (y < TFT_H && (g_dirty[y >> 3] & (1u << (y & 7)))) {
        g_dirty[y >> 3] &= (uint8_t)~(1u << (y & 7));
        NV3007_FlushRow(y, g_line);
    }
}

static void bm_page_end(void)
{
    uint16_t y;
    for (y = 0; y < TFT_H; y++) bm_flush_row(y);
}

static void bm_pix(uint16_t x, uint16_t y, uint16_t c565)
{
    if (x < TFT_W && y < TFT_H) {
        g_line[x] = c565;
        bm_dirty_row(y);
    }
}

static void bm_hspan(uint16_t x0, uint16_t x1, uint16_t y, uint16_t c565)
{
    uint16_t i;
    if (y >= TFT_H) return;
    if (x0 > x1) { uint16_t t = x0; x0 = x1; x1 = t; }
    if (x1 >= TFT_W) x1 = (uint16_t)(TFT_W - 1);
    for (i = x0; i <= x1; i++) g_line[i] = c565;
    bm_dirty_row(y);
}

static void bm_fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, scolor c)
{
    uint16_t t;
    uint16_t y;
    uint16_t c565 = bm_565(c);
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    if (y1 >= TFT_H) y1 = (uint16_t)(TFT_H - 1);
    for (y = y0; y <= y1; y++) bm_hspan(x0, x1, y, c565);
}

static void bm_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t th, scolor c)
{
    bm_fill(x0, y0, x1, (uint16_t)(y0 + th - 1), c);
    bm_fill(x0, (uint16_t)(y1 - th + 1), x1, y1, c);
    bm_fill(x0, (uint16_t)(y0 + th), (uint16_t)(x0 + th - 1), (uint16_t)(y1 - th), c);
    bm_fill((uint16_t)(x1 - th + 1), (uint16_t)(y0 + th), x1, (uint16_t)(y1 - th), c);
}

static void bm_glyph(uint16_t x, uint16_t y, glyph_t g, uint8_t scale, uint16_t c565)
{
    uint8_t row, col;
    uint8_t bytes_per_row = (g.w == 16) ? 2 : 1;
    for (row = 0; row < g.h; row++) {
        for (col = 0; col < g.w; col++) {
            uint8_t b = g.data[row * bytes_per_row + (col >> 3)];
            if (b & (0x80 >> (col & 7))) {
                uint16_t px  = (uint16_t)(x + (uint16_t)col * scale);
                uint16_t py0 = (uint16_t)(y + (uint16_t)row * scale);
                uint16_t r;
                for (r = 0; r < scale; r++)
                    bm_hspan(px, (uint16_t)(px + scale - 1), (uint16_t)(py0 + r), c565);
            }
        }
    }
}

static void bm_text(uint16_t x, uint16_t y, const char *s, uint8_t scale, scolor c)
{
    uint16_t c565 = bm_565(c);
    while (*s) {
        glyph_t g = bm_font_glyph((uint8_t)*s++);
        bm_glyph(x, y, g, scale, c565);
        x += (uint16_t)g.w * scale + scale;
    }
}


static void bm_icon16(uint16_t x, uint16_t y, const uint8_t *data, scolor c)
{
    uint16_t c565 = bm_565(c);
    uint8_t row, col;
    for (row = 0; row < 16; row++) {
        uint16_t bits = (uint16_t)(((uint16_t)data[row * 2] << 8) | data[row * 2 + 1]);
        for (col = 0; col < 16; col++) {
            if (bits & (0x8000 >> col))
                bm_pix((uint16_t)(x + col), (uint16_t)(y + row), c565);
        }
    }
}

/* 电池图标（矩形绘制，填充随电量） */
static void bm_battery(uint16_t x, uint16_t y, uint8_t pct, scolor outline, scolor fill)
{
    uint16_t fw;
    bm_fill(x, y, (uint16_t)(x + 12), (uint16_t)(y + 9), outline);       /* 机身 */
    bm_fill((uint16_t)(x + 1), (uint16_t)(y + 1), (uint16_t)(x + 11), (uint16_t)(y + 8), bm_pal()->bg);
    bm_fill((uint16_t)(x + 13), (uint16_t)(y + 3), (uint16_t)(x + 14), (uint16_t)(y + 6), outline); /* 端子 */
    if (pct > 100) pct = 100;
    fw = (uint16_t)(10u * pct / 100u);
    if (fw > 0)
        bm_fill((uint16_t)(x + 1), (uint16_t)(y + 1), (uint16_t)(x + fw), (uint16_t)(y + 8), fill);
}

static const theme_palette *bm_pal(void) { return &PALETTES[g_ui.theme]; }

static uint8_t bm_is_pixel(void)
{
    return 0;   /* KLB 主题无点阵底纹 */
}

static uint8_t bm_is_hack(void)
{
    return 0;   /* KLB 主题无黑客特化 */
}

static uint8_t bm_is_min(void)
{
    return 0;   /* KLB 主题无极简特化 */
}

/* 时钟字号�?×7 窄点阵）：像�?49px→scale7，极简/黑客 35px→scale5 */
static uint8_t bm_clock_scale(void) { return bm_is_pixel() ? 3 : 2; }
static uint16_t bm_darker565(uint16_t c565, uint8_t f)
{
    uint8_t r5 = (uint8_t)((c565 >> 11) & 0x1Fu);
    uint8_t g6 = (uint8_t)((c565 >> 5) & 0x3Fu);
    uint8_t b5 = (uint8_t)(c565 & 0x1Fu);
    r5 = (uint8_t)((r5 * f) >> 4);
    g6 = (uint8_t)((g6 * f) >> 4);
    b5 = (uint8_t)((b5 * f) >> 4);
    return (uint16_t)(((uint16_t)r5 << 11) | ((uint16_t)g6 << 5) | b5);
}

/* Pixel themes get the pager LCD dot-matrix background (reference
 * numpad-ui-pager.html .screen::after, 3px grid). */
/* Pixel/hack themes get the LCD dot-matrix overlay (numpad-ui-pager.html
 * .screen::after, z-index 5 covers all content; minimal themes disable it). */
static void bm_fill_page_bg(scolor bg)
{
    uint16_t bg565 = bm_565(bg);
    if (bm_is_pixel())
        NV3007_FillDots(bg565, bm_darker565(bg565, 15), 3);
    else
        NV3007_Fill(bg565);
}

/* 像素主题：逻辑坐标 (x,y) 是否点阵深色点（step=3 规则） */
static uint8_t bm_px_dot(uint16_t x, uint16_t y)
{
    return (uint8_t)((y % 3u) == 1u && (x % 3u) == 1u);
}

/* 直接窗口填充：像素主题恢复点阵底纹，其余主题纯色 */
static void bm_fill_bg_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t bg565)
{
    uint16_t x, y, c, x1 = (uint16_t)(x0 + w - 1u), y1 = (uint16_t)(y0 + h - 1u);
    if (!bm_is_pixel()) { NV3007_FillRect(x0, y0, w, h, bg565); return; }
    uint16_t dot565 = bm_darker565(bg565, 15);
    NV3007_SetWindow((uint16_t)(NV3007_VIS_X1 - y1), x0,
                    (uint16_t)(NV3007_VIS_X1 - y0), x1);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            c = bm_px_dot(x, y) ? dot565 : bg565;
            NV3007_WritePixel(c);
        }
}

static uint16_t bm_date_y(void) { return (uint16_t)(TFT_H - (bm_is_pixel() ? 28u : 30u)); }

/* ══════════════�?数字格式化（避免 printf 浮点依赖�?══════════════�?*/
static char *bm_u64_str(char *p, unsigned long long v)
{
    char tmp[24];
    int n = 0;
    do { tmp[n++] = (char)('0' + (int)(v % 10)); v /= 10; } while (v);
    while (n) *p++ = tmp[--n];
    return p;
}

/* 极简主题电池色（brand-spec�?=20% �?/ <20% 红） */
#define BM_BAT_OK565   0x2D08   /* RGB(46,160,68)  */
#define BM_BAT_LOW565  0xD228   /* RGB(214,69,69)  */

/* 蔡勒公式�?=周日 .. 6=周六 */
static uint8_t bm_weekday(void)
{
    int y = g_ui.clock.year;
    int m = g_ui.clock.mon;
    int d = g_ui.clock.day;
    if (m < 3) { m += 12; y--; }
    return (uint8_t)((d + 13 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7);
}

static void bm_fmt_clock(char *o)
{
    o[0] = (char)('0' + g_ui.clock.hh / 10); o[1] = (char)('0' + g_ui.clock.hh % 10);
    o[2] = ':';
    o[3] = (char)('0' + g_ui.clock.mm / 10); o[4] = (char)('0' + g_ui.clock.mm % 10);
    o[5] = 0;
}

static void bm_fmt_date(char *o)
{
    static const char *wd[7] = { "周日", "周一", "周二", "周三", "周四", "周五", "周六" };
    char *p = o;
    *p++ = (char)('0' + g_ui.clock.mon / 10); *p++ = (char)('0' + g_ui.clock.mon % 10);
    *p++ = '.'; *p++ = (char)('0' + g_ui.clock.day / 10); *p++ = (char)('0' + g_ui.clock.day % 10);
    *p++ = ' ';
    strcpy(p, wd[bm_weekday()]);
}

/* 超长结果转指数：1.23456e+12（brand-spec�?*/
static void bm_fmt_sci(char *out, size_t n, double v)
{
    int e10 = 0, neg = (v < 0);
    double m = neg ? -v : v;
    unsigned long long ip, f6;
    char *p = out;
    (void)n;
    if (m > 0) {
        while (m >= 10) { m /= 10; e10++; }
        while (m < 1)   { m *= 10; e10--; }
    }
    ip = (unsigned long long)m;
    f6 = (unsigned long long)((m - ip) * 1e6 + 0.5);
    while (f6 && (f6 % 10) == 0) f6 /= 10;
    if (neg) *p++ = '-';
    p = bm_u64_str(p, ip);
    if (f6) {
        *p++ = '.';
        p = bm_u64_str(p, f6);
    }
    *p++ = 'e';
    if (e10 < 0) { *p++ = '-'; e10 = -e10; } else { *p++ = '+'; }
    p = bm_u64_str(p, (unsigned long long)e10);
    *p = 0;
}

static void bm_fmt_result(char *out, size_t n, double v)
{
    char *p = out;
    unsigned long long ip, f6;
    double frac;
    int neg;
    (void)n;
    if (v != v) {
        strcpy(out, "Err");
        return;
    }
    if (v > 9e11 || v < -9e11) {
        bm_fmt_sci(out, n, v);
        return;
    }
    neg = (v < 0);
    if (neg) v = -v;
    ip = (unsigned long long)v;
    frac = v - (double)ip;
    if (frac >= 0.0000005) {
        f6 = (unsigned long long)(frac * 1e6 + 0.5);
        while (f6 && (f6 % 10) == 0) f6 /= 10;
        if (neg) *p++ = '-';
        p = bm_u64_str(p, ip);
        *p++ = '.';
        p = bm_u64_str(p, f6);
        *p = 0;
    } else {
        if (neg) *p++ = '-';
        p = bm_u64_str(p, ip);
        *p = 0;
    }
}

/* ══════════════�?三页�?══════════════�?*/
static void bm_draw_nav_dots(void)
{
    const theme_palette *p = bm_pal();
    /* Pixel family: 8px hollow squares (2px border); others keep 6px. */
    uint16_t d = bm_is_pixel() ? 8 : 6, gap = 10;
    uint16_t total = (uint16_t)(3 * d + 2 * gap);
    uint16_t x = (uint16_t)((TFT_W - total) / 2);
    uint16_t y = (uint16_t)(TFT_H - 10);
    int i;
    for (i = 0; i < 3; i++) {
        uint16_t dx = (uint16_t)(x + (uint16_t)i * (d + gap));
        if ((int)g_ui.page == i)
            bm_fill(dx, y, (uint16_t)(dx + d - 1), (uint16_t)(y + d - 1), p->active);
        else
            bm_rect(dx, y, (uint16_t)(dx + d - 1), (uint16_t)(y + d - 1), 2, p->border);
    }
}

/* ---- 抗锯齿字形渲染（4-bit coverage，矢量感非点阵） ----
 * 字形按原生网格大小 1:1 写入（12px 标签 / 16px 数值 / 32px 时钟 / 40px 结果），
 * 每字形一个窗口，coverage 与 fg/bg 按 5-6-5 混合，透明像素写 bg 保持窗口流。 */

static uint16_t bm_blend565(uint16_t fg, uint16_t bg, uint8_t cov)
{
    uint16_t r, g, b;
    if (cov == 0) return bg;
    if (cov >= 15) return fg;
    r = (uint16_t)((((fg >> 11) & 0x1F) * cov + ((bg >> 11) & 0x1F) * (15 - cov) + 7) / 15);
    g = (uint16_t)((((fg >> 5) & 0x3F) * cov + ((bg >> 5) & 0x3F) * (15 - cov) + 7) / 15);
    b = (uint16_t)(((fg & 0x1F) * cov + ((bg & 0x1F) * (15 - cov)) + 7) / 15);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void bm_glyph_blit(uint16_t x, uint16_t y, glyph_t g, uint16_t fg565, uint16_t bg565)
{
    uint16_t row, col;
    uint16_t col0 = (uint16_t)(NV3007_VIS_X1 - (y + g.h - 1));
    uint16_t col1 = (uint16_t)(NV3007_VIS_X1 - y);
    NV3007_SetWindow(col0, x, col1, (uint16_t)(x + g.w - 1));
    /* pixel stream: column-first (bottom-to-top per logical column)
     * matching NV3007 RAMWR auto-increment (CASET outer then RASET inner). */
    for (col = 0; col < g.w; col++) {
        uint8_t gc = (uint8_t)(col + g.ox);  /* actual column in glyph grid */
        for (row = 0; row < g.h; row++) {
#if NV3007_TEXT_FLIP
            uint16_t rr = (uint16_t)((g.h - 1) - row);
#else
            uint16_t rr = row;
#endif
            uint8_t cov = (uint8_t)((g.data[rr * g.stride + (gc >> 1)] >> ((gc & 1) ? 0 : 4)) & 0x0F);
            NV3007_WritePixel(bm_blend565(fg565, bg565, cov));
        }
    }
}

static uint8_t bm_str_has_utf8(const char *s)
{
    while (*s) if ((uint8_t)*s++ >= 0x80) return 1;
    return 0;
}

static void bm_text_loop(uint16_t x, uint16_t y, const char *s, uint16_t fg565, uint16_t bg565,
                         glyph_t (*get)(uint16_t))
{
    while (*s) {
        glyph_t g = get((uint8_t)*s++);
        bm_glyph_blit(x, y, g, fg565, bg565);
        x = (uint16_t)(x + g.w + 1);
    }
}

static void bm_text_loop_utf8(uint16_t x, uint16_t y, const char *s, uint16_t fg565, uint16_t bg565)
{
    while (*s) {
        glyph_t g = bm_font_glyph_utf8(&s);
        bm_glyph_blit(x, y, g, fg565, bg565);
        x = (uint16_t)(x + g.w + 1);
    }
}

static void bm_text_loop_utf8_label(uint16_t x, uint16_t y, const char *s, uint16_t fg565, uint16_t bg565)
{
    while (*s) {
        glyph_t g = bm_font_glyph_label_utf8(&s);
        bm_glyph_blit(x, y, g, fg565, bg565);
        x = (uint16_t)(x + g.w + 1);
    }
}

/* 标签/品牌/芯片：纯 ASCII → 9px；含中文 → 16px */
static void bm_text_direct(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    if (bm_str_has_utf8(s)) bm_text_loop_utf8_label(x, y, s, fg565, bg565);
    else bm_text_loop(x, y, s, fg565, bg565, bm_font_glyph_label);
}

/* 小值/表达式：16px（含中文自动走 utf8） */
static void bm_text_direct_16(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    if (bm_str_has_utf8(s)) bm_text_loop_utf8(x, y, s, fg565, bg565);
    else bm_text_loop(x, y, s, fg565, bg565, bm_font_glyph);
}

/* micro�?8px 品牌/芯片/页码 */
static void bm_text_direct_micro(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    if (bm_str_has_utf8(s)) bm_text_loop_utf8_label(x, y, s, fg565, bg565);
    else bm_text_loop(x, y, s, fg565, bg565, bm_font_glyph_micro);
}

/* 表达式�?13px */
static void bm_text_direct_expr(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    bm_text_loop(x, y, s, bm_565(fg), bm_565(bg), bm_font_glyph_expr);
}

/* 模式值�?22px */
static void bm_text_direct_mode(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    bm_text_loop(x, y, s, bm_565(fg), bm_565(bg), bm_font_glyph_mode);
}

/* 运算符号显示映射：* → ×（U+00D7）、/ → ÷（U+00F7），符合设计规范示例 */
static void bm_ops_display(char *dst, const char *src)
{
    while (*src) {
        if (*src == '*')      *dst++ = (char)0xD7;
        else if (*src == '/') *dst++ = (char)0xF7;
        else                  *dst++ = *src;
        src++;
    }
    *dst = 0;
}

/* 右对齐（22px mode 表达式） */
static void bm_text_direct_right_mode(uint16_t xr, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t w = bm_text_width_mode(s);
    uint16_t x = (w < xr + 1) ? (uint16_t)(xr - w + 1) : 0;
    bm_text_direct_mode(x, y, s, fg, bg);
}

/* 时钟：32px 显示数字 */
static void bm_text_direct_clock(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    bm_text_loop(x, y, s, bm_565(fg), bm_565(bg), bm_font_glyph_clock);
}

/* 结果：40px 显示数字 */
static void bm_text_direct_result(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    bm_text_loop(x, y, s, bm_565(fg), bm_565(bg), bm_font_glyph_result);
}

/* 12px 日期数字 */
static void bm_text_direct_date12(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    bm_text_loop(x, y, s, bm_565(fg), bm_565(bg), bm_font_glyph_date12);
}

/* 日期 "08.21 周六"：数字 12px + 中文 12px 对齐 */
static void bm_draw_date(uint16_t x, uint16_t y, const char *s, scolor fg, scolor bg)
{
    char dnum[8], dzh[8];
    strncpy(dnum, s, 5); dnum[5] = 0;      /* "08.21" */
    strcpy(dzh, s + 6);                     /* "周六" */
    bm_text_direct_date12(x, y, dnum, fg, bg);
    bm_text_direct((uint16_t)(x + bm_text_width_date12(dnum) + 2), y, dzh, fg, bg);
}

/* 右对齐（16px / 时钟 / 结果） */
static void bm_text_direct_right(uint16_t xr, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t w = bm_text_width(s);
    uint16_t x = (w < xr + 1) ? (uint16_t)(xr - w + 1) : 0;
    bm_text_direct_16(x, y, s, fg, bg);
}
static void bm_text_direct_right_clock(uint16_t xr, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t w = bm_text_width_clock(s);
    uint16_t x = (w < xr + 1) ? (uint16_t)(xr - w + 1) : 0;
    bm_text_direct_clock(x, y, s, fg, bg);
}
static void bm_text_direct_right_result(uint16_t xr, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t w = bm_text_width_result(s);
    uint16_t x = (w < xr + 1) ? (uint16_t)(xr - w + 1) : 0;
    bm_text_direct_result(x, y, s, fg, bg);
}
static void bm_text_direct_right_expr(uint16_t xr, uint16_t y, const char *s, scolor fg, scolor bg)
{
    uint16_t w = bm_text_width_expr(s);
    uint16_t x = (w < xr + 1) ? (uint16_t)(xr - w + 1) : 0;
    bm_text_direct_expr(x, y, s, fg, bg);
}

static void bm_icon16_direct(uint16_t x, uint16_t y, const uint8_t *data,
                             scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    uint8_t row, col;
    uint16_t col0 = (uint16_t)(NV3007_VIS_X1 - (y + 15));
    uint16_t col1 = (uint16_t)(NV3007_VIS_X1 - y);
    NV3007_SetWindow(col0, x, col1, (uint16_t)(x + 15));
    for (col = 0; col < 16; col++) {
        for (row = 0; row < 16; row++) {
            /* 物理列从 col0（逻辑 y 底部）递增，先写的字形行落在底部，
             * 所以按 15-row 反转，保证字形行 0（顶部）显示在顶部�?*/
#if NV3007_TEXT_FLIP
            uint8_t r2 = (uint8_t)(15u - row);
#else
            /* 物理列从 col0（逻辑 y 底部）递增，先写的字形行落在底部，
             * 所以按 15-row 反转，保证字形行 0（顶部）显示在顶部�?*/
            uint8_t r2 = row;
#endif
            uint16_t bits = (uint16_t)(((uint16_t)data[r2 * 2] << 8) | data[r2 * 2 + 1]);
            NV3007_WritePixel((bits & (0x8000 >> col)) ? fg565 : bg565);
        }
    }
}


/* 设置页图标 14x14：取 16x16 位图中心 14 行/列（参考规范 setting-icon 14px，黑客 13px 用同一 14 绘制） */
static void bm_icon14_direct(uint16_t x, uint16_t y, const uint8_t *data,
                             scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    uint8_t row, col;
    uint16_t col0 = (uint16_t)(NV3007_VIS_X1 - (y + 13));
    uint16_t col1 = (uint16_t)(NV3007_VIS_X1 - y);
    NV3007_SetWindow(col0, x, col1, (uint16_t)(x + 13));
    for (col = 0; col < 14; col++) {
        for (row = 0; row < 14; row++) {
#if NV3007_TEXT_FLIP
            uint8_t r2 = (uint8_t)(14u - row);   /* 中心底行 14 -> 逻辑底部 */
#else
            uint8_t r2 = (uint8_t)(1u + row);
#endif
            uint16_t bits = (uint16_t)(((uint16_t)data[r2 * 2] << 8) | data[r2 * 2 + 1]);
            NV3007_WritePixel((bits & (0x8000 >> (col + 1))) ? fg565 : bg565);
        }
    }
}

/* ---- Direct shape helpers (no g_line) ---- */
static void bm_rect_direct(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           uint16_t th, uint16_t c565)
{
    uint16_t w = (uint16_t)(x1 - x0 + 1);
    uint16_t h = (uint16_t)(y1 - y0 + 1);
    uint16_t mid = (h >= 2 * th) ? (uint16_t)(h - 2 * th) : 0;
    NV3007_FillRect(x0, y0, w, th, c565);
    NV3007_FillRect(x0, (uint16_t)(y1 - th + 1), w, th, c565);
    NV3007_FillRect(x0, (uint16_t)(y0 + th), 1, mid, c565);
    NV3007_FillRect(x1, (uint16_t)(y0 + th), 1, mid, c565);
}

/* 圆角近似：用背景色把四角切成 r x r 阶梯三角�? * 极简 r=3（≈8px 圆角）、黑�?r=2（≈6px 圆角）、像素不调用�?*/
static void bm_round_corners_direct(uint16_t x0, uint16_t y0, uint16_t x1,
                                    uint16_t y1, uint16_t r, uint16_t bg565)
{
    uint16_t i;
    if (r == 0) return;
    for (i = 0; i < r; i++) {
        uint16_t k = (uint16_t)(r - i);
        NV3007_FillRect((uint16_t)(x0 + i), y0, 1, k, bg565);
        NV3007_FillRect((uint16_t)(x1 - i), y0, 1, k, bg565);
        NV3007_FillRect((uint16_t)(x0 + i), (uint16_t)(y1 - k + 1), 1, k, bg565);
        NV3007_FillRect((uint16_t)(x1 - i), (uint16_t)(y1 - k + 1), 1, k, bg565);
    }
}

static void bm_battery_direct(uint16_t x, uint16_t y, uint8_t pct,
                              uint16_t outline, uint16_t fillc, uint16_t bgc)
{
    /* 14x14 图标（brand-spec / pager HTML：机�?12x6 + 端子 1x2�?*/
    /* pager HTML: rect 1.5,5.5 11x5, terminal 1x1 at x14, fill 2.6,7 8.3x2 */
    NV3007_FillRect(x, (uint16_t)(y + 4), 11, 5, outline);
    NV3007_FillRect((uint16_t)(x + 1), (uint16_t)(y + 5), 9, 3, bgc);
    NV3007_FillRect((uint16_t)(x + 13), (uint16_t)(y + 6), 1, 1, outline);
    if (pct > 100) pct = 100;
    {
        uint16_t fw = (uint16_t)(8u * pct / 100u);
        if (fw > 0) NV3007_FillRect((uint16_t)(x + 1), (uint16_t)(y + 6), fw, 2, fillc);
    }

}

/* ══════════════ 页面绘制（克兰因蓝 KLB 磁贴，vial-pad-klb-ui.md） ══════════════
 * 框架：外边距 12/14；顶部状态条 18px（y=12..30）；主区 y=30..130。
 * 磁贴：直角、1px 细框、8px 间隙；激活 = 反相（active 底 + active_fg 字）。
 * 字阶：标签 12px / 小值 16px / 时钟 32px / 结果 40px（抗锯齿 coverage）。 */

static const char *bm_mode_name(void)
{
    static const char *m[UI_MODE_COUNT] = { "USB", "BT", "RF" };
    return (g_ui.mode < UI_MODE_COUNT) ? m[g_ui.mode] : "?";
}

/* 顶部状态条：左品牌；右=状态点+连接名（主页）或芯片（计算器/设置） */
static void bm_draw_head(const char *brand, const char *chip, const char *pageno)
{
    const theme_palette *p = bm_pal();
    uint16_t dotx = (uint16_t)(TFT_W - 19);   /* 414-5 */
    uint16_t tx, cw;

    bm_text_direct_micro(14, 13, brand, p->muted, p->bg);
    if (pageno) {
        /* 右上页码（如设置页 "1 / 2"） */
        tx = bm_text_width_micro(pageno);
        tx = (uint16_t)(dotx - 6 - tx + 1);
        bm_text_direct_micro(tx, 13, pageno, p->muted, p->bg);
    } else if (chip) {
        cw = (uint16_t)((bm_str_has_utf8(chip) ? bm_text_width_label_utf8(chip) : bm_text_width_micro(chip)) + 14);
        tx = (uint16_t)(TFT_W - 14 - cw);
        NV3007_FillRect(tx, 13, cw, 12, bm_565(p->card));
        bm_rect_direct(tx, 13, (uint16_t)(tx + cw - 1), 24, 1, bm_565(p->border));
        bm_text_direct_micro((uint16_t)(tx + 7), 14, chip, p->muted, p->card);
    } else {
        tx = bm_text_width_micro(bm_mode_name());
        tx = (uint16_t)(dotx - 6 - tx + 1);
        bm_text_direct_micro(tx, 13, bm_mode_name(), p->muted, p->bg);
        NV3007_FillRect(dotx, 18, 5, 5, bm_565(p->active));
    }
}

/* 底部三点页面指示（4×4，当前页反相） */
static void bm_draw_page_dots(void)
{
    const theme_palette *p = bm_pal();
    uint16_t d = 4, gap = 8, total = (uint16_t)(3 * d + 2 * gap);
    uint16_t x = (uint16_t)((TFT_W - total) / 2);
    uint16_t y = (uint16_t)(TFT_H - 8);
    uint8_t i;
    for (i = 0; i < 3; i++) {
        uint16_t dx = (uint16_t)(x + (uint16_t)i * (d + gap));
        NV3007_FillRect(dx, y, d, d, (i == g_ui.page) ? bm_565(p->active) : bm_565(p->border));
    }
}

/* 磁贴：直角 + 1px 细框；激活 = active 底（无反框） */
static void bm_tile(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t active)
{
    const theme_palette *p = bm_pal();
    NV3007_FillRect(x, y, w, h, active ? bm_565(p->active) : bm_565(p->card));
    if (!active)
        bm_rect_direct(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1), 1, bm_565(p->border));
}

/* ---------- HOME-02 主次分栏（时间 136×100 / 电量+连接 124×46 / 模式 124×100 反相） ---------- */
static void bm_draw_home_shapes(void)
{
    const theme_palette *p = bm_pal();
    bm_fill_page_bg(p->bg);
    bm_draw_head("FinPad", NULL, NULL);
    bm_tile(14, 30, 136, 100, 0);    /* 时间 */
    bm_tile(158, 30, 124, 46, 0);    /* 电量 */
    bm_tile(158, 84, 124, 46, 0);    /* 连接 */
    bm_tile(290, 30, 124, 100, 1);   /* 模式（反相激活） */
    bm_draw_page_dots();
}
static void bm_draw_home_text(void)
{
    const theme_palette *p = bm_pal();
    char buf[16];

    /* 时间磁贴：TIME（9px）/ 14:32（32px）/ 08.17 周一（16px） */
    bm_text_direct(24, 39, "时间", p->muted, p->card);
    bm_fmt_clock(buf);
    bm_text_direct_clock(24, 58, buf, p->fg, p->card);
    bm_fmt_date(buf);
    if (bm_custom_text[0]) {
        strcpy(buf, bm_custom_text);
        bm_text_direct(24, 104, buf, p->muted, p->card);
    } else {
        bm_draw_date(24, 104, buf, p->muted, p->card);
    }

    /* 电量磁贴：BAT / 87% */
    bm_text_direct(168, 37, "电量", p->muted, p->card);
    buf[0] = (char)('0' + g_ui.battery / 10); buf[1] = (char)('0' + g_ui.battery % 10);
    buf[2] = '%'; buf[3] = 0;
    bm_text_direct_16(168, 55, buf, p->fg, p->card);

    /* 连接磁贴：连接 / BT + 状态点 */
    bm_text_direct(168, 91, "连接", p->muted, p->card);
    bm_text_direct_16(168, 109, bm_mode_name(), p->fg, p->card);
    NV3007_FillRect(250, 112, 5, 5, bm_565(p->active));

    /* 模式磁贴（反相）：MODE / FN1 / LAYER 1 */
    bm_text_direct(300, 39, "当前模式", p->active_fg, p->active);
    bm_text_direct_mode(300, 58, "FN1", p->active_fg, p->active);
    bm_text_direct(300, 104, "LAYER 1", p->active_fg, p->active);
}
static void bm_refresh_home_clock(void)
{
    static char prev[8] = "";
    char buf[8];
    const theme_palette *p = bm_pal();
    bm_fmt_clock(buf);
    if (strcmp(buf, prev) == 0) return;
    strcpy(prev, buf);
    bm_fill_bg_rect(24, 58, 120, 28, bm_565(p->card));
    bm_text_direct_clock(24, 58, buf, p->fg, p->card);
}
static void bm_refresh_home_date(void)
{
    static char prev[16] = "";
    char buf[16];
    const theme_palette *p = bm_pal();
    bm_fmt_date(buf);
    if (bm_custom_text[0]) strcpy(buf, bm_custom_text);
    if (strcmp(buf, prev) == 0) return;
    strcpy(prev, buf);
    bm_fill_bg_rect(24, 104, 120, 12, bm_565(p->card));
    if (bm_custom_text[0]) bm_text_direct(24, 104, buf, p->muted, p->card);
    else bm_draw_date(24, 104, buf, p->muted, p->card);
}

/* ---------- CALC-01 结果页 / CALC-02 运算页 ---------- */
static double bm_eval(const char *s);   /* 定义在下方（预览用） */

static void bm_draw_calc_shapes(void)
{
    const theme_palette *p = bm_pal();
    bm_fill_page_bg(p->bg);
    bm_draw_head("计算器", g_ui.calc.finalized ? "DEC" : "M·512", NULL);
    bm_draw_page_dots();
}
static void bm_draw_calc_text(void)
{
    const theme_palette *p = bm_pal();
    char rb[24], eb[40];

    if (g_ui.calc.finalized) {
        /* 结果页：表达式（13px muted，×÷）+ 40px 结果 + 光标（1.1s 闪烁） */
        bm_ops_display(eb, g_ui.calc.expr);
        if (eb[0]) strcat(eb, " =");
        bm_text_direct_right_expr((uint16_t)(TFT_W - 14), 40, eb, p->muted, p->bg);
        bm_fmt_result(rb, sizeof(rb), g_ui.calc.result);
        bm_text_direct_right_result((uint16_t)(TFT_W - 14), 58, rb, p->fg, p->bg);
        if ((g_bm_tick_ms / 1100) & 1)
            NV3007_FillRect((uint16_t)(TFT_W - 10), 58, 3, 40, bm_565(p->active));
    } else {
        /* 运算页：左历史列表 + 右表达式（22px，×÷）+ 预览（13px muted "= 64"） */
        uint8_t hi;
        bm_text_direct(14, 40, "最近计算", p->muted, p->bg);
        for (hi = 0; hi < g_ui.calc.hist_n && hi < 3; hi++) {
            uint16_t hy = (uint16_t)(58 + hi * 20);
            char hx[21], hd[24];
            strncpy(hx, g_ui.calc.hist[hi].expr, sizeof(hx) - 1);
            hx[sizeof(hx) - 1] = 0;
            while (hx[0] && bm_text_width_expr(hx) > 170) hx[strlen(hx) - 1] = 0;
            bm_ops_display(hd, hx);
            bm_text_direct_expr(14, hy, hd, p->fg, p->bg);
            bm_text_direct_right_expr(190, hy, g_ui.calc.hist[hi].res, p->muted, p->bg);
        }
        bm_ops_display(eb, g_ui.calc.expr);
        while (eb[0] && bm_text_width_mode(eb) > 380) eb[strlen(eb) - 1] = 0;
        bm_text_direct_right_mode((uint16_t)(TFT_W - 14), 44, eb, p->fg, p->bg);
        if ((g_bm_tick_ms / 1100) & 1)
            NV3007_FillRect((uint16_t)(TFT_W - 10), 44, 3, 22, bm_565(p->active));
        if (g_ui.calc.expr[0]) {
            char pv[24];
            bm_fmt_result(rb, sizeof(rb), bm_eval(g_ui.calc.expr));
            pv[0] = '='; pv[1] = ' '; strcpy(pv + 2, rb);
            bm_text_direct_right_expr((uint16_t)(TFT_W - 14), 100, pv, p->muted, p->bg);
        }
    }
}
static void bm_refresh_calc(void)
{
    const theme_palette *p = bm_pal();
    bm_fill_bg_rect(14, 38, (uint16_t)(TFT_W - 28), 80, bm_565(p->bg));
    bm_draw_calc_text();
}

/* ---------- SETT-01 设置 2×2 磁贴 ---------- */
static void bm_draw_sett_tile(uint8_t idx)
{
    const theme_palette *p = bm_pal();
    uint16_t x = (idx == 0 || idx == 2) ? 14 : 218;
    uint16_t y = (idx < 2) ? 38 : 88;
    uint16_t w = 196, h = 42;
    char val[16];

    if (idx == 2) {   /* 主题（反相激活，深/浅联动） */
        bm_tile(x, y, w, h, 1);
        bm_text_direct((uint16_t)(x + 10), (uint16_t)(y + 4), "主题", p->active_fg, p->active);
        bm_text_direct_16((uint16_t)(x + 10), (uint16_t)(y + 22),
                       g_ui.theme == THEME_KLB_LIGHT ? "浅色" : "深色", p->active_fg, p->active);
        return;
    }
    bm_tile(x, y, w, h, 0);
    switch (idx) {
    case 0:   /* 亮度：值 + 3px 进度条 */
        bm_text_direct((uint16_t)(x + 10), (uint16_t)(y + 4), "亮度", p->muted, p->card);
        val[0] = (char)('0' + g_ui.brightness / 10); val[1] = (char)('0' + g_ui.brightness % 10);
        val[2] = '%'; val[3] = 0;
        bm_text_direct_16((uint16_t)(x + 10), (uint16_t)(y + 22), val, p->fg, p->card);
        NV3007_FillRect((uint16_t)(x + 10), (uint16_t)(y + 38), (uint16_t)(w - 20), 3, bm_565(p->border));
        NV3007_FillRect((uint16_t)(x + 10), (uint16_t)(y + 38),
                        (uint16_t)((w - 20) * g_ui.brightness / 100), 3, bm_565(p->muted));
        break;
    case 1: { /* 自动休眠：值 + 13×13 开关（ON=active 填充） */
        uint16_t s = g_sleep_opts[g_ui.sleep_index & 3];
        uint16_t sx = (uint16_t)(x + w - 10 - 13);
        uint16_t sy = (uint16_t)(y + (h - 13) / 2);
        bm_text_direct((uint16_t)(x + 10), (uint16_t)(y + 4), "自动休眠", p->muted, p->card);
        if (s == 0) strcpy(val, "永不");
        else { val[0] = (char)('0' + s / 10); val[1] = (char)('0' + s % 10);
               val[2] = 'm'; val[3] = 'i'; val[4] = 'n'; val[5] = 0; }
        bm_text_direct_16((uint16_t)(x + 10), (uint16_t)(y + 22), val, p->fg, p->card);
        if (s != 0)
            NV3007_FillRect(sx, sy, 13, 13, bm_565(p->active));
        else
            bm_rect_direct(sx, sy, (uint16_t)(sx + 12), (uint16_t)(sy + 12), 1, bm_565(p->border));
        break;
    }
    default:  /* 恢复默认：执行/完成 */
        bm_text_direct((uint16_t)(x + 10), (uint16_t)(y + 4), "恢复默认", p->muted, p->card);
        bm_text_direct_16((uint16_t)(x + 10), (uint16_t)(y + 22),
                       g_ui.reset_t ? "完成" : "执行", p->fg, p->card);
        break;
    }
}
/* ---------- SETT-02 主题选择（2×1 并排：深色 / 浅色） ---------- */
static void bm_draw_theme_shapes(void)
{
    const theme_palette *p = bm_pal();
    uint16_t x0 = 14, x1 = 218, y = 36, w = 196, h = 94;
    uint8_t dark = (g_ui.theme == THEME_KLB);
    bm_fill_page_bg(p->bg);
    bm_draw_head("设置", "外观", NULL);

    /* 深色（默认） */
    bm_tile(x0, y, w, h, dark);
    bm_text_direct(x0 + 12, y + 8, "深色主题", dark ? p->active_fg : p->muted, dark ? p->active : p->card);
    bm_text_direct_16(x0 + 12, y + 30, "克莱因蓝", dark ? p->active_fg : p->fg, dark ? p->active : p->card);
    bm_text_direct(x0 + 12, y + 74, "默认 · 低功耗", dark ? p->active_fg : p->muted, dark ? p->active : p->card);
    NV3007_FillRect(x0 + w - 28, y + h - 28, 12, 12, 0x0194);  /* IKB #002FA7 */

    /* 浅色（明亮） */
    bm_tile(x1, y, w, h, !dark);
    bm_text_direct(x1 + 12, y + 8, "浅色主题", !dark ? p->active_fg : p->muted, !dark ? p->active : p->card);
    bm_text_direct_16(x1 + 12, y + 30, "冰白", !dark ? p->active_fg : p->fg, !dark ? p->active : p->card);
    bm_text_direct(x1 + 12, y + 74, "明亮 · 高对比", !dark ? p->active_fg : p->muted, !dark ? p->active : p->card);
    NV3007_FillRect(x1 + w - 28, y + h - 28, 12, 12, 0xFFFF);  /* 白 #FCFEFF */
    bm_rect_direct(x1 + w - 28, y + h - 28, x1 + w - 17, y + h - 17, 1, bm_565(p->border));
}

static void bm_draw_settings_shapes(void)
{
    const theme_palette *p = bm_pal();
    if (g_ui.sett_sub == 1) { bm_draw_theme_shapes(); return; }
    bm_fill_page_bg(p->bg);
    bm_draw_head("设置", NULL, "1 / 2");
    bm_draw_sett_tile(0);
    bm_draw_sett_tile(1);
    bm_draw_sett_tile(2);
    bm_draw_sett_tile(3);
    bm_draw_page_dots();
}
static void bm_refresh_settings_tile(uint8_t idx)
{
    const theme_palette *p = bm_pal();
    uint16_t x = (idx == 0 || idx == 2) ? 14 : 218;
    uint16_t y = (idx < 2) ? 38 : 88;
    bm_fill_bg_rect(x, y, 196, 42, bm_565(p->bg));
    bm_draw_sett_tile(idx);
}

/* ---------- 整页绘制 ---------- */
static void bm_draw_page(void)
{
    if (!g_ui.ready) return;
#if BM_UI_BG_ONLY == 1
    {
        uint16_t yy, xx;
        for (yy = 0; yy < TFT_H; yy++) {
            for (xx = 0; xx < TFT_W; xx++) g_line[xx] = 0xF800;
            NV3007_FlushRow(yy, g_line);
        }
    }
#else
    switch (g_ui.page) {
    case UI_PAGE_HOME:     bm_draw_home_shapes(); bm_draw_home_text(); break;
    case UI_PAGE_CALC:     bm_draw_calc_shapes(); bm_draw_calc_text(); break;
    case UI_PAGE_SETTINGS: bm_draw_settings_shapes(); break;
    default: break;
    }
#endif
}

/* ══════════════ 局部刷新（B0.8.8+）══════════════
 * 按键/设置变更只重绘受影响区域，避免整页重绘。 */
#define UI_PART_NONE  0
#define UI_PART_CALC  1
#define UI_PART_SET0  2
#define UI_PART_SET1  4
#define UI_PART_SET3  8

static double bm_eval(const char *s)
{
    const char *p = s;
    double total = 0;
    int sign = 1;
    char term[32];
    int ti = 0;
    while (1) {
        char c = *p++;
        if (c == '+' || c == '-' || c == '\0') {
            int i = 0, had = 0;
            double v = 1.0, num = 0;
            char op = '*';
            term[ti] = '\0';
            while (term[i]) {
                char t = term[i++];
                if (t >= '0' && t <= '9') { num = num * 10 + (t - '0'); had = 1; }
                else if (t == '*' || t == '/') {
                    if (had) { v = (op == '*') ? v * num : (num == 0 ? 1.0 / 0.0 : v / num); num = 0; had = 0; }
                    op = t;
                }
            }
            if (had) { v = (op == '*') ? v * num : (num == 0 ? 1.0 / 0.0 : v / num); }
            if (ti > 0) total += sign * v;
            ti = 0;
            sign = (c == '-') ? -1 : 1;
            if (c == '\0') break;
        } else {
            if (ti < 31) term[ti++] = c;
        }
    }
    return total;
}

/* ══════════════�?RTC ══════════════�?*/
#ifndef BM_SIM
static void bm_rtc_init(void)
{
    uint16_t y, mo, d, h, mi, s;
    sys_safe_access_enable();
    R8_CK32K_CONFIG &= ~(RB_CLK_OSC32K_XT | RB_CLK_XT32K_PON);
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_CK32K_CONFIG |= RB_CLK_INT32K_PON;
    sys_safe_access_disable();
    RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
    if (y <= 2020 || y > 2070) {
        RTC_InitTime(2026, 1, 1, 0, 0, 0);
    }
}

static void bm_clock_read(void)
{
    uint16_t y, mo, d, h, mi, s;
    RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
    g_ui.clock.year = y;
    g_ui.clock.mon  = (uint8_t)mo;
    g_ui.clock.day  = (uint8_t)d;
    g_ui.clock.hh   = (uint8_t)h;
    g_ui.clock.mm   = (uint8_t)mi;
}

void ui_hook_get_rtc(int *hour, int *min, int *sec)
{
    uint16_t y, mo, d, h, mi, s;
    RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
    if (hour) *hour = h;
    if (min)  *min  = mi;
    if (sec)  *sec  = s;
}
#else  /* BM_SIM: PC local clock */
static void bm_rtc_init(void) { }

static void bm_clock_read(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    const char *sh = getenv("SIM_CLOCK_HH");
    const char *sm = getenv("SIM_CLOCK_MM");
    g_ui.clock.year = (uint16_t)(t->tm_year + 1900);
    g_ui.clock.mon  = (uint8_t)(t->tm_mon + 1);
    g_ui.clock.day  = (uint8_t)t->tm_mday;
    g_ui.clock.hh   = sh ? (uint8_t)atoi(sh) : (uint8_t)t->tm_hour;
    g_ui.clock.mm   = sm ? (uint8_t)atoi(sm) : (uint8_t)t->tm_min;
}

void ui_hook_get_rtc(int *hour, int *min, int *sec)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (hour) *hour = t->tm_hour;
    if (min)  *min  = t->tm_min;
    if (sec)  *sec  = t->tm_sec;
}
#endif

#if defined(__GNUC__)
#define UI_WEAK __attribute__((weak))
#else
#define UI_WEAK
#endif

UI_WEAK void ui_hook_mode_output(ui_mode_t mode) { (void)mode; }
UI_WEAK void ui_hook_reset_connection(void) { }

/* ══════════════�?公共 API ══════════════�?*/
void ui_set_page(ui_page_t page)
{
    if (page < UI_PAGE_COUNT) {
        g_ui.page = (uint8_t)page;
        g_ui.dirty = 1;
    }
}

ui_page_t ui_get_page(void) { return (ui_page_t)g_ui.page; }

void ui_set_theme(ui_theme_t dark)
{
    g_ui.theme = dark ? THEME_KLB : THEME_KLB_LIGHT;
    g_ui.dirty = 1;
}

void ui_set_mode(ui_mode_t mode)
{
    if (mode < UI_MODE_COUNT) {
        g_ui.mode = (uint8_t)mode;
        ui_hook_mode_output(mode);
        g_ui.dirty = 1;
    }
}

void ui_calc_input(char key)
{
    char *e = g_ui.calc.expr;
    if (key == 'C') {
        e[0] = 0; g_ui.calc.result = 0; g_ui.calc.finalized = 0;
    } else if (key == '\b') {
        size_t n = strlen(e);
        if (g_ui.calc.finalized) { e[0] = 0; g_ui.calc.result = 0; g_ui.calc.finalized = 0; }
        else if (n) e[n - 1] = 0;
    } else if (key == '=') {
        g_ui.calc.result = bm_eval(e);
        g_ui.calc.finalized = 1;
        /* CALC-02：记录最近计算（最多 3 条，最新在前） */
        if (e[0]) {
            uint8_t i;
            if (g_ui.calc.hist_n < 3) g_ui.calc.hist_n++;
            for (i = g_ui.calc.hist_n - 1; i > 0; i--) {
                strcpy(g_ui.calc.hist[i].expr, g_ui.calc.hist[i - 1].expr);
                strcpy(g_ui.calc.hist[i].res,  g_ui.calc.hist[i - 1].res);
            }
            strncpy(g_ui.calc.hist[0].expr, e, sizeof(g_ui.calc.hist[0].expr) - 1);
            g_ui.calc.hist[0].expr[sizeof(g_ui.calc.hist[0].expr) - 1] = 0;
            bm_fmt_result(g_ui.calc.hist[0].res, sizeof(g_ui.calc.hist[0].res), g_ui.calc.result);
        }
    } else {
        size_t n;
        if (g_ui.calc.finalized) { e[0] = 0; g_ui.calc.finalized = 0; }
        n = strlen(e);
        if (n < sizeof(g_ui.calc.expr) - 1) { e[n] = key; e[n + 1] = 0; }
    }
    g_ui.partial = UI_PART_CALC;
}

void ui_settings_apply(uint8_t idx)
{
    /* SETT-02 主题选择子页：1=深色 2=浅色 3=返回 */
    if (g_ui.sett_sub == 1) {
        if (idx == 0) { ui_set_theme(1); return; }   /* 左磁贴 = 深色 */
        if (idx == 1) { ui_set_theme(0); return; }   /* 右磁贴 = 浅色 */
        if (idx == 2) { g_ui.sett_sub = 0; g_ui.dirty = 1; return; }  /* 返回 SETT-01 */
        return;
    }
    switch (idx) {
    case 0: { /* 亮度�?0�?0�?0�?0�?00 */
        static const uint8_t steps[5] = { 20, 40, 60, 80, 100 };
        uint8_t i;
        for (i = 0; i < 5; i++) if (steps[i] == g_ui.brightness) break;
        g_ui.brightness = steps[(i + 1) % 5];
        break;
    }
    case 1: /* 休眠�?0s/30s/60s/永不 */
        g_ui.sleep_index = (uint8_t)((g_ui.sleep_index + 1) & 3);
        break;
    case 2: /* 主题：设置页进入 SETT-02 选择；其他页快捷循环 */
        if (g_ui.page == UI_PAGE_SETTINGS) {
            g_ui.sett_sub = 1;      /* 进入 SETT-02 主题选择 */
        } else {
            g_ui.theme = (uint8_t)((g_ui.theme + 1) % THEME_COUNT);
        }
        break;
    case 3: /* 重置连接 */
        ui_reset_connection();
        break;
    default:
        break;
    }
    if (idx == 2) {
        g_ui.dirty = 1;              /* 主题：全局配色，整页重绘 */
    } else {
        g_ui.dirty = 0;              /* 其余行：局部刷新该行（覆盖 reset 的 dirty） */
        g_ui.partial = (idx == 0) ? UI_PART_SET0
                     : (idx == 1) ? UI_PART_SET1
                     :              UI_PART_SET3;
    }
}

uint8_t ui_get_brightness(void) { return g_ui.brightness; }
uint8_t ui_get_theme(void) { return g_ui.theme; }

void ui_set_brightness(uint8_t percent)
{
    if (percent > 100) percent = 100;
    g_ui.brightness = percent;
    g_ui.dirty = 1;
}

int ui_get_sleep_seconds(void) { return (int)g_sleep_opts[g_ui.sleep_index & 3]; }

void ui_reset_connection(void)
{
    ui_hook_reset_connection();
    g_ui.reset_t = g_bm_tick_ms;
    g_ui.dirty = 1;
}

/* ?? Custom display text (raw HID 0xE2/0xE3, persisted at 0x3F10) ?? */
void UI_UpdateCustomText(void)
{
    g_ui.dirty = 1;
}

const char *UI_GetCustomText(void)
{
    return bm_custom_text;
}

void UI_SetCustomText(const uint8_t *data, uint8_t len)
{
    uint8_t i;
    if (len > UI_TEXT_MAX) len = UI_TEXT_MAX;
    for (i = 0; i < len; i++) bm_custom_text[i] = (char)data[i];
    bm_custom_text[len] = 0;
    bm_custom_loaded = 1;
#ifndef BM_SIM
    EEPROM_ERASE(UI_TEXT_ADDR, 4);
    EEPROM_WRITE(UI_TEXT_ADDR, (uint8_t *)bm_custom_text, len + 1);
#endif
    UI_UpdateCustomText();
}


/* ══════════════�?裸机入口 ══════════════�?*/
void ui_bm_init(void)
{
#ifndef BM_SIM
    TMR0_TimerInit(60000);                 /* 1ms @ 60MHz */
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_EnableIRQ(TMR0_IRQn);
#endif

    bm_rtc_init();

    /* Load persisted custom text (raw HID 0xE2) */
    bm_custom_loaded = 0;
#ifndef BM_SIM
    EEPROM_READ(UI_TEXT_ADDR, (uint8_t *)bm_custom_text, UI_TEXT_MAX + 1);
    bm_custom_text[UI_TEXT_MAX] = 0;
    { uint8_t ci; for (ci = 0; ci < UI_TEXT_MAX; ci++) {
        char ch = bm_custom_text[ci];
        if (ch == (char)0xFF || ch < 0x20 || ch > 0x7E) { bm_custom_text[ci] = 0; break; }
    } }
#else
    bm_custom_text[0] = 0;
#endif

    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.theme = THEME_KLB_LIGHT;         /* 默认浅色（冰白） */
    g_ui.brightness = 80;
    g_ui.battery = 86;
    bm_clock_read();
    g_ui.ready = 1;

#if BM_UI_DIR_TEST_BOOT
    NV3007_DisplayOn();
    ui_bm_direction_test();
#else
    bm_draw_page();
    NV3007_DisplayOn();              /* 首帧写入 GRAM 后再开显示 */
#endif
}

void ui_init(void)
{
    ui_bm_init();
}

void ui_bm_process(void)
{
    static uint32_t last = 0;
    uint32_t now = g_bm_tick_ms;

    /* 按键/状态变更（可能来自 TMR3 ISR）→ 主循环统一重绘，避免 ISR 内长阻塞。
     * B0.8.8：局部刷新优先（计算器/设置行），整页 dirty 仍走整页重绘。 */
    if (g_ui.dirty) {
        g_ui.dirty = 0;
        g_ui.partial = 0;
        bm_draw_page();
    } else if (g_ui.partial) {
        uint8_t part = g_ui.partial;
        g_ui.partial = 0;
        if ((part & UI_PART_CALC) && g_ui.page == UI_PAGE_CALC)
            bm_refresh_calc();
        else if (part & UI_PART_CALC)
            g_ui.dirty = 1;               /* 页面已切换：下轮整页重绘 */
        if ((part & (UI_PART_SET0 | UI_PART_SET1 | UI_PART_SET3)) &&
            g_ui.page == UI_PAGE_SETTINGS) {
            if (part & UI_PART_SET0) bm_refresh_settings_tile(0);
            if (part & UI_PART_SET1) bm_refresh_settings_tile(1);
            if (part & UI_PART_SET3) bm_refresh_settings_tile(3);
        } else if (part & (UI_PART_SET0 | UI_PART_SET1 | UI_PART_SET3))
            g_ui.dirty = 1;
    }

    if (now - last >= 1000) {
        last = now;
        bm_clock_read();
#if !BM_UI_BG_ONLY
        if (g_ui.page == UI_PAGE_HOME) {
            /* Partial refresh: only changed text regions, never the
             * whole page - keeps the panel settled. */
            bm_refresh_home_clock();
            bm_refresh_home_date();
        }
#endif
    }

    /* 设置页“已重置”→恢复“执行�?*/
    if (g_ui.reset_t && now - g_ui.reset_t >= 900) {
        g_ui.reset_t = 0;
        if (g_ui.page == UI_PAGE_SETTINGS) bm_refresh_settings_tile(3);
    }
}

void ui_bm_direction_test(void)
{
    const theme_palette *p = &PALETTES[THEME_KLB];
    bm_fill_page_bg(p->bg);
    bm_text_direct_16(30, 100, "TOP", p->fg, p->bg);
    bm_text_direct_16(30, 120, "BOT", p->fg, p->bg);
    bm_text_direct_16(50, 60, "2Pq", p->fg, p->bg);
    bm_icon16_direct(210, 40, bm_icon_sun, p->muted, p->bg);
    bm_icon16_direct(250, 40, bm_icon_half, p->muted, p->bg);
    bm_icon16_direct(290, 40, bm_icon_reset, p->muted, p->bg);
}

#endif /* UI_BM_EN */
