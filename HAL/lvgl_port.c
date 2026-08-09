/********************************** (C) COPYRIGHT *******************************
 * File Name          : lvgl_port.c
 * Author             : WCH (modified)
 * Version            : V1.0
 * Date               : 2026/08/03
 * Description        : LVGL 8.3 port — display driver + TMR0 1ms tick (M1)
 *                      Partial-refresh: 428×3 single buffer (2568 B, B0.2 overlay).
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
#include "ui_mem.h"        /* LVGL custom memory hooks (B0.6) */
#include <string.h>        /* memcpy (ui_lvgl_realloc) */

extern uint8_t g_boot_mode;   /* 0x0B=USB / 0xBE=BLE / 0x24=RF */

#if LVGL_EN

/* ── Mode-selected LVGL pools (B0.6) ──────────────────────────────────
 * USB mode : 16KB pool + 4-row draw buffer in .lvgl_shared (RAM-base
 *            overlay region; BLE stack inactive).
 * BLE mode : 6KB pool + 2-row draw buffer in .lvgl_shared_ble, the free
 *            tail of the shared region after .ovl_ble + .ble_heap.
 * With LV_MEM_CUSTOM=1 LVGL allocates through ui_lvgl_* (first-fit
 * free-list) instead of its static array. */
#define LVGL_BUF_ROWS      3   /* USB: 428x3x2 = 2568B (NV3007 428x142 landscape) */
#define LVGL_USB_POOL_SIZE (16 * 1024)
#define LVGL_BLE_POOL_SIZE (6 * 1024)
#define LVGL_BLE_BUF_ROWS  2   /* BLE: 428x2x2 = 1712B */

static uint8_t   lvgl_pool_usb[LVGL_USB_POOL_SIZE] __attribute__((section(".lvgl_shared")));
static lv_color_t lvgl_draw_buf_usb[ST7789_WIDTH * LVGL_BUF_ROWS] __attribute__((section(".lvgl_shared")));
static uint8_t   lvgl_shared_pad[0x170] __attribute__((section(".lvgl_shared"), used));   /* keep shared region >= BLE LVGL tail (0x4B70) */
static uint8_t   lvgl_pool_ble[LVGL_BLE_POOL_SIZE] __attribute__((section(".lvgl_shared_ble")));
static lv_color_t lvgl_draw_buf_ble[ST7789_WIDTH * LVGL_BLE_BUF_ROWS] __attribute__((section(".lvgl_shared_ble")));

/* ── tiny first-fit allocator over the active pool ──────────────────── */
typedef struct ui_mem_blk {
    uint32_t size;    /* block size, header included, 8B aligned */
    uint32_t free;    /* 1 = free */
} ui_mem_blk_t;

static uint8_t * s_pool_base;
static uint32_t  s_pool_bytes;

static uint32_t ui_mem_align_up(uint32_t v)
{
    return (v + 7u) & ~7u;
}

void ui_lvgl_mem_init(void * base, uint32_t bytes)
{
    ui_mem_blk_t * h = (ui_mem_blk_t *)base;
    s_pool_base = (uint8_t *)base;
    s_pool_bytes = bytes;
    h->size = bytes;
    h->free = 1;
}

void * ui_lvgl_alloc(size_t size)
{
    uint32_t need = ui_mem_align_up((uint32_t)size + sizeof(ui_mem_blk_t));
    ui_mem_blk_t * h = (ui_mem_blk_t *)s_pool_base;
    ui_mem_blk_t * end = (ui_mem_blk_t *)(s_pool_base + s_pool_bytes);

    if (s_pool_base == NULL) return NULL;
    while (h < end) {
        if (h->free && h->size >= need) {
            if (h->size - need >= sizeof(ui_mem_blk_t) + 8) {
                ui_mem_blk_t * n = (ui_mem_blk_t *)((uint8_t *)h + need);
                n->size = h->size - need;
                n->free = 1;
                h->size = need;
            }
            h->free = 0;
            return (void *)((uint8_t *)h + sizeof(ui_mem_blk_t));
        }
        h = (ui_mem_blk_t *)((uint8_t *)h + h->size);
    }
    return NULL;
}

void ui_lvgl_free(void * ptr)
{
    ui_mem_blk_t * h;
    ui_mem_blk_t * n;
    ui_mem_blk_t * cur;

    if (ptr == NULL) return;
    h = (ui_mem_blk_t *)((uint8_t *)ptr - sizeof(ui_mem_blk_t));
    h->free = 1;
    /* coalesce with next */
    n = (ui_mem_blk_t *)((uint8_t *)h + h->size);
    if ((uint8_t *)n < s_pool_base + s_pool_bytes && n->free) {
        h->size += n->size;
    }
    /* coalesce with previous */
    cur = (ui_mem_blk_t *)s_pool_base;
    while (cur < h) {
        ui_mem_blk_t * nx = (ui_mem_blk_t *)((uint8_t *)cur + cur->size);
        if (nx == h && cur->free) {
            cur->size += h->size;
            break;
        }
        cur = nx;
    }
}

void * ui_lvgl_realloc(void * ptr, size_t new_size)
{
    ui_mem_blk_t * h;
    uint32_t old_size;
    void * np;

    if (ptr == NULL) return ui_lvgl_alloc(new_size);
    h = (ui_mem_blk_t *)((uint8_t *)ptr - sizeof(ui_mem_blk_t));
    old_size = h->size - sizeof(ui_mem_blk_t);
    if (ui_mem_align_up((uint32_t)new_size + sizeof(ui_mem_blk_t)) <= h->size) {
        return ptr;
    }
    np = ui_lvgl_alloc(new_size);
    if (np == NULL) return NULL;
    memcpy(np, ptr, old_size < (uint32_t)new_size ? old_size : (uint32_t)new_size);
    ui_lvgl_free(ptr);
    return np;
}

/* ── Partial refresh buffer: 428 × 3 rows = 2568 B (≈1/47 screen) ───
 * Full framebuffer 428×142×2 = 118 KB won't fit 32K RAM.  Single buffer,
 * no second buffer (double-buffer would double RAM).
 * 3 rows (not 4/6) because the LVGL pool+draw buffer share the RAM-base
 * overlay region with the BLE stack highcode + heap (Ld/Link.ld, B0.2). */

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

    if (g_boot_mode == 0x0B) {
        /* USB mode: full 16KB pool + 3-row draw buffer */
        ui_lvgl_mem_init(lvgl_pool_usb, sizeof(lvgl_pool_usb));
        lv_disp_draw_buf_init(&draw_buf, lvgl_draw_buf_usb, NULL,
                              ST7789_WIDTH * LVGL_BUF_ROWS);
    } else {
        /* BLE/RF mode: 6KB pool + 2-row draw buffer in shared-RAM tail */
        ui_lvgl_mem_init(lvgl_pool_ble, sizeof(lvgl_pool_ble));
        lv_disp_draw_buf_init(&draw_buf, lvgl_draw_buf_ble, NULL,
                              ST7789_WIDTH * LVGL_BLE_BUF_ROWS);
    }
    lv_init();
    lvgl_tick_init();
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = ST7789_WIDTH;   /* 428 (NV3007 landscape) */
    disp_drv.ver_res  = ST7789_HEIGHT;  /* 142 */
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
