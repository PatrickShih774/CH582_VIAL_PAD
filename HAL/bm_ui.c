/**
 * bm_ui.c �?裸机 UI（无 LVGL�?
 *
 * 设计规范：C:\ClaudeProject\tft_NV3007\brand-spec.md
 *   - 六主题（像素/极简/黑客 × 双色），设置页循�?
 *   - 三页面（主页 / 计算�?/ 设置），底部三点导航
 *   - 428×142 横屏，无全帧缓冲，直接窗口直�?
 * 驱动：复�?HAL/NV3007.c（API 命名沿用 ST7789_* 兼容旧调用点，逻辑横屏 428×142，行=物理列转置）�?
 * 公共 API �?LVGL �?numpad_ui.h 同名，按键路由零改动�?
 */
#ifndef BM_SIM
#include "config.h"
#include "CH58x_timer.h"
#include "CH58x_clk.h"
#endif
#include "NV3007.h"
#include "bm_ui.h"
#include "bm_font.h"
#include <string.h>
#ifdef BM_SIM
#include <time.h>
void DelayMs(uint16_t ms);          /* provided by the SDL simulator backend */
#endif

#if UI_BM_EN

/* B0.8 debug: 0 = normal UI; 1 = solid RED direct flush (bypasses dirty
 * bitmap / content renderers); 2 = boot sequence of diagnostic frames. */
#define BM_UI_BG_ONLY 0

/* 逻辑横屏尺寸（对�?HAL/NV3007.h�?*/
#define TFT_W ST7789_WIDTH
#define TFT_H ST7789_HEIGHT

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
    THEME_PIXEL_GREEN = 0,
    THEME_PIXEL_AMBER,
    THEME_MIN_LIGHT,      /* 默认 */
    THEME_MIN_DARK,
    THEME_HACK_GREEN,
    THEME_HACK_AMBER,
    THEME_COUNT
} bm_theme_t;

static const theme_palette PALETTES[THEME_COUNT] = {
    /* 像素·�?*/
    { {0xBD,0xE0,0xB0},{0xAE,0xD4,0xA0},{0x3F,0x6E,0x4B},{0x12,0x36,0x1F},
      {0x1F,0x6B,0x3F},{0xA8,0xCC,0x9A},{0x12,0x36,0x1F},{0xBD,0xE0,0xB0},{0xB7,0xDA,0xAA} },
    /* 像素·琥珀 */
    { {0xE3,0xD8,0xB5},{0xD6,0xCC,0xA3},{0x6E,0x5C,0x3F},{0x36,0x1F,0x12},
      {0x6E,0x4A,0x3F},{0xD0,0xC6,0xA0},{0x36,0x1F,0x12},{0xE3,0xD8,0xB5},{0xE0,0xD6,0xB0} },
    /* 极简·浅（默认�?*/
    { {0xFF,0xFF,0xFF},{0xFF,0xFF,0xFF},{0xE8,0xE8,0xE8},{0x1A,0x1A,0x1A},
      {0x8C,0x8C,0x8C},{0xEF,0xEF,0xEF},{0x27,0x66,0xC0},{0xFF,0xFF,0xFF},{0xFA,0xFA,0xFA} },
    /* 极简·�?*/
    { {0x12,0x12,0x12},{0x1E,0x1E,0x1E},{0x33,0x33,0x33},{0xEE,0xEE,0xEE},
      {0x88,0x88,0x88},{0x2A,0x2A,0x2A},{0x27,0x66,0xC0},{0xFF,0xFF,0xFF},{0x1A,0x1A,0x1A} },
    /* 黑客·�?*/
    { {0x02,0x0D,0x08},{0x0E,0x22,0x18},{0x2E,0x7D,0x4F},{0x41,0xFF,0x9A},
      {0x5F,0xC9,0x8C},{0x0F,0x24,0x18},{0x41,0xFF,0x9A},{0x02,0x10,0x0A},{0x06,0x13,0x0C} },
    /* 黑客·琥珀 */
    { {0x0D,0x07,0x02},{0x20,0x12,0x08},{0x7D,0x4A,0x2E},{0xFF,0xB4,0x41},
      {0xC9,0x8F,0x5F},{0x24,0x14,0x09},{0xFF,0xB4,0x41},{0x10,0x0A,0x02},{0x13,0x09,0x05} },
};

/* ══════════════�?16×16 图标位图 ══════════════�?*/
static const uint8_t bm_icon_bt[32] = {
    0x08,0x00, 0x18,0x00, 0x14,0x00, 0x12,0x00, 0x11,0x00, 0x10,0x80,
    0x18,0x60, 0x10,0x20, 0x18,0x60, 0x10,0x80, 0x11,0x00, 0x12,0x00,
    0x14,0x00, 0x18,0x00, 0x08,0x00, 0x00,0x00,
};
static const uint8_t bm_icon_sun[32] = {
    0x00,0x00, 0x01,0x80, 0x01,0x80, 0x01,0x80, 0x30,0xC3, 0x30,0xC3,
    0x01,0x80, 0x0F,0xF0, 0x0F,0xF0, 0x01,0x80, 0x30,0xC3, 0x30,0xC3,
    0x01,0x80, 0x01,0x80, 0x01,0x80, 0x00,0x00,
};
static const uint8_t bm_icon_clock[32] = {
    0x00,0x00, 0x0F,0xF0, 0x18,0x18, 0x10,0x08, 0x10,0x88, 0x10,0x88,
    0x10,0x88, 0x10,0x28, 0x10,0x08, 0x18,0x18, 0x0F,0xF0, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};
