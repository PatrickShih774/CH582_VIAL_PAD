/**
 * @file numpad_ui.c
 * @brief 数字小键盘嵌入式 UI · LVGL v8.3 代码框架
 *
 * 页面：
 *   Page 0 主页：左 60% 时钟+日期+右上角 WiFi/蓝牙状态图标，右 40% USB/蓝牙/RF 模式按钮
 *   Page 1 计算器：顶部过程行 + 底部 24px 大字号结果行（纯显示，实体小键盘驱动）
 *   Page 2 设置：亮度(进度条)/休眠/主题(深浅切换)/重置连接，每行左侧带图标
 * 底部：3 个页面指示圆点（可点击翻页）
 *
 * 图标：
 *   主页状态图标（WiFi/蓝牙）与「重置连接」箭头复用 montserrat 内置符号字形
 *   （LV_SYMBOL_WIFI / BLUETOOTH / REFRESH）；设置页亮度/休眠/主题为手绘
 *   12×12 ALPHA_8BIT 位图（ui_icon_*），颜色统一取自 st_icon 的 image_recolor
 *   （= muted），深浅主题切换随 muted 自动翻转。
 *
 * 主题机制（对应计划书 §6.6「方式一」，更稳健的等价实现）：
 *   全部颜色收敛到共享 lv_style_t 对象；切主题时原位重填这些样式结构体并调用
 *   lv_obj_report_style_change()，所有挂载对象自动重绘——无需 lv_obj_remove_style_all()
 *   遍历，避免清空布局样式，瞬时切换、无闪烁。
 */

#include "numpad_ui.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "config.h"   /* LVGL_EN - B0.3: 0 = legacy UI (LVGL sources stay in tree) */

#if LVGL_EN

/* ═══════════════════ 语言与字体 ═══════════════════
 * UI_USE_CN_LABELS is defined in numpad_ui.h (default 0 = English).
 * Set it to 1 there after generating ui_font_cn_14/12. */

#if UI_USE_CN_LABELS
/* 中文字体：用 LVGL 字体转换器生成含所需字符合集的 14px / 12px 字体，
 * 并声明为 lv_font_t ui_font_cn_14; / ui_font_cn_12; */
extern lv_font_t ui_font_cn_14;
extern lv_font_t ui_font_cn_12;

#define TXT_MODE_USB        "USB"
#define TXT_MODE_BT         "蓝牙"
#define TXT_MODE_RF         "RF"
#define TXT_LBL_BRIGHTNESS  "亮度"
#define TXT_LBL_SLEEP       "休眠"
#define TXT_LBL_THEME       "主题"
#define TXT_LBL_RESET       "重置连接"
#define TXT_THEME_LIGHT     "浅色"
#define TXT_THEME_DARK      "深色"
#define TXT_SLEEP_NEVER     "永不"
#define TXT_RESET_GO        "执行"
#define TXT_RESET_DONE      "已重置"
#define TXT_DATE            "2026.08.04 周二"

#define UI_FONT_MAIN_14     (&ui_font_cn_14)
#define UI_FONT_MAIN_12     (&ui_font_cn_12)
/* 规范小字号 10~10.5px；CN 字体集暂无 10px，退回 12px（需精确时生成 ui_font_cn_10 再改此宏） */
#define UI_FONT_MAIN_10     (&ui_font_cn_12)
#else
#define TXT_MODE_USB        "USB"
#define TXT_MODE_BT         "BT"
#define TXT_MODE_RF         "RF"
#define TXT_LBL_BRIGHTNESS  "Bright"
#define TXT_LBL_SLEEP       "Sleep"
#define TXT_LBL_THEME       "Theme"
#define TXT_LBL_RESET       "Reset link"
#define TXT_THEME_LIGHT     "Light"
#define TXT_THEME_DARK      "Dark"
#define TXT_SLEEP_NEVER     "Off"
#define TXT_RESET_GO        "Run"
#define TXT_RESET_DONE      "Done"
#define TXT_DATE            "2026.08.04 Tue"

#define UI_FONT_MAIN_14     (&lv_font_montserrat_14)
#define UI_FONT_MAIN_12     (&lv_font_montserrat_12)
#define UI_FONT_MAIN_10     (&lv_font_montserrat_10)
#endif

/* ═══════════════════ 跨工具链 weak ═══════════════════ */
#if defined(__GNUC__)
#define UI_WEAK __attribute__((weak))
#elif defined(__ICCARM__) || defined(__CC_ARM)
#define UI_WEAK __weak
#else
#define UI_WEAK
#endif

/* ═══════════════════ 共享样式对象（主题颜色全部收敛于此） ═══════════════════ */
static lv_style_t st_screen;        /* 页面背景 */
static lv_style_t st_card;          /* 卡片/容器 */
static lv_style_t st_btn;           /* 普通按钮 */
static lv_style_t st_btn_pressed;   /* 按下态 */
static lv_style_t st_btn_active;    /* 激活态（选中按钮） */
static lv_style_t st_label;         /* 主文字 */
static lv_style_t st_label_muted;   /* 次要文字 */
static lv_style_t st_divider;       /* 分隔线 */
static lv_style_t st_soft;          /* 计算器显示区 */
static lv_style_t st_dot;           /* 指示点（常态） */
static lv_style_t st_dot_active;    /* 指示点（当前页） */
static lv_style_t st_track;         /* 亮度条轨道 */
static lv_style_t st_fill;          /* 亮度条填充 */
static lv_style_t st_focus;         /* 焦点描边 */
static lv_style_t st_icon;          /* 位图图标着色（image_recolor → muted，随主题翻转） */

