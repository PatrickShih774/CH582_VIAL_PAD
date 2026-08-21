/**
 * bm_ui.h — 裸机 UI 公共 API（无 LVGL）
 *
 * 与 LVGL 版 numpad_ui.h 保持同名同签名，USB_MODE.c 按键路由零改动；
 * 设计规范：C:\ClaudeProject\tft_NV3007\brand-spec.md（六主题 / 三页面 / 428×142）。
 * 驱动复用 HAL/NV3007.c（API 统一为 NV3007_*，逻辑横屏 428×142）。
 */
#ifndef BM_UI_H
#define BM_UI_H

#include <stdint.h>

/* ---- 与 numpad_ui.h 兼容的类型 ---- */
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

/* ---- 公共 API（与 numpad_ui.h 同名） ---- */
void ui_init(void);
void ui_set_page(ui_page_t page);
ui_page_t ui_get_page(void);
void ui_set_theme(ui_theme_t dark);
void ui_set_mode(ui_mode_t mode);
void ui_calc_input(char key);
void ui_settings_apply(uint8_t idx);
uint8_t ui_get_brightness(void);
uint8_t ui_get_theme(void);
void ui_set_brightness(uint8_t percent);
int ui_get_sleep_seconds(void);
void ui_reset_connection(void);

/* ---- weak hooks（与 LVGL 版一致） ---- */
void ui_hook_mode_output(ui_mode_t mode);
void ui_hook_reset_connection(void);
void ui_hook_get_rtc(int *hour, int *min, int *sec);

/* ?? Custom display text (raw HID 0xE2/0xE3; persisted at 0x3F10) ?? */
void UI_UpdateCustomText(void);
const char *UI_GetCustomText(void);
void UI_SetCustomText(const uint8_t *data, uint8_t len);

/* ---- 裸机入口（hidkbd_main 调用） ---- */
void ui_bm_init(ui_mode_t mode);   /* 首帧按指定模式绘制，避免 init 后重绘两遍 */
void ui_bm_process(void);  /* 1Hz 时钟刷新 + 设置页反馈恢复 */

/* 方向自检：白底 + "TOP"/"BOT" 标记 + 大号不对称字符 2Pq + 三个图标。
 * 用于确认文字/图标上下方向（模拟器 --dir 与真机共用）。 */
void ui_bm_direction_test(void);

/* 开机自检帧：1 = 上电只画方向自检帧（按 Tab+Backspace 翻页进入正常 UI）；
 * 0 = 正常启动。配合 NV3007_TEXT_FLIP 快速确认/修复真机方向。 */
#ifndef BM_UI_DIR_TEST_BOOT
#define BM_UI_DIR_TEST_BOOT 0
#endif

#endif /* BM_UI_H */