/* 半日（主题图标，brand-spec：半填充�?+ 分界�?*/
static const uint8_t bm_icon_half[32] = {
    0x00,0x00, 0xFF,0xC0, 0xFF,0xF0, 0xFF,0xF8, 0xFF,0xFC, 0xFF,0xFC,
    0xFF,0xFE, 0xFF,0xFE, 0xFF,0xFE, 0xFF,0xFE, 0xFF,0xFC, 0xFF,0xFC,
    0xFF,0xF8, 0xFF,0xF0, 0xFF,0xC0, 0x00,0x00,
};
static const uint8_t bm_icon_reset[32] = {
    0x00,0x00, 0x0F,0xF0, 0x18,0x18, 0x21,0x84, 0x22,0x44, 0x24,0x24,
    0x24,0x24, 0x24,0x24, 0x22,0x44, 0x21,0x84, 0x18,0x18, 0x0F,0xF0,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};

/* ══════════════�?UI 状�?══════════════�?*/
typedef struct {
    uint8_t  page;         /* UI_PAGE_* */
    uint8_t  theme;        /* bm_theme_t */
    uint8_t  mode;         /* UI_MODE_* */
    uint8_t  battery;      /* 0..100 */
    uint8_t  brightness;   /* 20/40/60/80/100 */
    uint8_t  sleep_index;  /* 0..3 -> 10s/30s/60s/永不 */
    uint8_t  ready;
    uint8_t  dirty;        /* 按键/状态变更后置位，主循环统一重绘 */
    uint32_t reset_t;      /* "已重�? 反馈时间�?*/
    struct { char expr[32]; double result; uint8_t finalized; } calc;
    struct { uint8_t hh, mm; uint16_t year; uint8_t mon, day; } clock;
} bm_ui_t;

static bm_ui_t g_ui;
static const uint8_t g_sleep_opts[4] = { 10, 30, 60, 0 };

static const theme_palette *bm_pal(void);

/* ══════════════�?渲染原语（行缓冲合成 + 整行直写�?══════════════�?
 * 每页先合成为 428×2B 行缓冲，结束时按“自检纯色同款”的整列窗口写面�?
 * （每逻辑行一�?SetWindow + 428 像素），避免成百上千次小窗口 RAMWR
 * �?NV3007 上引起条�?亮度不均，同时大幅提升刷新速度�?*/
static uint16_t g_line[TFT_W];               /* 428×2B 行缓�?*/
static uint8_t  g_dirty[(TFT_H + 7) / 8];    /* 脏行位图 18B */

