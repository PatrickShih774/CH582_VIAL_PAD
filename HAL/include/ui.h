/*
 * ui.h
 *
 * UI framework for NV3007 428×142 landscape display.
 *   Default home: real-time clock (HH:MM:SS) + HID keyboard state.
 *   Feature 1: calculator (UI skeleton, logic added later).
 */

#ifndef HAL_UI_H_
#define HAL_UI_H_

#include <stdint.h>

/* ── UI states ──────────────────────────────────────────────────────── */
typedef enum {
    UI_STATE_HOME = 0,   /* default: clock + HID state */
    UI_STATE_CALC,       /* calculator (framework) */
} UI_STATE_t;

/* ── Combo key to toggle HID keyboard ↔ calculator ────────────────────
 * Tab + Backspace: neither is used by the calculator, so the same combo
 * works in BOTH modes (HID→calc and calc→HID). */
#define UI_TOGGLE_K1  0x2B   /* Tab */
#define UI_TOGGLE_K2  0x2A   /* Backspace (HID_KEYBOARD_DELETE) */

/* ── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief   Initialize UI: RTC + render current state.
 */
void UI_Init(void);

/**
 * @brief   Main-loop UI task: refresh clock each second, handle state.
 */
void UI_Process(void);

/**
 * @brief   Switch UI state (renders the target screen).
 */
void UI_SetState(UI_STATE_t state);

/**
 * @brief   Get current UI state.
 */
UI_STATE_t UI_GetState(void);

/**
 * @brief   Request HID↔calculator toggle (set flag, handled in UI_Process).
 */
void UI_RequestToggle(void);

/**
 * @brief   Feed pressed keys into calculator mode (HID output suppressed).
 * @param   keys  array of HID usage keycodes
 * @param   n     number of keys
 */
void UI_CalcProcessKeys(const uint8_t *keys, uint8_t n);

/**
 * @brief   Custom display text buffer (set by host via raw HID 0xE2).
 *          Stored in EEPROM, shown on the home screen.
 */
#define UI_TEXT_MAX    15

void UI_UpdateCustomText(void);   /* redraw custom text after host update */
const char *UI_GetCustomText(void);

/**
 * @brief   Set custom display text (host via raw HID 0xE2), persist to
 *          EEPROM and refresh the home screen.
 */
void UI_SetCustomText(const uint8_t *data, uint8_t len);

/**
 * @brief   Check if two keycodes are both present (combo detection).
 */
static inline uint8_t UI_KeysBoth(const uint8_t *keys, uint8_t n, uint8_t k1, uint8_t k2)
{
    uint8_t f1 = 0, f2 = 0, i;
    for (i = 0; i < n; i++) {
        if (keys[i] == k1) f1 = 1;
        if (keys[i] == k2) f2 = 1;
    }
    return (f1 && f2);
}

#endif /* HAL_UI_H_ */