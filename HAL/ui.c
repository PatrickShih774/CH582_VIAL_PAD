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
#include "NV3007.h"
#include "ui.h"

extern uint8_t g_boot_mode;   /* 0x0B=USB / 0xBE=BLE / 0x24=RF (defined in APP/hidkbd_main.c) */

/* ── UI layout (284×76) ──────────────────────────────────────────────── */
/* ── Top bar (firmware coords: y=0 = BOTTOM of physical panel) ───────── */
#define UI_TOP_USB_X   0
#define UI_TOP_Y       64      /* 1× font 8 high, near panel top */
#define UI_TOP_BAT_X   250
#define UI_TOP_BAT_Y   62
#define UI_CUSTOM_X    0       /* custom text at bottom-left */
#define UI_CUSTOM_Y    0

/* ── Center clock (4× HH:MM + 2× seconds) ────────────────────────────── */
#define UI_CLOCK_X     84      /* "HH:MM" 4× = 116 wide, centered: (284-116)/2 */
#define UI_CLOCK_Y     22      /* 4× 32 high, vertically centered */
#define UI_CLOCK_W     116
#define UI_SEC_X       205     /* seconds, 2× font, right of clock */
#define UI_SEC_Y       22      /* bottom-aligned with clock baseline */
#define UI_SEC_W       16

/* ── Bottom bar (date + mode) ────────────────────────────────────────── */
#define UI_DATE_X      112     /* "YYYY-MM-DD" 1× ≈ 59 wide, centered */
#define UI_BOT_Y       8
#define UI_MODE_X      210
#define UI_MODE_Y      0

#define UI_TEXT_ADDR   0x3F10  /* custom text EEPROM (avoid keymap 0x3000, mode 0x3F00) */

static UI_STATE_t ui_state     = UI_STATE_HOME;
static uint8_t     last_sec    = 0xFF;
static uint16_t    last_min    = 0xFFFF;
static uint16_t    last_hour   = 0xFFFF;
static uint16_t    last_y      = 0xFFFF;
static uint16_t    last_mo     = 0xFF;
static uint16_t    last_d      = 0xFF;
static uint8_t     ui_toggle_req = 0;
static char        ui_custom_text[UI_TEXT_MAX + 1];

/* ── Battery icon (frame + fill + terminal) ──────────────────────────── */
static void UI_IconBattery(uint16_t x, uint16_t y, uint16_t color)
{
    NV3007_DrawHLine(x, y, 12, color);
    NV3007_DrawHLine(x, y + 6, 12, color);
    NV3007_DrawVLine(x, y, 7, color);
    NV3007_DrawVLine(x + 11, y, 7, color);
    NV3007_DrawPixel(x + 12, y + 2, color);   /* positive terminal */
    NV3007_DrawPixel(x + 12, y + 3, color);
    NV3007_DrawPixel(x + 12, y + 4, color);
    NV3007_FillRect(x + 1, y + 1, 8, 5, color);   /* charge fill */
}

/* ── Check mark ✓ ────────────────────────────────────────────────────── */
static void UI_IconCheck(uint16_t x, uint16_t y, uint16_t color)
{
    NV3007_DrawPixel(x,     y + 4, color);
    NV3007_DrawPixel(x + 1, y + 3, color);
    NV3007_DrawPixel(x + 2, y + 2, color);
    NV3007_DrawPixel(x + 3, y + 1, color);
    NV3007_DrawPixel(x + 4, y,     color);
    NV3007_DrawPixel(x + 2, y + 4, color);   /* hook */
    NV3007_DrawPixel(x + 3, y + 3, color);
}

/* ── Clock: big HH:MM (3×) + seconds (1×) ────────────────────────────── */
static void UI_DrawClock(uint16_t h, uint16_t m, uint16_t s)
{
    char buf[8];

    /* HH:MM — 4× font (20×32) */
    NV3007_SetFontZoom(4);
    buf[0] = (char)('0' + h / 10);  buf[1] = (char)('0' + h % 10);
    buf[2] = ':';
    buf[3] = (char)('0' + m / 10);  buf[4] = (char)('0' + m % 10);
    buf[5] = '\0';
    NV3007_FillRect(UI_CLOCK_X, UI_CLOCK_Y, UI_CLOCK_W, 32, NV3007_BLACK);
    NV3007_DrawString(buf, UI_CLOCK_X, UI_CLOCK_Y, NV3007_WHITE, NV3007_BLACK);

    /* seconds — 2× font (10×16), right of clock */
    NV3007_SetFontZoom(2);
    buf[0] = (char)('0' + s / 10);  buf[1] = (char)('0' + s % 10);
    buf[2] = '\0';
    NV3007_FillRect(UI_SEC_X, UI_SEC_Y, UI_SEC_W, 16, NV3007_BLACK);
    NV3007_DrawString(buf, UI_SEC_X, UI_SEC_Y, NV3007_WHITE, NV3007_BLACK);
}