/* ═══════════════════ 控件句柄 ═══════════════════ */
static lv_obj_t * g_scr;
static lv_obj_t * g_page[UI_PAGE_COUNT];
static lv_obj_t * g_dot[UI_PAGE_COUNT];
static lv_obj_t * g_mode_btn[UI_MODE_COUNT];
static lv_obj_t * g_lbl_time;
static lv_obj_t * g_lbl_date;
static lv_obj_t * g_calc_proc;
static lv_obj_t * g_calc_result;
static lv_obj_t * g_set_bright_fill;
static lv_obj_t * g_set_bright_val;
static lv_obj_t * g_set_sleep_val;
static lv_obj_t * g_set_theme_val;
static lv_obj_t * g_set_reset_val;

/* ═══════════════════ 应用状态 ═══════════════════ */
static uint8_t g_theme_dark = UI_THEME_LIGHT;
static uint8_t g_page_cur   = UI_PAGE_HOME;
static uint8_t g_mode       = UI_MODE_USB;
static uint8_t g_brightness = 80;
static uint8_t g_sleep_idx  = 1;
static const uint16_t g_sleep_opts[4] = { 10, 30, 60, 0 };   /* 0 = 永不 */

static char    g_calc_expr[32];
static double  g_calc_value    = 0.0;   /* 与 g_calc_result(标签句柄) 区分命名 */
static uint8_t g_calc_finalized = 0;

/* ═══════════════════ 前向声明 ═══════════════════ */
static void ui_styles_init(void);
static void ui_theme_build(void);
static void ui_theme_report(void);
static void ui_obj_reset(lv_obj_t * obj);
static void ui_page_home_create(void);
static void ui_page_calc_create(void);
static void ui_page_settings_create(void);
static void ui_dots_create(void);
static void ui_dots_refresh(void);
static lv_obj_t * ui_settings_row_create(lv_obj_t * parent, const char * name, uint8_t idx, const lv_img_dsc_t * icon);
static void ui_mode_event_cb(lv_event_t * e);
static void ui_dot_event_cb(lv_event_t * e);
static void ui_settings_event_cb(lv_event_t * e);
static void ui_clock_timer_cb(lv_timer_t * t);
static void ui_reset_timer_cb(lv_timer_t * t);
static void ui_calc_refresh(void);
static double ui_calc_eval(const char * s);
static void ui_calc_fmt_expr(char * out, size_t n, const char * s);
static void ui_calc_fmt_double(char * out, size_t n, double v);
static void ui_set_brightness_refresh(void);
static void ui_set_sleep_refresh(void);

/* ═══════════════════ 位图图标（12×12 ALPHA_8BIT） ═══════════════════
 * 仅提供 alpha 蒙版，颜色统一由 st_icon 的 image_recolor（= muted）决定，
 * 深浅主题切换时随 muted 自动翻转。设置页 4 行中 3 行用位图；
 * 主页状态图标（WiFi/蓝牙）与「重置连接」箭头复用 LVGL 内置符号字形，
 * 见对应 create 函数。 */

/*
  亮度（太阳）  '#'=实 '.'=空
  ......... ...  → 逐行 12 字节（0xFF/0x00）
  .....XX.....
  ..X......X..
  ............
  ....XXXX....
  .X.XXXXXX.X.
  .X.XXXXXX.X.
  ....XXXX....
  ............
  ..X......X..
  .....XX.....
  ............
*/
static const uint8_t ui_icon_sun_data[] = {
    0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,
    0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0xFF,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0xFF,
    0xFF,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0xFF,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,
    0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,
};

/*
  休眠（表盘）  外圈 + 时针（上）/分针（右下）
  ............
  ....XXXX....
  ..XX...XX..
  ..X..X...X.
  .X...X....X.
  .X...X....X.
  .X....X...X.
  .X.....X..X.
  ..X.......X.
  ..XX...XX..
  ....XXXX....
  ............
*/
static const uint8_t ui_icon_clock_data[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0x00,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,
    0x00,0xFF,0x00,0x00,0xFF,0x00,0x00,0xFF,0x00,0x00,
    0x00,0xFF,0x00,0x00,0xFF,0x00,0x00,0x00,0xFF,0x00,
    0x00,0xFF,0x00,0x00,0xFF,0x00,0x00,0x00,0xFF,0x00,
    0x00,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,0xFF,0x00,
    0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,
    0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,
    0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,
};

