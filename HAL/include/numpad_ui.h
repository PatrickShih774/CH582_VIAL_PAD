/**
 * @file numpad_ui.h
 * @brief 数字小键盘嵌入式 UI · LVGL v8.3 代码框架
 *        284×76 TFT 长条屏 · 双色主题 · 3 页面
 *        对应项目计划书 V2.1（2026-08-04）与 numpad-ui-preview.html
 *
 * 使用前提：
 *   - LVGL v8.3.x（与 v9 API 不兼容，请勿启用 lvgl_v9 相关宏）
 *   - 已初始化显示驱动（284×76 / RGB565）与输入驱动
 *   - 集成说明见同目录 INTEGRATION.md
 */
#ifndef NUMPAD_UI_H
#define NUMPAD_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ───────────────── 语言 / 字体开关 ─────────────────
 * UI_USE_CN_LABELS = 1：中文标签（蓝牙/亮度/休眠…），需提供 CJK 字体
 *                     （numpad_ui.c 顶部 UI_FONT_MAIN_*，默认引用 ui_font_cn_14/12）
 * UI_USE_CN_LABELS = 0：英文标签，仅用 LVGL 内置 Montserrat 即可编译运行 */
#ifndef UI_USE_CN_LABELS
/* Default: English labels — the CN fonts (ui_font_cn_14/12) require the
 * Python+PIL font generator (tools/gen_cjk_font.py).  Generate them and
 * switch this to 1 for Chinese labels. */
#define UI_USE_CN_LABELS 0
#endif

/* ───────────────── 类型定义 ───────────────── */
typedef enum {
    UI_THEME_LIGHT = 0,
    UI_THEME_DARK  = 1
} ui_theme_t;

typedef enum {
    UI_MODE_USB = 0,   /* USB */
    UI_MODE_BT,        /* 蓝牙 */
    UI_MODE_RF,        /* RF */
    UI_MODE_COUNT
} ui_mode_t;

typedef enum {
    UI_PAGE_HOME = 0,  /* 主页：时钟 + 键盘模式 */
    UI_PAGE_CALC,      /* 计算器：纯运算过程显示 */
    UI_PAGE_SETTINGS,  /* 设置：亮度 / 休眠 / 主题 / 重置连接 */
    UI_PAGE_COUNT
} ui_page_t;

/* ───────────────── 公共 API ───────────────── */

/**
 * 初始化 UI。调用时机：lv_init() + 显示/触摸驱动初始化 之后、主循环之前。
 */
void ui_init(void);

/**
 * 页面切换（底部圆点 / 物理翻页键 / 滑动识别调用）。
 * @param page UI_PAGE_HOME / UI_PAGE_CALC / UI_PAGE_SETTINGS
 */
void ui_set_page(ui_page_t page);

/**
 * 查询当前页面（供实体键/物理翻页键路由）。
 * @return UI_PAGE_HOME / UI_PAGE_CALC / UI_PAGE_SETTINGS
 */
ui_page_t ui_get_page(void);

/**
 * 主题切换（对应计划书 §6.6 全局标志 g_theme_dark → ui_theme_apply_all）。
 * @param dark UI_THEME_DARK 切深色，UI_THEME_LIGHT 切浅色
 */
void ui_set_theme(ui_theme_t dark);

/**
 * 键盘模式切换。主页右侧按钮点击后自动调用；外部（如物理模式键）也可直接调用。
 * 会更新按钮高亮并触发 ui_hook_mode_output()。
 */
void ui_set_mode(ui_mode_t mode);

/**
 * 计算器输入（实体数字小键盘驱动）。
 * 支持的字符：'0'-'9'、'+'、'-'、'*'(×)、'/'（÷）、'='（出结果）、'C'（清空）、
 *             '\b'（退格，可选）。
 */
void ui_calc_input(char key);

/* ───────────────── 设置项 ───────────────── */
uint8_t ui_get_brightness(void);          /* 0..100 */
void    ui_set_brightness(uint8_t percent);
int     ui_get_sleep_seconds(void);       /* 10 / 30 / 60 / 0(永不) */
void    ui_reset_connection(void);        /* 触发 ui_hook_reset_connection() */

/* ───────────────── 可覆盖 hook（weak，默认空实现/模拟） ───────────────── */

/**
 * 向外输出当前键盘模式标志。用户可在自己的 .c 中定义同名强符号覆盖。
 */
void ui_hook_mode_output(ui_mode_t mode);

/**
 * 执行连接重置（重连 USB/蓝牙/RF）。weak 默认空实现。
 */
void ui_hook_reset_connection(void);

/**
 * 读取 RTC 时间。weak 默认内部模拟走秒（从 14:30 起）。
 * 用户可改为硬件 RTC 读取。
 */
void ui_hook_get_rtc(int *hour, int *min, int *sec);

#ifdef __cplusplus
}
#endif

#endif /* NUMPAD_UI_H */
