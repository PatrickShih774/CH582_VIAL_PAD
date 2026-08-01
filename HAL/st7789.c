/********************************** (C) COPYRIGHT *******************************
 * File Name          : st7789.c
 * Author             : WCH (modified)
 * Version            : V1.0
 * Date               : 2026/08/01
 * Description        : ST7789V SPI LCD driver — bit-bang, 76×284, 5×7 font
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "st7789.h"
#include <string.h>

/* ── Pin definitions ────────────────────────────────────────────────── */
#define PIN_SCK     GPIO_Pin_9    /* PA9  — SPI clock */
#define PIN_MOSI    GPIO_Pin_8    /* PA8  — SPI data  */
#define PIN_DC      GPIO_Pin_7    /* PB7  — data/command */
#define PIN_BL      GPIO_Pin_4    /* PB4  — backlight, ACTIVE-LOW (low = on) */
#define PIN_CS      GPIO_Pin_11   /* PA11 — chip select (was WS2812 pin) */

/* CS mode: 1 = pulse CS low→high per byte (PA11 GPIO)
 *           0 = hold CS low (grounded) — confirmed working */
#define ST7789_CS_PULSE   0

/* ── Bit-bang SPI primitives (mode 0: CPOL=0, CPHA=0) ──────────────── */
#define SCK_LOW()   GPIOA_ResetBits(PIN_SCK)
#define SCK_HIGH()  GPIOA_SetBits(PIN_SCK)
#define MOSI_LOW()  GPIOA_ResetBits(PIN_MOSI)
#define MOSI_HIGH() GPIOA_SetBits(PIN_MOSI)
#define DC_LOW()    GPIOB_ResetBits(PIN_DC)
#define DC_HIGH()   GPIOB_SetBits(PIN_DC)
#define BL_LOW()    GPIOB_ResetBits(PIN_BL)  /* backlight ON  (active-low) */
#define BL_HIGH()   GPIOB_SetBits(PIN_BL)    /* backlight OFF (active-low) */
#define CS_LOW()    GPIOA_ResetBits(PIN_CS)
#define CS_HIGH()   GPIOA_SetBits(PIN_CS)

/* ── ST7789 commands ────────────────────────────────────────────────── */
#define ST7789_NOP      0x00
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A
#define ST7789_PORCTRL  0xB2
#define ST7789_GCTRL    0xB7
#define ST7789_VCOMS    0xBB
#define ST7789_LCMCTRL  0xC0
#define ST7789_VDVVRHEN 0xC2
#define ST7789_VRHS     0xC3
#define ST7789_VDVS     0xC4
#define ST7789_FRCTRL2  0xC6
#define ST7789_PWCTRL1  0xD0
#define ST7789_GMCTRP1  0xE0
#define ST7789_GMCTRN1  0xE1