/*
  主题（弯月）  左凸右凹的月牙
  ............
  ....XXXX....
  ...XXXXXX...
  ..XXX.......
  .XXX........
  .XXX........
  .XXX........
  .XXX........
  ..XX........
  ...XX.......
  ....XXXX....
  ............
*/
static const uint8_t ui_icon_crescent_data[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static const lv_img_dsc_t ui_icon_sun = {
    .header.cf = LV_IMG_CF_ALPHA_8BIT, .header.always_zero = 0, .header.reserved = 0,
    .header.w = 10, .header.h = 10,
    .data_size = sizeof(ui_icon_sun_data), .data = ui_icon_sun_data,
};

static const lv_img_dsc_t ui_icon_clock = {
    .header.cf = LV_IMG_CF_ALPHA_8BIT, .header.always_zero = 0, .header.reserved = 0,
    .header.w = 10, .header.h = 10,
    .data_size = sizeof(ui_icon_clock_data), .data = ui_icon_clock_data,
};

static const lv_img_dsc_t ui_icon_crescent = {
    .header.cf = LV_IMG_CF_ALPHA_8BIT, .header.always_zero = 0, .header.reserved = 0,
    .header.w = 10, .header.h = 10,
    .data_size = sizeof(ui_icon_crescent_data), .data = ui_icon_crescent_data,
};

/* ═══════════════════ 主题 ═══════════════════ */

static void ui_styles_init(void)
{
    lv_style_init(&st_screen);
    lv_style_init(&st_card);
    lv_style_init(&st_btn);
    lv_style_init(&st_btn_pressed);
    lv_style_init(&st_btn_active);
    lv_style_init(&st_label);
    lv_style_init(&st_label_muted);
    lv_style_init(&st_divider);
    lv_style_init(&st_soft);
    lv_style_init(&st_dot);
    lv_style_init(&st_dot_active);
    lv_style_init(&st_track);
    lv_style_init(&st_fill);
    lv_style_init(&st_focus);
    lv_style_init(&st_icon);
}

/** 依据 g_theme_dark 重填全部共享样式（计划书 §5 双色 token 逐值映射） */
static void ui_theme_build(void)
{
    const uint8_t dark = g_theme_dark;
    lv_color_t bg       = lv_color_hex(dark ? 0x121212u : 0xFFFFFFu);
    lv_color_t card     = lv_color_hex(dark ? 0x1E1E1Eu : 0xFFFFFFu);
    lv_color_t border   = lv_color_hex(dark ? 0x333333u : 0xE8E8E8u);
    lv_color_t fg       = lv_color_hex(dark ? 0xEEEEEEu : 0x1A1A1Au);
    lv_color_t muted    = lv_color_hex(dark ? 0x888888u : 0x999999u);
    lv_color_t pressed  = lv_color_hex(dark ? 0x2A2A2Au : 0xF5F5F5u);
    lv_color_t active   = lv_color_hex(dark ? 0xFFFFFFu : 0x333333u);
    lv_color_t active_fg= lv_color_hex(dark ? 0x121212u : 0xFFFFFFu);
    lv_color_t soft     = lv_color_hex(dark ? 0x1A1A1Au : 0xFAFAFAu);

    /* 页面背景 */
    lv_style_set_bg_color(&st_screen, bg);
    lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);

    /* 卡片 / 容器 */
    lv_style_set_bg_color(&st_card, card);
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_border_color(&st_card, border);
    lv_style_set_border_width(&st_card, 1);
    lv_style_set_radius(&st_card, 4);

    /* 普通按钮 */
    lv_style_set_bg_color(&st_btn, card);
    lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
    lv_style_set_border_color(&st_btn, border);
    lv_style_set_border_width(&st_btn, 1);
    lv_style_set_radius(&st_btn, 6);
    lv_style_set_text_color(&st_btn, fg);

    /* 按下态 */
    lv_style_set_bg_color(&st_btn_pressed, pressed);
    lv_style_set_bg_opa(&st_btn_pressed, LV_OPA_COVER);

    /* 激活态（选中） */
    lv_style_set_bg_color(&st_btn_active, active);
    lv_style_set_bg_opa(&st_btn_active, LV_OPA_COVER);
    lv_style_set_text_color(&st_btn_active, active_fg);
    lv_style_set_border_color(&st_btn_active, active);

    /* 文字 */
    lv_style_set_text_color(&st_label, fg);
    lv_style_set_text_color(&st_label_muted, muted);

    /* 分隔线 */
    lv_style_set_border_color(&st_divider, border);
    lv_style_set_border_width(&st_divider, 1);

    /* 计算器显示区 */
    lv_style_set_bg_color(&st_soft, soft);
    lv_style_set_bg_opa(&st_soft, LV_OPA_COVER);
    lv_style_set_border_color(&st_soft, border);
    lv_style_set_border_width(&st_soft, 1);
    lv_style_set_radius(&st_soft, 4);

    /* 指示点 */
    lv_style_set_bg_color(&st_dot, border);
    lv_style_set_bg_opa(&st_dot, LV_OPA_COVER);
    lv_style_set_radius(&st_dot, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&st_dot_active, fg);
    lv_style_set_bg_opa(&st_dot_active, LV_OPA_COVER);
    lv_style_set_radius(&st_dot_active, LV_RADIUS_CIRCLE);

    /* 亮度条 */
    lv_style_set_bg_color(&st_track, border);
    lv_style_set_bg_opa(&st_track, LV_OPA_COVER);
    lv_style_set_radius(&st_track, 2);
    lv_style_set_bg_color(&st_fill, fg);
    lv_style_set_bg_opa(&st_fill, LV_OPA_COVER);
    lv_style_set_radius(&st_fill, 2);

    /* 焦点描边 */
    lv_style_set_outline_width(&st_focus, 1);
    lv_style_set_outline_color(&st_focus, active);
    lv_style_set_outline_pad(&st_focus, 1);

    /* 位图图标：以 muted 着色（ALPHA_8BIT 图像仅提供 alpha 蒙版，颜色全部取自 recolor） */
    lv_style_set_img_recolor(&st_icon, muted);
    lv_style_set_img_recolor_opa(&st_icon, LV_OPA_COVER);
}

/** 通知所有挂载对象：样式已变更，请重绘 */
static void ui_theme_report(void)
{
    lv_obj_report_style_change(&st_screen);
    lv_obj_report_style_change(&st_card);
    lv_obj_report_style_change(&st_btn);
    lv_obj_report_style_change(&st_btn_pressed);
    lv_obj_report_style_change(&st_btn_active);
    lv_obj_report_style_change(&st_label);
    lv_obj_report_style_change(&st_label_muted);
    lv_obj_report_style_change(&st_divider);
    lv_obj_report_style_change(&st_soft);
    lv_obj_report_style_change(&st_dot);
    lv_obj_report_style_change(&st_dot_active);
    lv_obj_report_style_change(&st_track);
    lv_obj_report_style_change(&st_fill);
    lv_obj_report_style_change(&st_focus);
    lv_obj_report_style_change(&st_icon);
}

/** 对应计划书 §6.6：ui_theme_apply_all() */
void ui_theme_apply_all(void)
{
    ui_theme_build();
    ui_theme_report();
}

/* ═══════════════════ 工具函数 ═══════════════════ */

