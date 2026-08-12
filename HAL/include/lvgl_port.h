/*
 * lvgl_port.h
 *
 * LVGL 8.3 port for CH582 + NV3007 428×142.
 *   M1: display driver (partial buffer + flush_cb) + TMR0 1ms tick.
 *   Input (matrix→keypad) added in M4.
 */

#ifndef HAL_LVGL_PORT_H_
#define HAL_LVGL_PORT_H_

/**
 * @brief   Init LVGL: lv_init + partial-refresh display driver + TMR0 tick.
 *          Must be called after the display is initialised.
 */
void LVGL_Init(void);

/**
 * @brief   Main-loop LVGL task: process timers / redraw dirty areas.
 */
void LVGL_Process(void);

#endif /* HAL_LVGL_PORT_H_ */