/* ── 5 × 7 ASCII font (0x20–0x7E) ──────────────────────────────────── */
static const uint8_t font_5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 0x20 space */
    {0x00,0x00,0x5F,0x00,0x00}, /* 0x21 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 0x22 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 0x23 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 0x24 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 0x25 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 0x26 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 0x27 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 0x28 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 0x29 ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 0x2A * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 0x2B + */
    {0x00,0x50,0x30,0x00,0x00}, /* 0x2C , */
    {0x08,0x08,0x08,0x08,0x08}, /* 0x2D - */
    {0x00,0x60,0x60,0x00,0x00}, /* 0x2E . */
    {0x20,0x10,0x08,0x04,0x02}, /* 0x2F / */
    {0x3E,0x45,0x49,0x51,0x3E}, /* 0x30 0 */
    {0x00,0x40,0x7F,0x42,0x00}, /* 0x31 1 */
    {0x46,0x49,0x51,0x61,0x42}, /* 0x32 2 */
    {0x31,0x4B,0x45,0x41,0x21}, /* 0x33 3 */
    {0x10,0x7F,0x12,0x14,0x18}, /* 0x34 4 */
    {0x39,0x45,0x45,0x45,0x27}, /* 0x35 5 */
    {0x30,0x49,0x49,0x4A,0x3C}, /* 0x36 6 */
    {0x03,0x05,0x09,0x71,0x01}, /* 0x37 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 0x38 8 */
    {0x1E,0x29,0x49,0x49,0x06}, /* 0x39 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 0x3A : */
    {0x00,0x56,0x36,0x00,0x00}, /* 0x3B ; */
    {0x00,0x08,0x14,0x22,0x41}, /* 0x3C < */
    {0x14,0x14,0x14,0x14,0x14}, /* 0x3D = */
    {0x41,0x22,0x14,0x08,0x00}, /* 0x3E > */
    {0x02,0x01,0x51,0x09,0x06}, /* 0x3F ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 0x40 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 0x41 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 0x42 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 0x43 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 0x44 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 0x45 E */
    {0x7F,0x09,0x09,0x01,0x01}, /* 0x46 F */
    {0x3E,0x41,0x41,0x51,0x32}, /* 0x47 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 0x48 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 0x49 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 0x4A J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 0x4B K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 0x4C L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 0x4D M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 0x4E N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 0x4F O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 0x50 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 0x51 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 0x52 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 0x53 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 0x54 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 0x55 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 0x56 V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* 0x57 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 0x58 X */
    {0x03,0x04,0x78,0x04,0x03}, /* 0x59 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 0x5A Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* 0x5B [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 0x5C \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* 0x5D ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 0x5E ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 0x5F _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 0x60 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 0x61 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 0x62 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 0x63 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 0x64 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 0x65 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 0x66 f */
    {0x08,0x14,0x54,0x54,0x3C}, /* 0x67 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 0x68 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 0x69 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 0x6A j */
    {0x00,0x7F,0x10,0x28,0x44}, /* 0x6B k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 0x6C l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 0x6D m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 0x6E n */
    {0x38,0x44,0x44,0x44,0x38}, /* 0x6F o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 0x70 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 0x71 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 0x72 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 0x73 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 0x74 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 0x75 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 0x76 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 0x77 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 0x78 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 0x79 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 0x7A z */
    {0x00,0x08,0x36,0x41,0x00}, /* 0x7B { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 0x7C | */
    {0x00,0x41,0x36,0x08,0x00}, /* 0x7D } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* 0x7E ~ */
};

/* ── Static helpers ─────────────────────────────────────────────────── */

/**
 * @brief   Send a byte via bit-bang SPI (MSB first, MODE 3).
 *          CPOL=1 (SCK idles HIGH), CPHA=1 (data sampled on the
 *          RISING edge; MOSI set after the falling edge).
 *          CS: pulse per byte (ST7789_CS_PULSE=1) or fixed low (=0).
 */
static void SPI_WriteByte(uint8_t data)
{
    uint8_t i;
#if ST7789_CS_PULSE
    CS_LOW();                          /* select this byte */
#endif
    for (i = 0; i < 8; i++) {
        SCK_LOW();                     /* falling edge (1st) */
        if (data & 0x80)
            MOSI_HIGH();
        else
            MOSI_LOW();
        data <<= 1;
        __nop(); __nop();
        SCK_HIGH();                    /* rising edge (2nd) — CPHA=1 sample */
        __nop(); __nop();
    }
#if ST7789_CS_PULSE
    CS_HIGH();                         /* deselect byte */
#endif
}

/**
 * @brief   Write a command byte (DC low, with setup time).
 *          DC restored high after the byte, matching seller timing —
 *          with CS fixed low, DC is the ONLY byte-type delimiter.
 */
static void ST7789_WriteCmd(uint8_t cmd)
{
    DC_LOW();
    __nop(); __nop(); __nop(); __nop();   /* DC setup time */
    SPI_WriteByte(cmd);
    DC_HIGH();
}

/**
 * @brief   Write a data byte (DC high, with setup time).
 */
static void ST7789_WriteData(uint8_t data)
{
    DC_HIGH();
    __nop(); __nop(); __nop(); __nop();   /* DC setup time */
    SPI_WriteByte(data);
}

/**
 * @brief   Write a 16-bit data word (DC high, MSB first).
 */