/* ── Top bar: USB MODE + custom text + battery ───────────────────────── */
static void UI_DrawStatus(void)
{
    const char *mode = "USB MODE";
    if (g_boot_mode == 0xBE) {
        mode = "BT MODE";
    } else if (g_boot_mode == 0x24) {
        mode = "RF MODE";
    }
    NV3007_SetFontZoom(1);
    NV3007_DrawString(mode, UI_TOP_USB_X, UI_TOP_Y, NV3007_GREEN, NV3007_BLACK);
    UI_IconBattery(UI_TOP_BAT_X, UI_TOP_BAT_Y, NV3007_WHITE);
}

/* ── Public: custom text update (called from raw HID 0xE2) ───────────── */
void UI_UpdateCustomText(void)
{
    if (ui_state == UI_STATE_HOME) {
        NV3007_SetFontZoom(1);
        NV3007_FillRect(UI_CUSTOM_X, UI_CUSTOM_Y, NV3007_WIDTH - UI_CUSTOM_X, 8, NV3007_BLACK);
        if (ui_custom_text[0]) {
            NV3007_DrawString(ui_custom_text, UI_CUSTOM_X, UI_CUSTOM_Y,
                              NV3007_YELLOW, NV3007_BLACK);
        }
    }
}

const char *UI_GetCustomText(void)
{
    return ui_custom_text;
}

void UI_SetCustomText(const uint8_t *data, uint8_t len)
{
    uint8_t i;
    if (len > UI_TEXT_MAX) len = UI_TEXT_MAX;
    for (i = 0; i < len; i++) ui_custom_text[i] = (char)data[i];
    ui_custom_text[len] = '\0';

    /* persist to EEPROM (erase 1 sector, then write text + terminator) */
    EEPROM_ERASE(UI_TEXT_ADDR, 4);
    EEPROM_WRITE(UI_TEXT_ADDR, (uint8_t *)ui_custom_text, len + 1);

    UI_UpdateCustomText();
}

/* ── Bottom date "YYYY-MM-DD" ─────────────────────────────────────────── */
static void UI_DrawDate(uint16_t y, uint16_t mo, uint16_t d)
{
    char buf[12];
    NV3007_SetFontZoom(1);
    buf[0] = (char)('0' + y / 1000);  buf[1] = (char)('0' + (y / 100) % 10);
    buf[2] = (char)('0' + (y / 10) % 10); buf[3] = (char)('0' + y % 10);
    buf[4] = '-';
    buf[5] = (char)('0' + mo / 10);  buf[6] = (char)('0' + mo % 10);
    buf[7] = '-';
    buf[8] = (char)('0' + d / 10);   buf[9] = (char)('0' + d % 10);
    buf[10] = '\0';
    NV3007_FillRect(UI_DATE_X, UI_BOT_Y, 60, 8, NV3007_BLACK);
    NV3007_DrawString(buf, UI_DATE_X, UI_BOT_Y, NV3007_YELLOW, NV3007_BLACK);
}

/* ── Home: top bar + center clock + bottom date/mode ─────────────────── */
static void UI_DrawHome(void)
{
    uint16_t y, mo, d, h, mi, s;

    RTC_GetTime(&y, &mo, &d, &h, &mi, &s);

    NV3007_Fill(NV3007_BLACK);
    UI_DrawStatus();             /* top bar */
    UI_DrawClock(h, mi, s);      /* center clock */

    NV3007_SetFontZoom(1);

    /* bottom-left: custom text */
    if (ui_custom_text[0]) {
        NV3007_DrawString(ui_custom_text, UI_CUSTOM_X, UI_CUSTOM_Y,
                          NV3007_YELLOW, NV3007_BLACK);
    }

    UI_DrawDate(y, mo, d);       /* bottom: date */

    /* bottom-right: mode + check */
    NV3007_DrawString("MODA", UI_MODE_X, UI_MODE_Y, NV3007_YELLOW, NV3007_BLACK);
    UI_IconCheck(UI_MODE_X + 30, UI_MODE_Y, NV3007_GREEN);
}

/* ── Calculator screen (framework) ───────────────────────────────────── */
static void UI_DrawCalc(void)
{
    NV3007_Fill(NV3007_BLACK);

    /* Display area (top) */
    NV3007_DrawString("CALC", 0, 0, NV3007_YELLOW, NV3007_BLACK);
    NV3007_DrawString("0", 0, 27, NV3007_WHITE, NV3007_BLACK);

    /* Key hints (bottom) — placeholder for key mapping */
    NV3007_DrawString("NUM=+ - * /", 0, 54, NV3007_GREEN, NV3007_BLACK);
}