/** 移除对象默认样式并禁用滚动，便于像素级控制 */
static void ui_obj_reset(lv_obj_t * obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

/* ═══════════════════ 主页：时钟 + 模式 ═══════════════════ */

static void ui_page_home_create(void)
{
    lv_obj_t * page = lv_obj_create(g_scr);
    ui_obj_reset(page);
    lv_obj_set_size(page, 284, 76);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_add_style(page, &st_screen, 0);
    g_page[UI_PAGE_HOME] = page;

    /* 左区 60%：时钟 + 日期 */
    lv_obj_t * left = lv_obj_create(page);
    ui_obj_reset(left);
    lv_obj_set_size(left, 170, 76);
    lv_obj_set_pos(left, 0, 0);
    lv_obj_add_style(left, &st_screen, 0);

    g_lbl_time = lv_label_create(left);
    lv_obj_add_style(g_lbl_time, &st_label, 0);
    lv_obj_set_style_text_font(g_lbl_time, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(g_lbl_time, 8, 7);
    lv_label_set_text(g_lbl_time, "14:30");

    g_lbl_date = lv_label_create(left);
    lv_obj_add_style(g_lbl_date, &st_label_muted, 0);
    /* 规范 .home-date：font-size 10px · bottom 8 → y = 76 - 8 - 10 */
    lv_obj_set_style_text_font(g_lbl_date, UI_FONT_MAIN_10, 0);
    lv_obj_set_pos(g_lbl_date, 8, 76 - 8 - 10);
    lv_label_set_text(g_lbl_date, TXT_DATE);

    /* 右上角状态图标：WiFi + 蓝牙信号（montserrat 内置符号字形，取色 muted）。
     * 与左区（170px）右缘对齐，design 对应 .home-status（top:6 right:5）。 */
    lv_obj_t * status = lv_obj_create(left);
    ui_obj_reset(status);
    lv_obj_set_size(status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status, 3, 0);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -5, 6);   /* 规范 .home-status：top 6 · right 5 */

    lv_obj_t * st_wifi = lv_label_create(status);
    lv_obj_add_style(st_wifi, &st_label_muted, 0);
    lv_obj_set_style_text_font(st_wifi, &lv_font_montserrat_10, 0);   /* 规范图标 11px → 最接近 10px */
    lv_label_set_text(st_wifi, LV_SYMBOL_WIFI);

    lv_obj_t * st_bt = lv_label_create(status);
    lv_obj_add_style(st_bt, &st_label_muted, 0);
    lv_obj_set_style_text_font(st_bt, &lv_font_montserrat_10, 0);
    lv_label_set_text(st_bt, LV_SYMBOL_BLUETOOTH);

    /* 右区 40%：三个模式按钮（竖排 20px/个，间隙 4px） */
    lv_obj_t * right = lv_obj_create(page);
    ui_obj_reset(right);
    lv_obj_set_size(right, 114, 76);
    lv_obj_set_pos(right, 170, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(right, 4, 0);
    lv_obj_set_style_pad_row(right, 4, 0);

    static const char * mode_names[UI_MODE_COUNT] = {
        TXT_MODE_USB, TXT_MODE_BT, TXT_MODE_RF
    };
    for (uint8_t i = 0; i < UI_MODE_COUNT; i++) {
        lv_obj_t * b = lv_btn_create(right);
        lv_obj_set_size(b, LV_PCT(100), 20);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_add_style(b, &st_btn, 0);
        lv_obj_add_style(b, &st_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(b, &st_focus, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_text_font(b, UI_FONT_MAIN_10, 0);   /* 规范 11px → 最接近内置 10px */
        lv_obj_add_event_cb(b, ui_mode_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t * l = lv_label_create(b);
        lv_label_set_text(l, mode_names[i]);
        lv_obj_center(l);

        g_mode_btn[i] = b;
    }

    /* 默认 USB 为激活态 */
    lv_obj_add_style(g_mode_btn[UI_MODE_USB], &st_btn_active, 0);
}

/* ═══════════════════ 计算器：纯运算过程显示 ═══════════════════ */

static void ui_page_calc_create(void)
{
    lv_obj_t * page = lv_obj_create(g_scr);
    ui_obj_reset(page);
    lv_obj_set_size(page, 284, 76);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_add_style(page, &st_screen, 0);
    g_page[UI_PAGE_CALC] = page;

    /* 显示区：top 3 / bottom 8（避开底部指示点）→ 65px 高 */
    lv_obj_t * box = lv_obj_create(page);
    ui_obj_reset(box);
    lv_obj_set_size(box, 276, 65);
    lv_obj_set_pos(box, 4, 3);
    lv_obj_add_style(box, &st_soft, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 过程行：右对齐小字 + 底部细分隔线 */
    g_calc_proc = lv_label_create(box);
    lv_obj_add_style(g_calc_proc, &st_label_muted, 0);
    lv_obj_add_style(g_calc_proc, &st_divider, 0);
    lv_obj_set_style_border_side(g_calc_proc, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_text_font(g_calc_proc, UI_FONT_MAIN_10, 0);   /* 规范 11px → 最接近 10px */
    lv_obj_set_style_text_align(g_calc_proc, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(g_calc_proc, LV_PCT(100));
    lv_obj_set_height(g_calc_proc, 24);
    lv_label_set_long_mode(g_calc_proc, LV_LABEL_LONG_CLIP);
    lv_label_set_text(g_calc_proc, "");

    /* 结果行：24px 大字号右对齐 */
    g_calc_result = lv_label_create(box);
    lv_obj_add_style(g_calc_result, &st_label, 0);
    lv_obj_set_style_text_font(g_calc_result, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(g_calc_result, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(g_calc_result, LV_PCT(100));
    lv_obj_set_style_pad_top(g_calc_result, 2, 0);
    lv_obj_set_style_pad_bottom(g_calc_result, 5, 0);
    lv_obj_set_flex_grow(g_calc_result, 1);
    lv_label_set_long_mode(g_calc_result, LV_LABEL_LONG_CLIP);
    lv_label_set_text(g_calc_result, "0");
}

/* ═══════════════════ 设置页 ═══════════════════ */

/** 创建一行设置项（左侧图标 + 名称，右侧值区），返回行对象。
 *  @param icon 位图图标 dsc；为 NULL 时退化为 LVGL 内置符号字形（重置连接 → 刷新箭头） */
static lv_obj_t * ui_settings_row_create(lv_obj_t * parent, const char * name, uint8_t idx, const lv_img_dsc_t * icon)
{
    lv_obj_t * row = lv_obj_create(parent);
    ui_obj_reset(row);
    lv_obj_set_flex_grow(row, 1);
    /* Row must span the full list width — otherwise flex column with the
     * default cross-axis CENTER shrinks it to content width and all 4 rows
     * pile in the middle with left/right groups overlapping.  Matches
     * Reference/numpad-ui-preview.html .setting-row { width: 100% }. */
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    /* SPACE_BETWEEN: left group hugs left edge, right group hugs right
     * edge (flex-grow alone leaves the right group mid-row).  The left
     * group's own content is START-aligned (below) so the icon sits at
     * the row edge.  Matches preview .setting-row + margin-left:auto. */
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(row, 5, 0);
    lv_obj_set_style_pad_right(row, 5, 0);
    lv_obj_set_style_pad_top(row, 1, 0);
    lv_obj_set_style_pad_bottom(row, 1, 0);
    lv_obj_add_style(row, &st_divider, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(row, &st_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(row, &st_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(row, ui_settings_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    /* 左组：图标 + 名称 */
    lv_obj_t * lg = lv_obj_create(row);
    ui_obj_reset(lg);
    lv_obj_set_flex_flow(lg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lg, 4, 0);
    /* NOTE: no flex-grow here — with SPACE_BETWEEN the left group hugs the
     * left edge and the right group hugs the right edge.  Adding grow made
     * the left group absorb the middle and pushed the right group off the
     * right edge (chev stopped at x~221 instead of x~270). */
    lv_obj_set_height(lg, 12);            /* 12px icon / 11px line-height, fixed */

    if (icon != NULL) {
        lv_obj_t * ic = lv_img_create(lg);
        lv_obj_add_style(ic, &st_icon, 0);
        lv_img_set_src(ic, icon);
    } else {
        lv_obj_t * ic = lv_label_create(lg);
        lv_obj_add_style(ic, &st_label_muted, 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_10, 0);   /* 规范图标 11px → 最接近 10px */
        lv_label_set_long_mode(ic, LV_LABEL_LONG_CLIP);   /* symbol must not wrap/grow the row */
        lv_label_set_text(ic, "R");   /* ASCII "R" — FontAwesome refresh glyph mis-renders at 10px */
    }

    lv_obj_t * lbl = lv_label_create(lg);
    lv_obj_add_style(lbl, &st_label, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_MAIN_10, 0);   /* 规范标签 10.5px → 最接近 10px */
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);       /* no wrap → no height growth */
    lv_label_set_text(lbl, name);
    return row;
}

/** 右侧值区容器（右对齐：值 + 箭头/进度条） */
static lv_obj_t * ui_settings_right_group(lv_obj_t * row)
{
    lv_obj_t * rg = lv_obj_create(row);
    ui_obj_reset(rg);
    lv_obj_set_flex_flow(rg, LV_FLEX_FLOW_ROW);
    /* Grow to fill the rest of the row + END-align contents: the value and
     * chevron hug the right edge (matches preview margin-left:auto).  With
     * SPACE_BETWEEN alone LVGL 8.3 left the right group mid-row. */
    lv_obj_set_flex_grow(rg, 1);
    lv_obj_set_flex_align(rg, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rg, 4, 0);
    lv_obj_set_height(rg, 12);            /* match left group, fixed height */
    return rg;
}

static void ui_page_settings_create(void)
{
    lv_obj_t * page = lv_obj_create(g_scr);
    ui_obj_reset(page);
    lv_obj_set_size(page, 284, 76);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_add_style(page, &st_screen, 0);
    g_page[UI_PAGE_SETTINGS] = page;

    lv_obj_t * list = lv_obj_create(page);
    ui_obj_reset(list);
    lv_obj_set_size(list, 276, 64);   /* leave room for nav dots at y69 */
    lv_obj_set_pos(list, 4, 2);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    /* 规范 .settings-list 无 gap：4 行 flex:1 平分 65px = 16.25px/行（加 pad_row 会把行压到 14.75px 导致文字拥挤重叠） */

    /* 1) 亮度 */
    lv_obj_t * row = ui_settings_row_create(list, TXT_LBL_BRIGHTNESS, 0, &ui_icon_sun);
    lv_obj_t * rg  = ui_settings_right_group(row);
    lv_obj_t * track = lv_obj_create(rg);
    ui_obj_reset(track);
    lv_obj_set_size(track, 36, 4);
    lv_obj_add_style(track, &st_track, 0);
    g_set_bright_fill = lv_obj_create(track);
    ui_obj_reset(g_set_bright_fill);
    lv_obj_set_pos(g_set_bright_fill, 1, 1);
    lv_obj_set_size(g_set_bright_fill, 27, 2);
    lv_obj_add_style(g_set_bright_fill, &st_fill, 0);

    g_set_bright_val = lv_label_create(rg);
    lv_obj_add_style(g_set_bright_val, &st_label_muted, 0);
    lv_obj_set_style_text_font(g_set_bright_val, UI_FONT_MAIN_10, 0);   /* 规范值 10px */
    lv_label_set_text(g_set_bright_val, "80%");

    /* 2) 休眠 */
    row = ui_settings_row_create(list, TXT_LBL_SLEEP, 1, &ui_icon_clock);
    rg  = ui_settings_right_group(row);
    g_set_sleep_val = lv_label_create(rg);
    lv_obj_add_style(g_set_sleep_val, &st_label_muted, 0);
    lv_obj_set_style_text_font(g_set_sleep_val, UI_FONT_MAIN_10, 0);   /* 规范值 10px */
    lv_label_set_text(g_set_sleep_val, "");   /* 由 ui_set_sleep_refresh() 填充 */
    lv_obj_t * chev = lv_label_create(rg);
    lv_obj_add_style(chev, &st_label_muted, 0);
    lv_obj_set_style_text_font(chev, UI_FONT_MAIN_10, 0);   /* 规范 › → LV_SYMBOL_RIGHT（可渲染近似） */
    lv_label_set_text(chev, ">");   /* ASCII ">" — FontAwesome right glyph mis-renders at 10px */

    /* 3) 主题 */
    row = ui_settings_row_create(list, TXT_LBL_THEME, 2, &ui_icon_crescent);
    rg  = ui_settings_right_group(row);
    g_set_theme_val = lv_label_create(rg);
    lv_obj_add_style(g_set_theme_val, &st_label_muted, 0);
    lv_obj_set_style_text_font(g_set_theme_val, UI_FONT_MAIN_10, 0);   /* 规范值 10px */
    lv_label_set_text(g_set_theme_val, TXT_THEME_LIGHT);
    chev = lv_label_create(rg);
    lv_obj_add_style(chev, &st_label_muted, 0);
    lv_obj_set_style_text_font(chev, UI_FONT_MAIN_10, 0);
    lv_label_set_text(chev, ">");   /* ASCII ">" — FontAwesome right glyph mis-renders at 10px */

    /* 4) 重置连接（无位图 → 内置刷新箭头符号） */
    row = ui_settings_row_create(list, TXT_LBL_RESET, 3, NULL);
    rg  = ui_settings_right_group(row);
    g_set_reset_val = lv_label_create(rg);
    lv_obj_add_style(g_set_reset_val, &st_label_muted, 0);
    lv_obj_set_style_text_font(g_set_reset_val, UI_FONT_MAIN_10, 0);   /* 规范值 10px */
    lv_label_set_text(g_set_reset_val, TXT_RESET_GO);
    chev = lv_label_create(rg);
    lv_obj_add_style(chev, &st_label_muted, 0);
    lv_obj_set_style_text_font(chev, UI_FONT_MAIN_10, 0);
    lv_label_set_text(chev, ">");   /* ASCII ">" — FontAwesome right glyph mis-renders at 10px */
    /* 最后一行去掉分隔线 */
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_NONE, 0);
}

/* ═══════════════════ 底部指示点 ═══════════════════ */

static void ui_dots_create(void)
{
    for (uint8_t i = 0; i < UI_PAGE_COUNT; i++) {
        g_dot[i] = lv_obj_create(g_scr);
        ui_obj_reset(g_dot[i]);
        lv_obj_set_size(g_dot[i], 4, 4);
        lv_obj_set_pos(g_dot[i], 131 + i * 9, 69);
        lv_obj_add_style(g_dot[i], &st_dot, 0);
        lv_obj_add_flag(g_dot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_dot[i], ui_dot_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    ui_dots_refresh();
}

static void ui_dots_refresh(void)
{
    for (uint8_t i = 0; i < UI_PAGE_COUNT; i++) {
        lv_obj_remove_style(g_dot[i], &st_dot_active, 0);
        if (i == g_page_cur) {
            lv_obj_add_style(g_dot[i], &st_dot_active, 0);
        }
    }
}

/* ═══════════════════ 页面与主题切换 ═══════════════════ */

void ui_set_page(ui_page_t page)
{
    if (page >= UI_PAGE_COUNT) return;
    g_page_cur = (uint8_t)page;
    for (uint8_t i = 0; i < UI_PAGE_COUNT; i++) {
        lv_obj_add_flag(g_page[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(g_page[page], LV_OBJ_FLAG_HIDDEN);
    ui_dots_refresh();
}

ui_page_t ui_get_page(void)
{
    return (ui_page_t)g_page_cur;
}

void ui_set_theme(ui_theme_t dark)
{
    g_theme_dark = (uint8_t)dark;
    ui_theme_apply_all();
    if (g_set_theme_val != NULL) {
        lv_label_set_text(g_set_theme_val, g_theme_dark ? TXT_THEME_DARK : TXT_THEME_LIGHT);
    }
}

/* ═══════════════════ 键盘模式 ═══════════════════ */

void ui_set_mode(ui_mode_t mode)
{
    if (mode >= UI_MODE_COUNT) return;
    g_mode = (uint8_t)mode;
    for (uint8_t i = 0; i < UI_MODE_COUNT; i++) {
        lv_obj_remove_style(g_mode_btn[i], &st_btn_active, 0);
    }
    lv_obj_add_style(g_mode_btn[mode], &st_btn_active, 0);
    ui_hook_mode_output(mode);
}

static void ui_mode_event_cb(lv_event_t * e)
{
    ui_mode_t m = (ui_mode_t)(intptr_t)lv_event_get_user_data(e);
    ui_set_mode(m);
}

static void ui_dot_event_cb(lv_event_t * e)
{
    ui_page_t p = (ui_page_t)(intptr_t)lv_event_get_user_data(e);
    ui_set_page(p);
}

/* ═══════════════════ 计算器（实体小键盘驱动，纯显示） ═══════════════════ */

/** 表达式求值：仅数字与 + - × ÷，乘除优先、从左到右 */
static double ui_calc_eval(const char * s)
{
    double result  = 0.0;
    double current = 0.0;
    double num     = 0.0;
    char   last_op = '+';
    uint8_t started = 0;

    while (*s != '\0') {
        char c = *s;
        if (c >= '0' && c <= '9') {
            num = num * 10.0 + (double)(c - '0');
            started = 1;
        } else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (!started) num = 0.0;
            switch (last_op) {
                case '+': result += current; current = num; break;
                case '-': result += current; current = -num; break;
                case '*': current *= num; break;
                case '/': if (num == 0.0) return 0.0 / 0.0; current /= num; break;
                default: break;
            }
            last_op = c;
            num = 0.0;
            started = 0;
        }
        s++;
    }
    if (!started) num = 0.0;
    switch (last_op) {
        case '+': result += current; current = num; break;
        case '-': result += current; current = -num; break;
        case '*': current *= num; break;
        case '/': if (num == 0.0) return 0.0 / 0.0; current /= num; break;
        default: break;
    }
    return result + current;
}

/** 显示：运算符两侧加空格，如 128+64*5 → 128 + 64 × 5。
 *  内部表达式统一存 ASCII '*'、'/'，这里转成 ×(U+00D7)、÷(U+00F7) 便于阅读；
 *  注意内置 Montserrat 无这两个字形（需自定义字体），此处输出合法 UTF-8 两字节。 */
static void ui_calc_fmt_expr(char * out, size_t n, const char * s)
{
    static const char opr_mul[] = { '*' };                       /* Montserrat lacks U+00D7 */
    static const char opr_div[] = { '/' };                       /* Montserrat lacks U+00F7 */
    size_t oi = 0;
    const char * p = s;
    while (*p != '\0' && oi + 3 < n) {
        char c = *p;
        const char * disp = (c == '*') ? opr_mul : (c == '/') ? opr_div : NULL;
        int is_op = (c == '+' || c == '-' || c == '*' || c == '/');
        if (is_op && oi > 0 && out[oi - 1] != ' ') out[oi++] = ' ';
        if (disp != NULL) { out[oi++] = disp[0]; }
        else out[oi++] = c;
        if (is_op && oi + 1 < n) out[oi++] = ' ';
        p++;
    }
    out[oi] = '\0';
}

static void ui_calc_fmt_double(char * out, size_t n, double v)
{
    if (v != v) {                    /* NaN */
        if (n > 3) { out[0] = 'E'; out[1] = 'r'; out[2] = 'r'; out[3] = '\0'; }
        return;
    }
    if (v == 0.0) v = 0.0;           /* 消除 -0 */
    if (v > 1e12 || v < -1e12 || (v != 0.0 && v < 1e-9)) {
        lv_snprintf(out, n, "%.4e", v);
    } else {
        lv_snprintf(out, n, "%.8g", v);
    }
}

static void ui_calc_refresh(void)
{
    if (g_calc_proc == NULL || g_calc_result == NULL) return;
    char expr[48];
    char res[16];
    ui_calc_fmt_expr(expr, sizeof(expr), g_calc_expr);
    lv_label_set_text(g_calc_proc, expr);
    double v = g_calc_finalized ? g_calc_value : ui_calc_eval(g_calc_expr);
    ui_calc_fmt_double(res, sizeof(res), v);
    lv_label_set_text(g_calc_result, res);
}

void ui_calc_input(char key)
{
    /* 内部统一存 ASCII 运算符（'*'、'/'）；显示层由 ui_calc_fmt_expr 转成 ×、÷ */
    char op = key;

    if (key == 'C') {
        g_calc_expr[0] = '\0';
        g_calc_value = 0.0;
        g_calc_finalized = 0;
    } else if (key == '\b') {
        if (g_calc_finalized) {
            g_calc_expr[0] = '\0';
            g_calc_value = 0.0;
            g_calc_finalized = 0;
        } else {
            size_t len = strlen(g_calc_expr);
            if (len > 0) g_calc_expr[len - 1] = '\0';
        }
    } else if (key == '=') {
        size_t n = strlen(g_calc_expr);
        while (n > 0 && (g_calc_expr[n - 1] == '+' || g_calc_expr[n - 1] == '-' ||
                         g_calc_expr[n - 1] == '*' || g_calc_expr[n - 1] == '/')) {
            g_calc_expr[--n] = '\0';
        }
        if (n == 0) return;
        g_calc_value = ui_calc_eval(g_calc_expr);
        g_calc_finalized = 1;
    } else if (op == '+' || op == '-' || op == '*' || op == '/') {
        if (g_calc_finalized) {
            ui_calc_fmt_double(g_calc_expr, sizeof(g_calc_expr), g_calc_value);
            size_t n = strlen(g_calc_expr);
            if (n + 1 < sizeof(g_calc_expr)) { g_calc_expr[n] = op; g_calc_expr[n + 1] = '\0'; }
            g_calc_finalized = 0;
        } else {
            size_t n = strlen(g_calc_expr);
            if (n == 0) {
                if (n + 2 < sizeof(g_calc_expr)) { g_calc_expr[0] = '0'; g_calc_expr[1] = op; g_calc_expr[2] = '\0'; }
            } else if (n + 1 < sizeof(g_calc_expr)) {
                char last = g_calc_expr[n - 1];
                if (last == '+' || last == '-' || last == '*' || last == '/') {
                    g_calc_expr[n - 1] = op;             /* 连续运算符：替换 */
                } else {
                    g_calc_expr[n] = op; g_calc_expr[n + 1] = '\0';
                }
            }
        }
    } else {  /* 数字 0-9 */
        if (g_calc_finalized) {
            g_calc_expr[0] = key; g_calc_expr[1] = '\0';
            g_calc_finalized = 0;
        } else {
            size_t n = strlen(g_calc_expr);
            if (n == 0) { g_calc_expr[0] = key; g_calc_expr[1] = '\0'; }
            else if (n == 1 && g_calc_expr[0] == '0') { g_calc_expr[0] = key; }
            else if (n + 1 < sizeof(g_calc_expr)) { g_calc_expr[n] = key; g_calc_expr[n + 1] = '\0'; }
        }
    }
    ui_calc_refresh();
}

/* ═══════════════════ 设置项 ═══════════════════ */

static void ui_set_brightness_refresh(void)
{
    if (g_set_bright_fill == NULL) return;
    uint32_t w = (34u * g_brightness) / 100u;
    if (w < 1) w = 1;
    lv_obj_set_width(g_set_bright_fill, w);
    if (g_set_bright_val != NULL) lv_label_set_text_fmt(g_set_bright_val, "%d%%", (int)g_brightness);
}

static void ui_set_sleep_refresh(void)
{
    if (g_set_sleep_val == NULL) return;
    uint16_t s = g_sleep_opts[g_sleep_idx];
    if (s == 0) lv_label_set_text(g_set_sleep_val, TXT_SLEEP_NEVER);
    else {
#if UI_USE_CN_LABELS
        lv_label_set_text_fmt(g_set_sleep_val, "%d秒", (int)s);
#else
        lv_label_set_text_fmt(g_set_sleep_val, "%ds", (int)s);
#endif
    }
}

uint8_t ui_get_brightness(void) { return g_brightness; }

void ui_set_brightness(uint8_t percent)
{
    if (percent > 100) percent = 100;
    g_brightness = percent;
    ui_set_brightness_refresh();
}

int ui_get_sleep_seconds(void) { return (int)g_sleep_opts[g_sleep_idx]; }

void ui_reset_connection(void)
{
    ui_hook_reset_connection();
    if (g_set_reset_val != NULL) lv_label_set_text(g_set_reset_val, TXT_RESET_DONE);
    lv_timer_t * t = lv_timer_create(ui_reset_timer_cb, 900, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void ui_reset_timer_cb(lv_timer_t * t)
{
    (void)t;
    if (g_set_reset_val != NULL) lv_label_set_text(g_set_reset_val, TXT_RESET_GO);
}

static void ui_settings_event_cb(lv_event_t * e)
{
    uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    ui_settings_apply(idx);
}

/** 设置页行操作（实体键与点击共用同一逻辑） */
void ui_settings_apply(uint8_t idx)
{
    switch (idx) {
        case 0:  /* 亮度：步进 +20，100 回卷到 20 */
            g_brightness = (g_brightness >= 100) ? 20 : g_brightness + 20;
            ui_set_brightness_refresh();
            break;
        case 1:  /* 休眠：循环 10s/30s/60s/永不 */
            g_sleep_idx = (g_sleep_idx + 1) % 4;
            ui_set_sleep_refresh();
            break;
        case 2:  /* 主题：即时全局切换 */
            ui_set_theme((ui_theme_t)!g_theme_dark);
            break;
        case 3:  /* 重置连接 */
            ui_reset_connection();
            break;
        default:
            break;
    }
}

/* ═══════════════════ 时钟 ═══════════════════ */

UI_WEAK void ui_hook_mode_output(ui_mode_t mode) { (void)mode; }
UI_WEAK void ui_hook_reset_connection(void) { }
UI_WEAK void ui_hook_get_rtc(int *hour, int *min, int *sec)
{
    static int hh = 14, mm = 30, ss = 0;   /* 模拟 RTC，从 14:30 起走秒 */
    ss++;
    if (ss >= 60) { ss = 0; mm++; if (mm >= 60) { mm = 0; hh = (hh + 1) % 24; } }
    *hour = hh; *min = mm; *sec = ss;
}

static void ui_clock_timer_cb(lv_timer_t * t)
{
    (void)t;
    int h = 0, m = 0, s = 0;
    ui_hook_get_rtc(&h, &m, &s);
    if (g_lbl_time != NULL) lv_label_set_text_fmt(g_lbl_time, "%02d:%02d", h, m);
    /* 日期：模拟固定值；接入硬件 RTC 后由用户在此更新 */
    if (g_lbl_date != NULL) lv_label_set_text(g_lbl_date, TXT_DATE);
}

/* ═══════════════════ 初始化 ═══════════════════ */

void ui_init(void)
{
    /* 样式必须先初始化再挂到对象上 */
    ui_styles_init();
    ui_theme_build();

    g_scr = lv_scr_act();
    lv_obj_remove_style_all(g_scr);
    lv_obj_add_style(g_scr, &st_screen, 0);

    ui_page_home_create();
    ui_page_calc_create();
    ui_page_settings_create();
    ui_dots_create();

    ui_theme_report();

    ui_calc_refresh();
    ui_set_brightness_refresh();
    ui_set_sleep_refresh();
    if (g_set_theme_val != NULL) lv_label_set_text(g_set_theme_val, TXT_THEME_LIGHT);
    if (g_set_reset_val != NULL) lv_label_set_text(g_set_reset_val, TXT_RESET_GO);

    ui_set_page(UI_PAGE_HOME);

    lv_timer_create(ui_clock_timer_cb, 1000, NULL);
}

#else /* LVGL_EN == 0 - B0.3 keyboard-first: legacy UI only.
       * Stubs keep USB_MODE.c / scan_key.c linkable. */

void ui_init(void) {}
void ui_set_page(ui_page_t page) { (void)page; }
ui_page_t ui_get_page(void) { return UI_PAGE_HOME; }
void ui_set_theme(ui_theme_t dark) { (void)dark; }
void ui_set_mode(ui_mode_t mode) { (void)mode; }
void ui_calc_input(char key) { (void)key; }
void ui_settings_apply(uint8_t idx) { (void)idx; }
uint8_t ui_get_brightness(void) { return 100; }
void ui_set_brightness(uint8_t percent) { (void)percent; }
int ui_get_sleep_seconds(void) { return 0; }
void ui_reset_connection(void) {}
void ui_hook_mode_output(ui_mode_t mode) { (void)mode; }
void ui_hook_reset_connection(void) {}
void ui_hook_get_rtc(int *hour, int *min, int *sec) { if (hour) *hour = 0; if (min) *min = 0; if (sec) *sec = 0; }

#endif /* LVGL_EN */