static void ST7789_WriteData16(uint16_t data)
{
    DC_HIGH();
    __nop(); __nop(); __nop(); __nop();   /* DC setup time */
    SPI_WriteByte((uint8_t)(data >> 8));
    SPI_WriteByte((uint8_t)(data & 0xFF));
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ST7789_Init(void)
{
    uint8_t i;

    /* ── Configure GPIO pins ──────────────────────────────────────── */
    GPIOA_ModeCfg(PIN_SCK | PIN_MOSI | PIN_CS, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(PIN_DC, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(PIN_BL, GPIO_ModeOut_PP_5mA);

    /* ── 1. Drive SCK & MOSI LOW before reset — prevent bus glitches
     * being mistaken for clock/data when CS is fixed low ──────────── */
    SCK_LOW();
    MOSI_LOW();
#if ST7789_CS_PULSE
    CS_HIGH();                         /* idle high, pulsed per byte */
#else
    CS_LOW();                          /* fixed low (grounded) */
#endif
    DC_LOW();
    BL_LOW();
    DelayMs(1);

    /* ── RST on PB23 (shared MCU reset) — hardware reset pulse at
     * MCU power-on.  Wait for ST7789 to stabilize. ─────────────────── */
    DelayMs(250);

    /* ── 2. Byte-boundary sync: 8× NOP(0x00) with DC=0 forces the SPI
     * shift register to an 8-bit boundary.  Replaces the CS-pulse
     * state reset when CS is fixed low. ────────────────────────────── */
    DC_LOW();
    __nop(); __nop(); __nop(); __nop();   /* DC setup */
    for (i = 0; i < 8; i++) {
        SPI_WriteByte(0x00);            /* NOP */
    }

    /* ── 3. Software reset ────────────────────────────────────────── */
    ST7789_WriteCmd(ST7789_SWRESET);
    DelayMs(150);

    /* ── Init sequence — 8-pin blue-board ST7789 (standard ST7789 regs) */
    ST7789_WriteCmd(0x11);            /* SLPOUT */
    DelayMs(120);

    ST7789_WriteCmd(0x36);            /* MADCTL — portrait (MV=0) */
    ST7789_WriteData(0x00);

    ST7789_WriteCmd(0x3A);            /* COLMOD — 16bit color */
    ST7789_WriteData(0x05);

    ST7789_WriteCmd(0xB2);            /* porch control */
    ST7789_WriteData(0x05);
    ST7789_WriteData(0x05);
    ST7789_WriteData(0x00);
    ST7789_WriteData(0x33);
    ST7789_WriteData(0x33);

    ST7789_WriteCmd(0xB7);            /* gate control */
    ST7789_WriteData(0x35);

    ST7789_WriteCmd(0xBB);            /* VCOM */
    ST7789_WriteData(0x21);

    ST7789_WriteCmd(0xC0);            /* LCM control */
    ST7789_WriteData(0x2C);

    ST7789_WriteCmd(0xC2);            /* VDV/VRH enable */
    ST7789_WriteData(0x01);

    ST7789_WriteCmd(0xC3);            /* VRH set */
    ST7789_WriteData(0x0B);

    ST7789_WriteCmd(0xC4);            /* VDV set */
    ST7789_WriteData(0x20);

    ST7789_WriteCmd(0xC6);            /* frame rate 60 Hz */
    ST7789_WriteData(0x0F);

    ST7789_WriteCmd(0xD0);            /* power ctrl 1 */
    ST7789_WriteData(0xA4);
    ST7789_WriteData(0xA1);

    ST7789_WriteCmd(0xE0);            /* positive gamma */
    ST7789_WriteData(0xD0); ST7789_WriteData(0x04); ST7789_WriteData(0x08);
    ST7789_WriteData(0x0A); ST7789_WriteData(0x09); ST7789_WriteData(0x05);
    ST7789_WriteData(0x2D); ST7789_WriteData(0x43); ST7789_WriteData(0x49);
    ST7789_WriteData(0x09); ST7789_WriteData(0x16); ST7789_WriteData(0x15);
    ST7789_WriteData(0x26); ST7789_WriteData(0x2B);
    ST7789_WriteCmd(0xE1);            /* negative gamma */
    ST7789_WriteData(0xD0); ST7789_WriteData(0x03); ST7789_WriteData(0x09);
    ST7789_WriteData(0x0A); ST7789_WriteData(0x0A); ST7789_WriteData(0x06);
    ST7789_WriteData(0x2E); ST7789_WriteData(0x44); ST7789_WriteData(0x40);
    ST7789_WriteData(0x3A); ST7789_WriteData(0x15); ST7789_WriteData(0x15);
    ST7789_WriteData(0x26); ST7789_WriteData(0x2A);


    ST7789_WriteCmd(0x60);            /* INVOFF — inversion off */

    ST7789_WriteCmd(0x29);            /* DISPON — display output on */
    DelayMs(120);                     /* wait for display module to start */

    /* ── Fill screen with black ────────────────────────────────────── */
    ST7789_Fill(ST7789_BLACK);
}

void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ST7789_WriteCmd(ST7789_CASET);
    ST7789_WriteData16((uint16_t)(x0 + ST7789_COL_OFF));
    ST7789_WriteData16((uint16_t)(x1 + ST7789_COL_OFF));

    ST7789_WriteCmd(ST7789_RASET);
    ST7789_WriteData16((uint16_t)(y0 + ST7789_LINE_OFF));
    ST7789_WriteData16((uint16_t)(y1 + ST7789_LINE_OFF));

    ST7789_WriteCmd(ST7789_RAMWR);
}

void ST7789_WritePixel(uint16_t color)
{
    ST7789_WriteData16(color);
}

void ST7789_Fill(uint16_t color)
{
    uint32_t i;
    uint32_t total = (uint32_t)ST7789_WIDTH * (uint32_t)ST7789_HEIGHT;

    ST7789_SetWindow(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);

    DC_HIGH();  /* all subsequent bytes are data */
    for (i = 0; i < total; i++) {
        SPI_WriteByte((uint8_t)(color >> 8));
        SPI_WriteByte((uint8_t)(color & 0xFF));
    }
}

void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t i, total;

    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    if (x + w > ST7789_WIDTH)  w = ST7789_WIDTH - x;
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;

    total = (uint32_t)w * (uint32_t)h;
    ST7789_SetWindow(x, y, x + w - 1, y + h - 1);

    DC_HIGH();
    for (i = 0; i < total; i++) {
        SPI_WriteByte((uint8_t)(color >> 8));
        SPI_WriteByte((uint8_t)(color & 0xFF));
    }
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    ST7789_SetWindow(x, y, x, y);
    ST7789_WriteData16(color);
}

void ST7789_DrawChar(char ch, uint16_t x, uint16_t y, uint16_t color, uint16_t bg)
{
    uint8_t i, j;
    uint8_t idx;

    if (ch < 0x20 || ch > 0x7E) ch = ' ';
    idx = ch - 0x20;

    if (x + 5 > ST7789_WIDTH || y + 8 > ST7789_HEIGHT) return;

    ST7789_SetWindow(x, y, x + 4, y + 7);   /* 5 wide × 8 high */
    DC_HIGH();

    /* font_5x7[idx][col]: 5 columns, each byte = 8 rows, bit7 = top.
     * Pixel (row j, col i) = font[idx][i] & (0x80 >> j). */
    for (j = 0; j < 8; j++) {
        for (i = 0; i < 5; i++) {
            if (font_5x7[idx][i] & (0x80 >> j)) {
                SPI_WriteByte((uint8_t)(color >> 8));
                SPI_WriteByte((uint8_t)(color & 0xFF));
            } else {
                SPI_WriteByte((uint8_t)(bg >> 8));
                SPI_WriteByte((uint8_t)(bg & 0xFF));
            }
        }
    }
}

void ST7789_DrawString(const char *str, uint16_t x, uint16_t y, uint16_t color, uint16_t bg)
{
    while (*str) {
        ST7789_DrawChar(*str, x, y, color, bg);
        x += 6;  /* 5 px char + 1 px spacing */
        if (x + 5 > ST7789_WIDTH) {
            x = 0;
            y += 9;  /* 8 px height + 1 px spacing */
        }
        str++;
    }
}

void ST7789_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    uint16_t i;
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    if (x + w > ST7789_WIDTH) w = ST7789_WIDTH - x;

    ST7789_SetWindow(x, y, x + w - 1, y);
    DC_HIGH();
    for (i = 0; i < w; i++) {
        SPI_WriteByte((uint8_t)(color >> 8));
        SPI_WriteByte((uint8_t)(color & 0xFF));
    }
}

void ST7789_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    uint16_t i;
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;

    ST7789_SetWindow(x, y, x, y + h - 1);
    DC_HIGH();
    for (i = 0; i < h; i++) {
        SPI_WriteByte((uint8_t)(color >> 8));
        SPI_WriteByte((uint8_t)(color & 0xFF));
    }
}

void ST7789_SetBrightness(uint8_t level)
{
    /* GPIO backlight (simple on/off) — active-low: level>0 => pin low */
    if (level)
        BL_LOW();
    else
        BL_HIGH();
}