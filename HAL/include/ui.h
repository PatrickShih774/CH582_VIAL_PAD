/*
 * ui.h
 *
 * UI framework for ST7789 284×76 landscape display.
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

#endif /* HAL_UI_H_ */