/********************************** (C) COPYRIGHT *******************************
 * File Name          : ui.c
 * Author             : WCH (modified)
 * Version            : V1.0
 * Date               : 2026/08/02
 * Description        : UI framework — home (clock + HID state) + calculator
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "CH58x_clk.h"     /* RTC_InitTime / RTC_GetTime */
#include "st7789.h"
#include "ui.h"

/* ── UI layout (284×76) ──────────────────────────────────────────────── */
#define UI_CLOCK_X    71      /* "HH:MM:SS" = 8ch×18-3 = 141 wide, centered: (284-141)/2 */
#define UI_CLOCK_Y    26      /* 3× font 24 high, vertically centered: (76-24)/2 */
#define UI_CLOCK_W    141
#define UI_CLOCK_H    24
#define UI_STATUS_X   0       /* bottom-left (y=0 renders at bottom on this panel) */
#define UI_STATUS_Y   0
#define UI_SEC_X      (UI_CLOCK_X + 6 * 18)  /* seconds chars start at index 6 */
#define UI_SEC_W      (2 * 18 - 3)            /* "SS" = 33 px */

static UI_STATE_t ui_state   = UI_STATE_HOME;
static uint8_t     last_sec  = 0xFF;
static uint16_t    last_min  = 0xFFFF;
static uint16_t    last_hour = 0xFFFF;

/* ── Format "HH:MM:SS" without sprintf ──────────────────────────────── */
static void fmt_time(uint16_t h, uint16_t m, uint16_t s, char *out)
{
    out[0] = (char)('0' + h / 10);  out[1] = (char)('0' + h % 10);
    out[2] = ':';
    out[3] = (char)('0' + m / 10);  out[4] = (char)('0' + m % 10);
    out[5] = ':';
    out[6] = (char)('0' + s / 10);  out[7] = (char)('0' + s % 10);
    out[8] = '\0';
}

/* ── Clock box (3× font, centered) ───────────────────────────────────── */
static void UI_DrawClock(uint16_t h, uint16_t m, uint16_t s)
{
    char buf[9];
    ST7789_SetFontZoom(3);
    ST7789_FillRect(UI_CLOCK_X, UI_CLOCK_Y, UI_CLOCK_W, UI_CLOCK_H, ST7789_BLACK);
    fmt_time(h, m, s, buf);
    ST7789_DrawString(buf, UI_CLOCK_X, UI_CLOCK_Y, ST7789_CYAN, ST7789_BLACK);
}

/* ── HID state (1× font, bottom-left) ────────────────────────────────── */
static void UI_DrawStatus(void)
{
    ST7789_SetFontZoom(1);
    ST7789_DrawString("USB MODE", UI_STATUS_X, UI_STATUS_Y, ST7789_GREEN, ST7789_BLACK);
}

/* ── Home: centered clock + bottom-left HID state ────────────────────── */
static void UI_DrawHome(uint16_t h, uint16_t m, uint16_t s)
{
    UI_DrawClock(h, m, s);
    UI_DrawStatus();
}

/* ── Calculator screen (framework) ───────────────────────────────────── */
static void UI_DrawCalc(void)
{
    ST7789_Fill(ST7789_BLACK);

    /* Display area (top) */
    ST7789_DrawString("CALC", 0, 0, ST7789_YELLOW, ST7789_BLACK);
    ST7789_DrawString("0", 0, 27, ST7789_WHITE, ST7789_BLACK);

    /* Key hints (bottom) — placeholder for key mapping */
    ST7789_DrawString("NUM=+ - * /", 0, 54, ST7789_GREEN, ST7789_BLACK);
}

/* ── Public API ─────────────────────────────────────────────────────── */

void UI_Init(void)
{
    uint16_t y, mo, d, h, mi, s;

    /* ── Enable internal 32K clock for RTC ─────────────────────────── */
    sys_safe_access_enable();
    R8_CK32K_CONFIG &= ~(RB_CLK_OSC32K_XT | RB_CLK_XT32K_PON);
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_CK32K_CONFIG |= RB_CLK_INT32K_PON;
    sys_safe_access_disable();

    /* RTC: initialize ONLY if time is invalid (first power-on).  RTC is
     * hardware-counted and survives reset (no power loss), so do NOT
     * reset it every boot — otherwise time zeroes on every reset. */
    RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
    if (y < 2020 || y > 2070) {
        RTC_InitTime(2026, 1, 1, 0, 0, 0);
        RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
    }
    last_sec = (uint8_t)s;

    ui_state = UI_STATE_HOME;
    UI_DrawHome(h, mi, s);
}

void UI_Process(void)
{
    uint16_t y, mo, d, h, mi, s;

    if (ui_state == UI_STATE_HOME) {
        RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
        if ((uint8_t)s != last_sec) {      /* second tick */
            if ((uint8_t)s == 0 || mi != last_min || h != last_hour) {
                /* minute/hour rolled over: redraw whole clock */
                UI_DrawClock(h, mi, s);
            } else {
                /* only seconds changed: redraw the 2-char seconds box */
                char buf[3];
                buf[0] = (char)('0' + s / 10);
                buf[1] = (char)('0' + s % 10);
                buf[2] = '\0';
                ST7789_SetFontZoom(3);
                ST7789_FillRect(UI_SEC_X, UI_CLOCK_Y, UI_SEC_W, UI_CLOCK_H, ST7789_BLACK);
                ST7789_DrawString(buf, UI_SEC_X, UI_CLOCK_Y, ST7789_CYAN, ST7789_BLACK);
            }
            last_hour = h;
            last_min  = mi;
            last_sec  = (uint8_t)s;
        }
        /* TODO: detect combo key → switch to CALC */
    }
    /* CALC state: static for now, key input handled later */
}

void UI_SetState(UI_STATE_t state)
{
    uint16_t y, mo, d, h, mi, s;

    if (state == ui_state) return;
    ui_state = state;

    if (state == UI_STATE_CALC) {
        UI_DrawCalc();
    } else {
        RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
        last_sec  = (uint8_t)s;
        last_min  = mi;
        last_hour = h;
        UI_DrawHome(h, mi, s);
    }
}

UI_STATE_t UI_GetState(void)
{
    return ui_state;
}