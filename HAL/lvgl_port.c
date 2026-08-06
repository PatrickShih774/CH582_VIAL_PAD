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
#include "numpad_ui.h"     /* 3-page dual-theme UI (ported from LVGL-opendesign) */
#include "CH58x_timer.h"   /* TMR0_TimerInit / TMR0_ITCfg / TMR0_GetITFlag */
#include "CH58x_clk.h"     /* RTC_InitTime / RTC_GetTime */

#if LVGL_EN

/* ── Partial refresh buffer: 284 × 10 rows = 5680 B (≈1/7.6 screen) ───
 * Full framebuffer 284×76×2 = 42 KB won't fit 32K RAM.  Single buffer,
 * no second buffer (double-buffer would double RAM). */
#define LVGL_BUF_ROWS   6
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

/* ── Public API ─────────────────────────────────────────────────────── */

/* ── RTC: internal 32K + init if invalid (same policy as M3.5 ui.c) ── */
static void lvgl_rtc_init(void)
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

/* ── Strong overrides of the weak numpad_ui hooks ───────────────────── */
void ui_hook_get_rtc(int *hour, int *min, int *sec)
{
    uint16_t y, mo, d, h, mi, s;
    RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
    *hour = (int)h; *min = (int)mi; *sec = (int)s;
}

void ui_hook_mode_output(ui_mode_t mode)
{
    /* Placeholder: route to 3-mode switch (USB/BLE/2.4G) later. */
    (void)mode;
}

void ui_hook_reset_connection(void)
{
    /* Placeholder: USB/BLE re-enumeration hook. */
}

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

    lvgl_rtc_init();         /* hardware RTC (internal 32K) */
    ui_init();               /* 3-page dual-theme numpad UI */
    lv_refr_now(NULL);       /* force first render */
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