/* ── Public API ─────────────────────────────────────────────────────── */

void UI_Init(void)
{
    uint16_t y, mo, d, h, mi, s;
    uint8_t  i;

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
    if (y <= 2020 || y > 2070) {      /* uninitialized RTC → 2020-01-01 etc. */
        RTC_InitTime(2026, 1, 1, 0, 0, 0);
        RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
    }
    last_sec  = (uint8_t)s;
    last_min  = mi;
    last_hour = h;
    last_y    = y;
    last_mo   = mo;
    last_d    = d;

    /* ── Load custom display text from EEPROM ──────────────────────── */
    EEPROM_READ(UI_TEXT_ADDR, (uint8_t *)ui_custom_text, UI_TEXT_MAX + 1);
    ui_custom_text[UI_TEXT_MAX] = '\0';
    for (i = 0; i < UI_TEXT_MAX; i++) {
        /* invalid (unwritten 0xFF) or non-printable → truncate here */
        if (ui_custom_text[i] == 0xFF || ui_custom_text[i] < 0x20 ||
            ui_custom_text[i] > 0x7E) {
            ui_custom_text[i] = '\0';
            break;
        }
    }
    if (!ui_custom_text[0]) {
        memcpy(ui_custom_text, "FinPad", 7);   /* default until host sets one */
    }

    ui_state = UI_STATE_HOME;
    UI_DrawHome();
}

void UI_RequestToggle(void)
{
    ui_toggle_req = 1;
}

/* ── Map HID usage keycode to a display char for the calculator ──────── */
static char UI_KeyToChar(uint8_t k)
{
    switch (k) {
        case 0x59: return '1';  case 0x5A: return '2';  case 0x5B: return '3';
        case 0x5C: return '4';  case 0x5D: return '5';  case 0x5E: return '6';
        case 0x5F: return '7';  case 0x60: return '8';  case 0x61: return '9';
        case 0x62: return '0';
        case 0x57: return '+';  case 0x56: return '-';
        case 0x55: return '*';  case 0x54: return '/';
        case 0x63: return '.';
        default:   return 0;
    }
}

void UI_CalcProcessKeys(const uint8_t *keys, uint8_t n)
{
    /* TODO: calculator state machine (digit/operator/result).
     * Framework: echo pressed keys on the display row. */
    char buf[16];
    uint8_t i, len = 0;

    for (i = 0; i < n && len < 14; i++) {
        char c = UI_KeyToChar(keys[i]);
        if (c) buf[len++] = c;
    }
    buf[len] = '\0';

    NV3007_SetFontZoom(3);
    NV3007_FillRect(0, 27, NV3007_WIDTH, 24, NV3007_BLACK);
    NV3007_DrawString(buf, 0, 27, NV3007_WHITE, NV3007_BLACK);
}

void UI_Process(void)
{
    uint16_t y, mo, d, h, mi, s;

    /* Handle HID↔calculator toggle request (set by TMR3 combo detection) */
    if (ui_toggle_req) {
        ui_toggle_req = 0;
        if (ui_state == UI_STATE_HOME)
            UI_SetState(UI_STATE_CALC);
        else
            UI_SetState(UI_STATE_HOME);
        return;
    }

    if (ui_state == UI_STATE_HOME) {
        RTC_GetTime(&y, &mo, &d, &h, &mi, &s);
        if ((uint8_t)s != last_sec) {      /* second tick */
            if ((uint8_t)s == 0 || mi != last_min || h != last_hour) {
                /* minute/hour rolled over: redraw whole clock */
                UI_DrawClock(h, mi, s);
            } else {
                /* only seconds changed: redraw 2× "SS" box only */
                char buf[3];
                buf[0] = (char)('0' + s / 10);
                buf[1] = (char)('0' + s % 10);
                buf[2] = '\0';
                NV3007_SetFontZoom(2);
                NV3007_FillRect(UI_SEC_X, UI_SEC_Y, UI_SEC_W, 16, NV3007_BLACK);
                NV3007_DrawString(buf, UI_SEC_X, UI_SEC_Y, NV3007_WHITE, NV3007_BLACK);
            }
            last_hour = h;
            last_min  = mi;
            last_sec  = (uint8_t)s;
        }
        if (y != last_y || mo != last_mo || d != last_d) {
            /* date changed (midnight rollover or host time-set): redraw date */
            UI_DrawDate(y, mo, d);
            last_y = y;
            last_mo = mo;
            last_d = d;
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
        last_y    = y;
        last_mo   = mo;
        last_d    = d;
        UI_DrawHome();
    }
}

UI_STATE_t UI_GetState(void)
{
    return ui_state;
}