static inline uint16_t bm_565(scolor c)
{
    return (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
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
        ST7789_FlushRow(y, g_line);
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
    for (row = 0; row < g.h; row++) {
        uint8_t bits = g.data[row];
        for (col = 0; col < g.w; col++) {
            if (bits & (0x80 >> col)) {
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

static void bm_text_right(uint16_t xr, uint16_t y, const char *s, uint8_t scale, scolor c)
{
    uint16_t w = bm_text_width(s, scale);
    uint16_t x = (w < xr + 1) ? (uint16_t)(xr - w + 1) : 0;
    bm_text(x, y, s, scale, c);
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
    return g_ui.theme == THEME_PIXEL_GREEN || g_ui.theme == THEME_PIXEL_AMBER;
}

static uint8_t bm_is_hack(void)
{
    return g_ui.theme == THEME_HACK_GREEN || g_ui.theme == THEME_HACK_AMBER;
}

static uint8_t bm_is_min(void)
{
    return g_ui.theme == THEME_MIN_LIGHT || g_ui.theme == THEME_MIN_DARK;
}

/* 时钟字号�?×7 窄点阵）：像�?49px→scale7，极简/黑客 35px→scale5 */
static uint8_t bm_clock_scale(void) { return bm_is_pixel() ? 7 : 5; }

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
    static const char *wd[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    char *p = bm_u64_str(o, g_ui.clock.year);
    *p++ = '.'; *p++ = (char)('0' + g_ui.clock.mon / 10); *p++ = (char)('0' + g_ui.clock.mon % 10);
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

/* ---- Direct-window text/icon renderers (seller method) ----
 * One window per glyph/icon, streamed CASET-inner (physical column
 * inner), writing fg/bg per scaled pixel.  Bypasses g_line entirely -
 * verified to display fine detail cleanly (the g_line path garbles it). */
static void bm_text_direct(uint16_t x, uint16_t y, const char *s,
                           uint8_t scale, scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    while (*s) {
        glyph_t g = bm_font_glyph((uint8_t)*s++);
        uint16_t gx0 = x;
        uint16_t gx1 = (uint16_t)(x + (uint16_t)g.w * scale - 1u);
        uint16_t gy1 = (uint16_t)(y + (uint16_t)g.h * scale - 1u);
        uint16_t col0 = (uint16_t)(ST7789_VIS_X1 - gy1);
        uint16_t col1 = (uint16_t)(ST7789_VIS_X1 - y);
        uint16_t xi, yi;
        ST7789_SetWindow(col0, gx0, col1, gx1);
        for (xi = gx0; xi <= gx1; xi++) {
            uint8_t cc = (uint8_t)((xi - gx0) / scale);
            for (yi = col0; yi <= col1; yi++) {
#if NV3007_TEXT_FLIP
                uint8_t rr = (uint8_t)((yi - col0) / scale);
#else
                uint8_t rr = (uint8_t)(((uint16_t)(ST7789_VIS_X1 - yi) - y) / scale);
#endif
                ST7789_WritePixel((g.data[rr] & (0x80 >> cc)) ? fg565 : bg565);
            }
        }
        x = (uint16_t)(gx1 + 1u + scale);
    }
}

/* 窄字形版（主页时钟用 5x7 数字，避�?8x8 宽点阵侵入状态簇�?*/
static void bm_text_direct_narrow(uint16_t x, uint16_t y, const char *s,
                                  uint8_t scale, scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    while (*s) {
        glyph_t g = bm_font_glyph_narrow((uint8_t)*s++);
        uint16_t gx0 = x;
        uint16_t gx1 = (uint16_t)(x + (uint16_t)g.w * scale - 1u);
        uint16_t gy1 = (uint16_t)(y + (uint16_t)g.h * scale - 1u);
        uint16_t col0 = (uint16_t)(ST7789_VIS_X1 - gy1);
        uint16_t col1 = (uint16_t)(ST7789_VIS_X1 - y);
        uint16_t xi, yi;
        ST7789_SetWindow(col0, gx0, col1, gx1);
        for (xi = gx0; xi <= gx1; xi++) {
            uint8_t cc = (uint8_t)((xi - gx0) / scale);
            for (yi = col0; yi <= col1; yi++) {
#if NV3007_TEXT_FLIP
                uint8_t rr = (uint8_t)((yi - col0) / scale);
#else
                uint8_t rr = (uint8_t)(((uint16_t)(ST7789_VIS_X1 - yi) - y) / scale);
#endif
                ST7789_WritePixel((g.data[rr] & (0x80 >> cc)) ? fg565 : bg565);
            }
        }
        x = (uint16_t)(gx1 + 1u + scale);
    }
}

static void bm_text_direct_right(uint16_t xr, uint16_t y, const char *s,
                                 uint8_t scale, scolor fg, scolor bg)
{
    uint16_t w = bm_text_width(s, scale);
    uint16_t x = (w < xr + 1) ? (uint16_t)(xr - w + 1) : 0;
    bm_text_direct(x, y, s, scale, fg, bg);
}

static void bm_icon16_direct(uint16_t x, uint16_t y, const uint8_t *data,
                             scolor fg, scolor bg)
{
    uint16_t fg565 = bm_565(fg), bg565 = bm_565(bg);
    uint8_t row, col;
    uint16_t col0 = (uint16_t)(ST7789_VIS_X1 - (y + 15));
    uint16_t col1 = (uint16_t)(ST7789_VIS_X1 - y);
    ST7789_SetWindow(col0, x, col1, (uint16_t)(x + 15));
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
            ST7789_WritePixel((bits & (0x8000 >> col)) ? fg565 : bg565);
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
    ST7789_FillRect(x0, y0, w, th, c565);
    ST7789_FillRect(x0, (uint16_t)(y1 - th + 1), w, th, c565);
    ST7789_FillRect(x0, (uint16_t)(y0 + th), 1, mid, c565);
    ST7789_FillRect(x1, (uint16_t)(y0 + th), 1, mid, c565);
}

/* 圆角近似：用背景色把四角切成 r x r 阶梯三角�? * 极简 r=3（≈8px 圆角）、黑�?r=2（≈6px 圆角）、像素不调用�?*/
static void bm_round_corners_direct(uint16_t x0, uint16_t y0, uint16_t x1,
                                    uint16_t y1, uint16_t r, uint16_t bg565)
{
    uint16_t i;
    if (r == 0) return;
    for (i = 0; i < r; i++) {
        uint16_t k = (uint16_t)(r - i);
        ST7789_FillRect((uint16_t)(x0 + i), y0, 1, k, bg565);
        ST7789_FillRect((uint16_t)(x1 - i), y0, 1, k, bg565);
        ST7789_FillRect((uint16_t)(x0 + i), (uint16_t)(y1 - k + 1), 1, k, bg565);
        ST7789_FillRect((uint16_t)(x1 - i), (uint16_t)(y1 - k + 1), 1, k, bg565);
    }
}

static void bm_battery_direct(uint16_t x, uint16_t y, uint8_t pct,
                              uint16_t outline, uint16_t fillc, uint16_t bgc)
{
    /* 14x14 图标（brand-spec / pager HTML：机�?12x6 + 端子 1x2�?*/
    ST7789_FillRect(x, (uint16_t)(y + 4), 12, 6, outline);
    ST7789_FillRect((uint16_t)(x + 1), (uint16_t)(y + 5), 10, 4, bgc);
    ST7789_FillRect((uint16_t)(x + 13), (uint16_t)(y + 6), 1, 2, outline);
    if (pct > 100) pct = 100;
    {
        uint16_t fw = (uint16_t)(9u * pct / 100u);
        if (fw > 0) ST7789_FillRect((uint16_t)(x + 1), (uint16_t)(y + 5), fw, 4, fillc);
    }
}

static void bm_draw_nav_dots_direct(void)
{
    const theme_palette *p = bm_pal();
    uint16_t bg565 = bm_565(p->bg);
    uint16_t d = bm_is_pixel() ? 8 : 6;
    uint16_t gap = bm_is_pixel() ? 10 : 8;
    uint16_t total = (uint16_t)(3 * d + 2 * gap);
    uint16_t x = (uint16_t)((TFT_W - total) / 2);
    uint16_t y = bm_is_pixel() ? (uint16_t)(TFT_H - 13)
                               : (uint16_t)(TFT_H - 12);
    int i;
    for (i = 0; i < 3; i++) {
        uint16_t dx = (uint16_t)(x + (uint16_t)i * (d + gap));
        if ((int)g_ui.page == i) {
            /* 激活态：像素=实心方块，极简=实心圆点，黑�?辉光圆点 */
            if (bm_is_pixel()) {
                ST7789_FillRect(dx, y, d, d, bm_565(p->active));
            } else if (g_ui.theme == THEME_MIN_LIGHT || g_ui.theme == THEME_MIN_DARK) {
                ST7789_FillRect(dx, y, d, d, bm_565(p->active));
                ST7789_FillRect(dx, y, 1, 1, bg565);
                ST7789_FillRect((uint16_t)(dx + d - 1), y, 1, 1, bg565);
                ST7789_FillRect(dx, (uint16_t)(y + d - 1), 1, 1, bg565);
                ST7789_FillRect((uint16_t)(dx + d - 1), (uint16_t)(y + d - 1), 1, 1, bg565);
            } else {
                ST7789_FillRect((uint16_t)(dx - 1), (uint16_t)(y - 1),
                                (uint16_t)(d + 2), (uint16_t)(d + 2), bm_565(p->pressed));
                ST7789_FillRect(dx, y, d, d, bm_565(p->active));
            }
        } else {
            /* 未激活：像素=空心方块，极简/黑客=实心圆点（border 色） */
            if (bm_is_pixel()) {
                bm_rect_direct(dx, y, (uint16_t)(dx + d - 1), (uint16_t)(y + d - 1),
                               2, bm_565(p->border));
            } else {
                ST7789_FillRect(dx, y, d, d, bm_565(p->border));
                ST7789_FillRect(dx, y, 1, 1, bg565);
                ST7789_FillRect((uint16_t)(dx + d - 1), y, 1, 1, bg565);
                ST7789_FillRect(dx, (uint16_t)(y + d - 1), 1, 1, bg565);
                ST7789_FillRect((uint16_t)(dx + d - 1), (uint16_t)(y + d - 1), 1, 1, bg565);
            }
        }
    }
}

static void bm_draw_home_shapes(void)
{
    const theme_palette *p = bm_pal();
    uint16_t bx0, bw, by, bh = 30;
    uint16_t bg565 = bm_565(p->bg);
    uint16_t fillc;
    int i;

    if (bm_is_pixel()) {
        bx0 = 266; bw = 152; by = 10;
    } else {
        bx0 = (uint16_t)(TFT_W - 170); bw = 160; by = 8;
    }

    ST7789_Fill(bg565);

    /* 电池填充（brand-spec）：极简�?�?红，其余主题次要�?*/
    if (bm_is_min())
        fillc = (g_ui.battery < 20) ? BM_BAT_LOW565 : BM_BAT_OK565;
    else
        fillc = bm_565(p->muted);

    /* 状态簇：左区（0..251）右上，right 10，gap 6：电�?| 86% | 蓝牙 */
    {
        uint16_t pct_w, bt_x, pct_x, batt_x;
        char bb[4];
        bb[0] = (char)('0' + g_ui.battery / 10);
        bb[1] = (char)('0' + g_ui.battery % 10);
        bb[2] = '%'; bb[3] = 0;
        pct_w = bm_text_width(bb, 1);
        bt_x = (uint16_t)(252 - 10 - 16);              /* 蓝牙 16px */
        pct_x = (uint16_t)(bt_x - 6 - pct_w);
        batt_x = (uint16_t)(pct_x - 6 - 14);           /* 电池 14px */
        bm_battery_direct(batt_x, (uint16_t)(bm_is_pixel() ? 8 : 10),
                          g_ui.battery, bm_565(p->muted), fillc, bg565);
    }

    for (i = 0; i < 3; i++) {
        uint16_t y = (uint16_t)(by + (uint16_t)i * (bh + 8));
        if (i == (int)g_ui.mode) {
            if (bm_is_pixel()) {
                ST7789_FillRect(bx0, y, bw, bh, bm_565(p->active));
            } else {
                uint16_t r = bm_is_min() ? 3 : 2;
                if (bm_is_hack())
                    ST7789_FillRect((uint16_t)(bx0 - 1), (uint16_t)(y - 1),
                                    (uint16_t)(bw + 2), (uint16_t)(bh + 2),
                                    bm_565(p->pressed));
                ST7789_FillRect(bx0, y, bw, bh, bm_565(p->active));
                bm_round_corners_direct(bx0, y, (uint16_t)(bx0 + bw - 1),
                                        (uint16_t)(y + bh - 1), r, bg565);
            }
        } else {
            /* 按钮背景 card + 主题边框（像�?2px fg / 极简黑客 1px border�?*/
            if (bm_is_pixel()) {
                ST7789_FillRect(bx0, y, bw, bh, bm_565(p->card));
                bm_rect_direct(bx0, y, (uint16_t)(bx0 + bw - 1),
                               (uint16_t)(y + bh - 1), 2, bm_565(p->fg));
            } else {
                uint16_t r = bm_is_min() ? 3 : 2;
                ST7789_FillRect(bx0, y, bw, bh, bm_565(p->card));
                bm_rect_direct(bx0, y, (uint16_t)(bx0 + bw - 1),
                               (uint16_t)(y + bh - 1), 1, bm_565(p->border));
                bm_round_corners_direct(bx0, y, (uint16_t)(bx0 + bw - 1),
                                        (uint16_t)(y + bh - 1), r, bg565);
            }
        }
    }
    bm_draw_nav_dots_direct();
}
static void bm_draw_home_text(void)
{
    const theme_palette *p = bm_pal();
    char buf[16];
    const char *modes[3] = { "USB", "BLE", "RF" };
    uint16_t bx0, bw, by, bh = 30;
    uint16_t ty;
    uint8_t label_scale = 2;   /* 按钮字号：像�?6 / 极简14 / 黑客13�?x7 x2�?*/
    int i;

    if (bm_is_pixel()) {
        bx0 = 266; bw = 152; by = 10;
    } else {
        bx0 = (uint16_t)(TFT_W - 170); bw = 160; by = 8;
    }

    bm_fmt_clock(buf);
    ty = bm_is_pixel() ? 26 : 12;
    if (bm_is_hack())
        bm_text_direct_narrow(15, (uint16_t)(ty + 1), buf, bm_clock_scale(),
                              p->active, p->bg);
    bm_text_direct_narrow(14, ty, buf, bm_clock_scale(), p->fg, p->bg);

    bm_fmt_date(buf);
    bm_text_direct(14, (uint16_t)(TFT_H - 28), buf, 2, p->muted, p->bg);

    buf[0] = (char)('0' + g_ui.battery / 10); buf[1] = (char)('0' + g_ui.battery % 10);
    buf[2] = '%'; buf[3] = 0;
    {
        uint16_t bt_x = (uint16_t)(252 - 10 - 16);
        uint16_t pct_w = bm_text_width(buf, 1);
        uint16_t pct_x = (uint16_t)(bt_x - 6 - pct_w);
        uint16_t top = bm_is_pixel() ? 8 : 10;
        scolor bt_col = bm_is_min() ? p->active : p->muted;
        scolor pct_col;
        if (bm_is_min()) {
            scolor c_ok = { 0x2E, 0xA0, 0x44 };
            scolor c_lo = { 0xD6, 0x45, 0x45 };
            pct_col = (g_ui.battery < 20) ? c_lo : c_ok;
        } else if (bm_is_hack()) {
            pct_col = p->fg;
        } else {
            pct_col = p->muted;
        }
        bm_icon16_direct(bt_x, top, bm_icon_bt, bt_col, p->bg);
        bm_text_direct(pct_x, (uint16_t)(top + 1), buf, 1, pct_col, p->bg);
    }

    for (i = 0; i < 3; i++) {
        uint16_t y = (uint16_t)(by + (uint16_t)i * (bh + 8));
        uint16_t th = (uint16_t)(7u * label_scale);
        uint16_t tty = (uint16_t)(y + (bh - th) / 2);
        if (bm_is_hack() && i != (int)g_ui.mode) {
            /* [ USB ]：括�?muted、名�?fg */
            char lb[3];
            uint16_t wb = bm_text_width("[ ", label_scale);
            uint16_t wn = bm_text_width(modes[i], label_scale);
            uint16_t tx = (uint16_t)(bx0 +
                            (bw - (wb + wn + bm_text_width(" ]", label_scale))) / 2);
            lb[0] = '['; lb[1] = ' '; lb[2] = 0;
            bm_text_direct(tx, tty, lb, label_scale, p->muted, p->bg);
            bm_text_direct((uint16_t)(tx + wb), tty, modes[i], label_scale, p->fg, p->bg);
            lb[0] = ' '; lb[1] = ']'; lb[2] = 0;
            bm_text_direct((uint16_t)(tx + wb + wn), tty, lb, label_scale, p->muted, p->bg);
        } else {
            const char *label = modes[i];
            char hl[12];
            uint16_t tw, tx;
            if (bm_is_hack()) {
                hl[0] = '['; hl[1] = ' ';
                strcpy(hl + 2, modes[i]);
                strcat(hl, " ]");
                label = hl;
            }
            tw = bm_text_width(label, label_scale);
            tx = (uint16_t)(bx0 + (bw - tw) / 2);
            if (i == (int)g_ui.mode)
                bm_text_direct(tx, tty, label, label_scale, p->active_fg, p->active);
            else
                bm_text_direct(tx, tty, label, label_scale, p->fg, p->bg);
        }
    }
}
static void bm_refresh_home_clock(void)
{
    static char prev[8] = "";
    char buf[8];
    const theme_palette *p = bm_pal();
    uint8_t scale = bm_clock_scale();
    uint16_t x0 = 14, y0, y1, x1, tw;

    bm_fmt_clock(buf);
    if (strcmp(buf, prev) == 0) return;
    strcpy(prev, buf);

    y0 = bm_is_pixel() ? 26 : 12;
    y1 = (uint16_t)(y0 + (uint16_t)(7u * scale) - 1u);
    tw = bm_text_width(buf, scale);
    x1 = (uint16_t)(x0 + tw - 1u);
    ST7789_FillRect(x0, y0, (uint16_t)(x1 - x0 + 1), (uint16_t)(y1 - y0 + 1), bm_565(p->bg));
    bm_text_direct_narrow(x0, y0, buf, scale, p->fg, p->bg);
}
static void bm_refresh_home_date(void)
{
    static char prev[16] = "";
    char buf[16];
    const theme_palette *p = bm_pal();
    uint16_t x0 = 14, y0, y1, x1, tw;

    bm_fmt_date(buf);
    if (strcmp(buf, prev) == 0) return;
    strcpy(prev, buf);

    y0 = (uint16_t)(TFT_H - 28);
    y1 = (uint16_t)(y0 + 15u);
    tw = bm_text_width(buf, 2);
    x1 = (uint16_t)(x0 + tw - 1u);
    ST7789_FillRect(x0, y0, (uint16_t)(x1 - x0 + 1), (uint16_t)(y1 - y0 + 1), bm_565(p->bg));
    bm_text_direct(x0, y0, buf, 2, p->muted, p->bg);
}

static void bm_draw_calc_shapes(void)
{
    const theme_palette *p = bm_pal();
    uint16_t px0 = 6, py0 = 4, px1 = (uint16_t)(TFT_W - 7), py1 = (uint16_t)(TFT_H - 15);
    uint16_t w = (uint16_t)(px1 - px0 + 1), h = (uint16_t)(py1 - py0 + 1);

    ST7789_Fill(bm_565(p->bg));
    ST7789_FillRect(px0, py0, w, h, bm_565(p->soft));
    bm_rect_direct(px0, py0, px1, py1, bm_is_pixel() ? 2 : 1, bm_565(p->border));
    /* 过程行分隔线：像�?32px �?2px，极简/黑客 36px �?1px */
    {
        uint16_t pdiv = bm_is_pixel() ? (uint16_t)(py0 + 31)
                                      : (uint16_t)(py0 + 35);
        ST7789_FillRect((uint16_t)(px0 + 2), pdiv,
                        (uint16_t)(px1 - px0 - 3),
                        bm_is_pixel() ? 2 : 1, bm_565(p->border));
    }
    bm_draw_nav_dots_direct();
}

static void bm_draw_calc_text(void)
{
    const theme_palette *p = bm_pal();
    uint16_t px0 = 6, py0 = 4, px1 = (uint16_t)(TFT_W - 7), py1 = (uint16_t)(TFT_H - 15);
    uint8_t es = 2;                       /* 过程行：16px */
    uint8_t rs = bm_is_pixel() ? 7 : 5;   /* 结果行：56px / 40px */
    uint16_t ey = (uint16_t)(py0 + (bm_is_pixel() ? 8 : 10));
    uint16_t ry;

    if (bm_is_hack() && g_ui.calc.expr[0]) {
        char eb[40];
        uint16_t ew, ex;
        eb[0] = '>'; eb[1] = ' ';
        strcpy(eb + 2, g_ui.calc.expr);
        ew = bm_text_width(eb, es);
        ex = (ew < (uint16_t)(px1 - 8) + 1) ? (uint16_t)((px1 - 8) - ew + 1) : 0;
        bm_text_direct(ex, ey, eb, es, p->muted, p->soft);
        if ((g_bm_tick_ms / 500) & 1)     /* 闪烁光标 */
            ST7789_FillRect((uint16_t)(ex + ew + 4), ey, 2,
                            (uint16_t)(7u * es), bm_565(p->active));
    } else {
        bm_text_direct_right((uint16_t)(px1 - 8), ey, g_ui.calc.expr, es,
                             p->muted, p->soft);
    }

    ry = (uint16_t)(py1 - 8 * rs - 4);
    if (g_ui.calc.finalized) {
        char rb[24];
        bm_fmt_result(rb, sizeof(rb), g_ui.calc.result);
        if (bm_is_hack())
            bm_text_direct_right((uint16_t)(px1 - 7), (uint16_t)(ry + 1),
                                 rb, rs, p->active, p->soft);
        bm_text_direct_right((uint16_t)(px1 - 8), ry, rb, rs, p->fg, p->soft);
    }
}

static void bm_draw_settings_shapes(void)
{
    const theme_palette *p = bm_pal();
    uint16_t x0 = 6, x1 = (uint16_t)(TFT_W - 7), y0 = 4, y1 = (uint16_t)(TFT_H - 15);
    uint16_t row_h = (uint16_t)((y1 - y0) / 4);
    uint8_t label_scale = 2;             /* 行文�?14-16px */
    uint8_t text_h = (uint8_t)(7u * label_scale);
    uint16_t bg565 = bm_565(p->bg);
    int i;

    ST7789_Fill(bg565);
    if (bm_is_pixel())
        bm_rect_direct(x0, y0, x1, y1, 2, bm_565(p->border));
    else if (g_ui.theme == THEME_HACK_GREEN || g_ui.theme == THEME_HACK_AMBER)
        bm_rect_direct(x0, y0, x1, y1, 1, bm_565(p->border));

    for (i = 0; i < 4; i++) {
        uint16_t ry0 = (uint16_t)(y0 + (uint16_t)i * row_h);
        uint16_t ty = (uint16_t)(ry0 + (row_h - text_h) / 2);

        if (i > 0)
            ST7789_FillRect((uint16_t)(x0 + 2), ry0, (uint16_t)(x1 - x0 - 3),
                            bm_is_pixel() ? 2 : 1, bm_565(p->border));

        if (i == 0) {
            uint16_t bar_x0;
            if (bm_is_pixel()) {
                uint16_t vw = bm_text_width("100%", 2);
                bar_x0 = (uint16_t)(x1 - 8 - vw - 6 - 64);
                bm_rect_direct(bar_x0, ty, (uint16_t)(bar_x0 + 63), (uint16_t)(ty + 7), 2, bm_565(p->border));
                if (g_ui.brightness > 0) {
                    uint16_t fw = (uint16_t)(60u * g_ui.brightness / 100u);
                    ST7789_FillRect((uint16_t)(bar_x0 + 2), (uint16_t)(ty + 2), fw, 4, bm_565(p->active));
                }
            } else {
                bar_x0 = (uint16_t)(x1 - 8 - 36 - 8 - 64);
                ST7789_FillRect(bar_x0, (uint16_t)(ty + 2), 64, 6, bm_565(p->border));
                if (g_ui.brightness > 0) {
                    uint16_t fw = (uint16_t)(64u * g_ui.brightness / 100u);
                    ST7789_FillRect(bar_x0, (uint16_t)(ty + 2), fw, 6, bm_565(p->active));
                }
            }
        }
    }
    bm_draw_nav_dots_direct();
}

static void bm_draw_settings_text(void)
{
    const theme_palette *p = bm_pal();
    static const char *names[4] = { "Bright", "Sleep", "Theme", "Reset" };
    static const char *themes[6] = { "PixG", "PixA", "MinL", "MinD", "HkG", "HkA" };
    static const uint8_t *icons[4] = { bm_icon_sun, bm_icon_clock, bm_icon_half, bm_icon_reset };
    uint16_t x0 = 6, x1 = (uint16_t)(TFT_W - 7), y0 = 4, y1 = (uint16_t)(TFT_H - 15);
    uint16_t row_h = (uint16_t)((y1 - y0) / 4);
    uint8_t label_scale = 2;
    uint8_t text_h = (uint8_t)(7u * label_scale);
    int i;

    for (i = 0; i < 4; i++) {
        uint16_t ry0 = (uint16_t)(y0 + (uint16_t)i * row_h);
        uint16_t ty = (uint16_t)(ry0 + (row_h - text_h) / 2);
        char val[16];

        bm_icon16_direct((uint16_t)(x0 + 8), (uint16_t)(ry0 + (row_h - 16) / 2), icons[i], p->muted, p->bg);
        if (bm_is_hack()) {
            char lb[24];
            lb[0] = '>'; lb[1] = ' ';
            strcpy(lb + 2, names[i]);
            bm_text_direct((uint16_t)(x0 + 30), ty, lb, label_scale, p->fg, p->bg);
        } else {
            bm_text_direct((uint16_t)(x0 + 30), ty, names[i], label_scale, p->fg, p->bg);
        }

        switch (i) {
        case 0:
            val[0] = (char)('0' + g_ui.brightness / 10); val[1] = (char)('0' + g_ui.brightness % 10);
            val[2] = '%'; val[3] = 0;
            bm_text_direct_right((uint16_t)(x1 - 8), ty, val, label_scale, p->muted, p->bg);
            break;
        case 1: {
            uint16_t s = g_sleep_opts[g_ui.sleep_index & 3];
            if (s == 0) strcpy(val, "Off");
            else { val[0] = (char)('0' + s / 10); val[1] = (char)('0' + s % 10); val[2] = 's'; val[3] = 0; }
            bm_text_direct_right((uint16_t)(x1 - 20), ty, val, label_scale, p->muted, p->bg);
            bm_text_direct((uint16_t)(x1 - 12), ty, ">", 1, p->muted, p->bg);
            break;
        }
        case 2:
            bm_text_direct_right((uint16_t)(x1 - 20), ty, themes[g_ui.theme], label_scale, p->muted, p->bg);
            bm_text_direct((uint16_t)(x1 - 12), ty, ">", 1, p->muted, p->bg);
            break;
        default:
            bm_text_direct_right((uint16_t)(x1 - 20), ty, g_ui.reset_t ? "Done" : "Run", label_scale, p->muted, p->bg);
            bm_text_direct((uint16_t)(x1 - 12), ty, ">", 1, p->muted, p->bg);
            break;
        }
    }
}
#if BM_UI_BG_ONLY == 2
/* Seller 2.79TFT demo 16x16 Chinese font (first 7 chars, 32 bytes each).
 * "ShenZhen JinYiChen Electronics" - copied verbatim from C51/main.c. */
static const uint8_t bm_seller_font[7][32] = {
    { 0x00,0x00,0xE4,0x3F,0x28,0x20,0x28,0x25,0x81,0x08,0x42,0x10,0x02,0x02,0x08,0x02,
      0xE8,0x3F,0x04,0x02,0x07,0x07,0x84,0x0A,0x44,0x12,0x34,0x62,0x04,0x02,0x00,0x02 },
    { 0x88,0x20,0x88,0x24,0x88,0x24,0x88,0x24,0x88,0x24,0xBF,0x24,0x88,0x24,0x88,0x24,
      0x88,0x24,0x88,0x24,0x88,0x24,0xB8,0x24,0x87,0x24,0x42,0x24,0x40,0x20,0x20,0x20 },
    { 0x80,0x00,0x80,0x00,0x40,0x01,0x20,0x02,0x10,0x04,0x08,0x08,0xF4,0x17,0x83,0x60,
      0x80,0x00,0xFC,0x1F,0x80,0x00,0x88,0x08,0x90,0x08,0x90,0x04,0xFF,0x7F,0x00,0x00 },
    { 0x80,0x00,0x82,0x00,0x84,0x0F,0x44,0x08,0x20,0x04,0xF0,0x3F,0x27,0x22,0x24,0x22,
      0xE4,0x3F,0x04,0x05,0x84,0x0C,0x84,0x54,0x44,0x44,0x24,0x78,0x0A,0x00,0xF1,0x7F },
    { 0xF8,0x0F,0x08,0x08,0xF8,0x0F,0x08,0x08,0xF8,0x0F,0x00,0x00,0xFC,0x3F,0x04,0x00,
      0xF4,0x1F,0x04,0x00,0xFC,0x7F,0x94,0x10,0x14,0x09,0x12,0x06,0x52,0x18,0x31,0x60 },
    { 0x80,0x00,0x80,0x00,0x80,0x00,0xFC,0x1F,0x84,0x10,0x84,0x10,0x84,0x10,0xFC,0x1F,
      0x84,0x10,0x84,0x10,0x84,0x10,0xFC,0x1F,0x84,0x50,0x80,0x40,0x80,0x40,0x00,0x7F },
    { 0x00,0x00,0xFE,0x1F,0x00,0x08,0x00,0x04,0x00,0x02,0x80,0x01,0x80,0x00,0xFF,0x7F,
      0x80,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0xA0,0x00,0x40,0x00 },
};

/* Port of the seller's display_char16_16: one 16x16 window, 32 bytes
 * streamed LSB-first, off-pixels painted white.  x = physical column
 * (0..141), y = physical row (0..427) - seller portrait coordinates. */
static void bm_seller_char16(uint16_t x, uint16_t y, uint16_t color, uint8_t idx)
{
    const uint8_t *d = bm_seller_font[idx];
    uint8_t column, tm;
    ST7789_SetWindow((uint16_t)(x + 12), y,
                     (uint16_t)(x + 15 + 12), (uint16_t)(y + 15));
    for (column = 0; column < 32; column++) {
        uint8_t temp = d[column];
        for (tm = 0; tm < 8; tm++) {
            ST7789_WritePixel((temp & 0x01) ? color : ST7789_WHITE);
            temp >>= 1;
        }
    }
}


/* Boot debug sequence: solid RED baseline -> seller demo characters
 * (16x16 window + LSB-first byte stream, seller font) -> full home page. */
static void bm_ui_dbg_seq(void)
{
    uint16_t yy, xx;

    /* Frame 1: solid red - direct flush baseline. */
    for (yy = 0; yy < TFT_H; yy++) {
        for (xx = 0; xx < TFT_W; xx++) g_line[xx] = 0xF800;
        ST7789_FlushRow(yy, g_line);
    }
    DelayMs(4000);

    /* Frame 2: seller demo characters - black background + the 7 colored
     * 16x16 Chinese chars at the seller's exact positions (portrait coords,
     * so they appear rotated on our landscape screen - we are checking
     * whether the pattern is clean or striped). */
    ST7789_Fill(ST7789_BLACK);
    bm_seller_char16(0,   160, ST7789_BLUE,  0);
    bm_seller_char16(20,  160, ST7789_GREEN, 1);
    bm_seller_char16(40,  160, ST7789_RED,   2);
    bm_seller_char16(60,  160, ST7789_BLUE,  3);
    bm_seller_char16(80,  160, ST7789_GREEN, 4);
    bm_seller_char16(100, 160, ST7789_BLUE,  5);
    bm_seller_char16(120, 160, ST7789_RED,   6);
    DelayMs(10000);

    /* Frame 3: white background + clock "14:30" drawn with the direct
     * window text renderer (same font, same layout as the home page). */
    ST7789_Fill(ST7789_WHITE);
    bm_text_direct(14, 12, "14:30", 5,
                   PALETTES[THEME_MIN_LIGHT].fg,
                   PALETTES[THEME_MIN_LIGHT].bg);
    DelayMs(6000);

    /* Frame 4: full home page (current noise reference). */
    bm_draw_page();
    DelayMs(4000);
}
#endif

static void bm_draw_page(void)
{
    if (!g_ui.ready) return;
#if BM_UI_BG_ONLY == 1
    /* Direct row test: fill g_line with RED and flush every row directly,
     * bypassing the dirty bitmap / bm_fill / bm_page_end.
     * Solid RED             => dirty-map or content renderer is the issue
     * red + black stripes   => FlushRow/panel/timing issue during UI */
    {
        uint16_t yy, xx;
        for (yy = 0; yy < TFT_H; yy++) {
            for (xx = 0; xx < TFT_W; xx++) g_line[xx] = 0xF800;
            ST7789_FlushRow(yy, g_line);
        }
    }
#else
    switch (g_ui.page) {
    case UI_PAGE_HOME:     bm_draw_home_shapes(); break;
    case UI_PAGE_CALC:     bm_draw_calc_shapes(); break;
    case UI_PAGE_SETTINGS: bm_draw_settings_shapes(); break;
    default: break;
    }
    switch (g_ui.page) {
    case UI_PAGE_HOME:     bm_draw_home_text(); break;
    case UI_PAGE_CALC:     bm_draw_calc_text(); break;
    case UI_PAGE_SETTINGS: bm_draw_settings_text(); break;
    default: break;
    }
#endif
}

/* ══════════════�?计算器（×÷ 优先�?══════════════�?*/
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
    g_ui.clock.year = (uint16_t)(t->tm_year + 1900);
    g_ui.clock.mon  = (uint8_t)(t->tm_mon + 1);
    g_ui.clock.day  = (uint8_t)t->tm_mday;
    g_ui.clock.hh   = (uint8_t)t->tm_hour;
    g_ui.clock.mm   = (uint8_t)t->tm_min;
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
    g_ui.theme = dark ? THEME_MIN_DARK : THEME_MIN_LIGHT;
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
    } else {
        size_t n;
        if (g_ui.calc.finalized) { e[0] = 0; g_ui.calc.finalized = 0; }
        n = strlen(e);
        if (n < sizeof(g_ui.calc.expr) - 1) { e[n] = key; e[n + 1] = 0; }
    }
    g_ui.dirty = 1;
}

void ui_settings_apply(uint8_t idx)
{
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
    case 2: /* 主题：六主题循环 */
        g_ui.theme = (uint8_t)((g_ui.theme + 1) % THEME_COUNT);
        break;
    case 3: /* 重置连接 */
        ui_reset_connection();
        break;
    default:
        break;
    }
    g_ui.dirty = 1;
}

uint8_t ui_get_brightness(void) { return g_ui.brightness; }

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

/* ══════════════�?裸机入口 ══════════════�?*/
void ui_bm_init(void)
{
#ifndef BM_SIM
    TMR0_TimerInit(60000);                 /* 1ms @ 60MHz */
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_EnableIRQ(TMR0_IRQn);
#endif

    bm_rtc_init();

    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.theme = THEME_MIN_LIGHT;          /* 默认极简·�?*/
    g_ui.brightness = 80;
    g_ui.battery = 86;
    bm_clock_read();
    g_ui.ready = 1;

#if BM_UI_BG_ONLY == 2
    bm_ui_dbg_seq();
#elif BM_UI_DIR_TEST_BOOT
    ui_bm_direction_test();
#else
    bm_draw_page();
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

    /* 按键/状态变更（可能来自 TMR3 ISR）→ 主循环统一重绘，避�?ISR 内长阻塞 */
    if (g_ui.dirty) {
        g_ui.dirty = 0;
        bm_draw_page();
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
        if (g_ui.page == UI_PAGE_SETTINGS) bm_draw_page();
    }
}

void ui_bm_direction_test(void)
{
    const theme_palette *p = &PALETTES[THEME_MIN_LIGHT];
    ST7789_Fill(bm_565(p->bg));
    bm_text_direct_narrow(30, 8, "TOP", 3, p->fg, p->bg);
    bm_text_direct_narrow(30, 108, "BOT", 3, p->fg, p->bg);
    bm_text_direct_narrow(50, 30, "2Pq", 8, p->fg, p->bg);
    bm_icon16_direct(210, 40, bm_icon_sun, p->muted, p->bg);
    bm_icon16_direct(250, 40, bm_icon_half, p->muted, p->bg);
    bm_icon16_direct(290, 40, bm_icon_reset, p->muted, p->bg);
}

#endif /* UI_BM_EN */
