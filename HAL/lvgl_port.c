/********************************** (C) COPYRIGHT *******************************
 * File Name          : lvgl_port.c
 * Author             : WCH (modified)
 * Version            : V1.0
 * Date               : 2026/08/03
 * Description        : LVGL 8.3 port — display driver + TMR0 1ms tick (M1)
 *                      Partial-refresh: 284×10 single buffer (5680 B).
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/

#include "config.h"        /* LVGL_EN */
#include "st7789.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "CH58x_timer.h"   /* TMR0_TimerInit / TMR0_ITCfg / TMR0_GetITFlag */

#if LVGL_EN

/* ── Partial refresh buffer: 284 × 10 rows = 5680 B (≈1/7.6 screen) ───
 * Full framebuffer 284×76×2 = 42 KB won't fit 32K RAM.  Single buffer,
 * no second buffer (double-buffer would double RAM). */
#define LVGL_BUF_ROWS   10
static lv_color_t lvgl_draw_buf[ST7789_WIDTH * LVGL_BUF_ROWS];

/* ── flush_cb: LVGL render area → ST7789 window burst write ────────── */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    ST7789_Flush((uint16_t)area->x1, (uint16_t)area->y1,
                 (uint16_t)(area->x2 - area->x1 + 1),
                 (uint16_t)(area->y2 - area->y1 + 1),
                 (uint16_t *)color_p);
    lv_disp_flush_ready(drv);
}

/* ── TMR0 1 ms tick → lv_tick_inc ─────────────────────────────────────
 * SysTick is owned by the BLE lib (HAL/MCU.c), so LVGL gets its own
 * free timer.  Overrides the weak default handler in startup_CH583.S.
 * ⚠️ MUST be __INTERRUPT: without it GCC emits `ret` instead of `mret`,
 * so the ISR returns to the interrupted function's ra — skipping the
 * instructions between mepc and ra and leaving callee-saved regs stale.
 * Matches TMR3_IRQHandler / USB_IRQHandler. */
__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
    if (TMR0_GetITFlag(TMR0_3_IT_CYC_END)) {
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
        lv_tick_inc(1);
    }
}

static void lvgl_tick_init(void)
{
    TMR0_TimerInit(60000);                 /* 1 ms @ 60 MHz sysclk */
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_EnableIRQ(TMR0_IRQn);
}

/* ── M1 test UI: RGB color bands + label ──────────────────────────────
 * Verifies flush path, RGB565 byte order, and font render.
 * Replaced by the real home screen in M3. */
static void lvgl_test_ui(void)
{
    static const uint32_t rgb[3] = {0xFF0000, 0x00FF00, 0x0000FF}; /* R G B (24-bit hex for lv_color_hex) */
    uint8_t i;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    for (i = 0; i < 3; i++) {
        lv_obj_t *bar = lv_obj_create(scr);
        lv_obj_set_pos(bar, 0, i * 25);
        lv_obj_set_size(bar, ST7789_WIDTH, 25);
        lv_obj_set_style_bg_color(bar, lv_color_hex(rgb[i]), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
    }

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "LVGL 284x76 OK");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

/* ── Public API ─────────────────────────────────────────────────────── */

void LVGL_Init(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t disp_drv;

    lv_init();
    lvgl_tick_init();

    lv_disp_draw_buf_init(&draw_buf, lvgl_draw_buf, NULL, ST7789_WIDTH * LVGL_BUF_ROWS);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = ST7789_WIDTH;
    disp_drv.ver_res  = ST7789_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lvgl_test_ui();          /* M1 color-band test; replaced in M3 */
}

void LVGL_Process(void)
{
    uint32_t t = lv_timer_handler();
    /* Yield when idle — the bare `lv_timer_handler()` in a tight while(1)
     * spins the CPU at 100% (no sleep), which added to VIAL/USB activity
     * browns-out the marginal 3.3V rail (rst=0x01 reset during VIAL comm,
     * captured in Bus Hound).  Sleeping >=1ms keeps the CPU idle like the
     * legacy v0.3 UI did.  lv_tick keeps advancing via the TMR0 ISR, so no
     * LVGL timer is missed. */
    if (t < 1) t = 1;
    if (t > 100) t = 100;
    DelayMs((uint16_t)t);
}

#endif /* LVGL_EN */
