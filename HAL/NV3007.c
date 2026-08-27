/********************************** (C) COPYRIGHT *******************************
 * File Name          : NV3007.c
 * Author             : WCH (modified)
 * Version            : V2.0
 * Date               : 2026/08/09
 * Description        : NV3007 SPI LCD driver (142x428, bit-bang, mode 0)
 *                      Public API is NV3007_* (v0.5 rename from ST7789_*).
 *                      Logical landscape 428x142, physical portrait 142x428.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "NV3007.h"
#include <string.h>

/* ---- Pin definitions ---- */
#define PIN_SCK     GPIO_Pin_9    /* PA9  - SPI clock */
#define PIN_MOSI    GPIO_Pin_8    /* PA8  - SPI data  */
#define PIN_DC      GPIO_Pin_7    /* PB7  - data/command */
#define PIN_BL      GPIO_Pin_4    /* PB4  - backlight, ACTIVE-LOW (low = on) */
#define PIN_CS      GPIO_Pin_11   /* PA11 - chip select (not used; CS grounded) */
#if NV3007_RST_USE_PA10
#define PIN_RST     GPIO_Pin_10   /* PA10 - 32K_XI, free while RTC uses internal 32K */
#else
#define PIN_RST     GPIO_Pin_11   /* PA11 - panel RST when NV3007_RST_GPIO=1 */
#endif

#define NV3007_CS_PULSE   0       /* 0 = hold CS low (grounded) */

/* ---- Bit-bang SPI primitives (SPI mode 0: CPOL=0, CPHA=0) ---- */
#define SCK_LOW()   GPIOA_ResetBits(PIN_SCK)
#define SCK_HIGH()  GPIOA_SetBits(PIN_SCK)
#define MOSI_LOW()  GPIOA_ResetBits(PIN_MOSI)
#define MOSI_HIGH() GPIOA_SetBits(PIN_MOSI)
#define DC_LOW()    GPIOB_ResetBits(PIN_DC)
#define DC_HIGH()   GPIOB_SetBits(PIN_DC)
#define BL_LOW()    GPIOB_ResetBits(PIN_BL)  /* raw: pin LOW */
#define BL_HIGH()   GPIOB_SetBits(PIN_BL)    /* raw: pin HIGH */
#if NV3007_BL_ACTIVE_HIGH
#define BL_ON()     BL_HIGH()
#define BL_OFF()    BL_LOW()
#else
#define BL_ON()     BL_LOW()
#define BL_OFF()    BL_HIGH()
#endif
#define CS_LOW()    do { } while (0)   /* CS tied to GND; PA11 released */
#define CS_HIGH()   do { } while (0)

#if NV3007_RST_GPIO
#define RST_LOW()   GPIOA_ResetBits(PIN_RST)
#define RST_HIGH()  GPIOA_SetBits(PIN_RST)
#endif

/* ---- NV3007 commands (MIPI-compatible subset) ---- */
#define NV3007_SWRESET  0x01
#define NV3007_SLPOUT   0x11
#define NV3007_SLPIN    0x10
#define NV3007_INVOFF   0x20
#define NV3007_DISPON   0x29
#define NV3007_DISPOFF  0x28
#define NV3007_CASET    0x2A
#define NV3007_RASET    0x2B
#define NV3007_RAMWR    0x2C
#define NV3007_MADCTL   0x36

/* Init-table markers (0xFD/0xFE are not used by this command set) */
#define NV_DELAY        0xFD   /* next byte = delay in 10 ms units */
#define NV_END          0xFE

/*
 * NV3007 vendor init sequence - from Arduino_GFX nv3007_init_operations
 * (same source as the ESPHome 142x428 recipe; LVGL lv_nv3007.c is the
 * 2.79" 279 variant and differs in gamma - do NOT mix them).
 */
#if NV3007_INIT_VARIANT == 0
static const uint8_t nv3007_init[] = {
    0xFF, 1, 0xA5,                 /* vendor-specific command mode entry */
    NV3007_SLPOUT, 0,              /* sleep out */
    NV_DELAY, 12,                  /* 120 ms */
    0xFF, 1, 0xA5,                 /* re-enter vendor mode */
    0x9A, 1, 0x08,
    0x9B, 1, 0x08,
    0x9C, 1, 0xB0,
    0x9D, 1, 0x17,
    0x9E, 1, 0xC2,
    0x8F, 2, 0x22, 0x04,
    0x84, 1, 0x90,
    0x83, 1, 0x7B,
    0x85, 1, 0x4F,
    /* gamma */
    0x6E, 1, 0x0F,  0x7E, 1, 0x0F,
    0x60, 1, 0x00,  0x70, 1, 0x00,
    0x6D, 1, 0x39,  0x7D, 1, 0x31,
    0x61, 1, 0x0A,  0x71, 1, 0x0A,
    0x6C, 1, 0x35,  0x7C, 1, 0x29,
    0x62, 1, 0x0F,  0x72, 1, 0x0F,
    0x68, 1, 0x4F,  0x78, 1, 0x45,
    0x66, 1, 0x33,  0x76, 1, 0x33,
    0x6B, 1, 0x14,  0x7B, 1, 0x14,
    0x63, 1, 0x09,  0x73, 1, 0x09,
    0x6A, 1, 0x13,  0x7A, 1, 0x16,
    0x64, 1, 0x08,  0x74, 1, 0x08,
    0x69, 1, 0x07,  0x79, 1, 0x0D,
    0x65, 1, 0x05,  0x75, 1, 0x05,
    0x67, 1, 0x33,  0x77, 1, 0x33,
    0x6F, 1, 0x00,  0x7F, 1, 0x00,
    /* timing */
    0x50, 1, 0x00,
    0x52, 1, 0xD6,
    0x53, 1, 0x04,
    0x54, 1, 0x04,
    0x55, 1, 0x1B,
    0x56, 1, 0x1B,
    /* display control */
    0xA0, 3, 0x2A, 0x24, 0x00,
    0xA1, 1, 0x84,
    0xA2, 1, 0x85,
    0xA8, 1, 0x34,
    0xA9, 1, 0x80,
    0xAA, 1, 0x73,
    0xAB, 2, 0x03, 0x61,
    0xAC, 2, 0x03, 0x65,
    0xAD, 2, 0x03, 0x60,
    0xAE, 2, 0x03, 0x64,
    0xB9, 1, 0x82,
    0xBA, 1, 0x83,
    0xBB, 1, 0x80,
    0xBC, 1, 0x81,
    0xBD, 1, 0x02,
    0xBE, 1, 0x01,
    0xBF, 1, 0x04,
    /* power control */
    0xC0, 1, 0x03,
    0xC4, 1, 0x33,
    0xC5, 1, 0x80,
    0xC6, 1, 0x73,
    0xC7, 1, 0x00,
    0xC8, 2, 0x33, 0x33,
    0xC9, 1, 0x5B,
    0xCA, 1, 0x5A,
    0xCB, 1, 0x5D,
    0xCC, 1, 0x5C,
    0xCD, 2, 0x33, 0x33,
    0xCE, 1, 0x5F,
    0xCF, 1, 0x5E,
    0xD0, 1, 0x61,
    0xD1, 1, 0x60,
    0xB0, 4, 0x3A, 0x3A, 0x00, 0x00,
    0xB6, 1, 0x32,
    0xB7, 1, 0x80,
    0xB8, 1, 0x73,
    /* color correction */
    0xE0, 1, 0x00,
    0xE1, 2, 0x03, 0x0F,
    0xE2, 1, 0x04,
    0xE3, 1, 0x01,
    0xE4, 1, 0x0E,
    0xE5, 1, 0x01,
    0xE6, 1, 0x19,
    0xE7, 1, 0x10,
    0xE8, 1, 0x10,
    0xE9, 1, 0x21,
    0xEA, 1, 0x12,
    0xEB, 1, 0xD0,
    0xEC, 1, 0x04,
    0xED, 1, 0x07,
    0xEE, 1, 0x07,
    0xEF, 1, 0x09,
    0xF0, 1, 0xD0,
    0xF1, 1, 0x0E,
    0xF9, 1, 0x56,
    0xF2, 4, 0x26, 0x1B, 0x0B, 0x20,
    0xEC, 1, 0x04,
    /* TE + vendor exit */
    0x35, 1, 0x00,
    0x44, 2, 0x00, 0x10,
    0x46, 1, 0x10,
    0xFF, 1, 0x00,                 /* exit vendor-specific mode */
    0x3A, 1, 0x05,                 /* 16bpp RGB565 */
    NV3007_SLPOUT, 0,
    NV_DELAY, 20,                  /* 200 ms */
    NV_END
};
#else  /* init variant 1: seller T279VJ-C10-01 (2.79" 142x428) sequence */
static const uint8_t nv3007_init[] = {
    0xFF, 1, 0xA5,                 /* vendor-specific command mode entry */
    0x9A, 1, 0x08,
    0x9B, 1, 0x08,
    0x9C, 1, 0xB0,
    0x9D, 1, 0x16,
    0x9E, 1, 0xC4,
    0x8F, 2, 0x55, 0x04,
    0x84, 1, 0x90,
    0x83, 1, 0x7B,
    0x85, 1, 0x33,
    /* gamma */
    0x60, 1, 0x00,  0x70, 1, 0x00,
    0x61, 1, 0x02,  0x71, 1, 0x02,
    0x62, 1, 0x04,  0x72, 1, 0x04,
    0x6C, 1, 0x29,  0x7C, 1, 0x29,
    0x6D, 1, 0x31,  0x7D, 1, 0x31,
    0x6E, 1, 0x0F,  0x7E, 1, 0x0F,
    0x66, 1, 0x21,  0x76, 1, 0x21,
    0x68, 1, 0x3A,  0x78, 1, 0x3A,
    0x63, 1, 0x07,  0x73, 1, 0x07,
    0x64, 1, 0x05,  0x74, 1, 0x05,
    0x65, 1, 0x02,  0x75, 1, 0x02,
    0x67, 1, 0x23,  0x77, 1, 0x23,
    0x69, 1, 0x08,  0x79, 1, 0x08,
    0x6A, 1, 0x13,  0x7A, 1, 0x13,
    0x6B, 1, 0x13,  0x7B, 1, 0x13,
    0x6F, 1, 0x00,  0x7F, 1, 0x00,
    /* timing */
    0x50, 1, 0x00,
    0x52, 1, 0xD6,
    0x53, 1, 0x08,
    0x54, 1, 0x08,
    0x55, 1, 0x1E,
    0x56, 1, 0x1C,
    /* display control */
    0xA0, 3, 0x2B, 0x24, 0x00,
    0xA1, 1, 0x87,
    0xA2, 1, 0x86,
    0xA5, 1, 0x00,
    0xA6, 1, 0x00,
    0xA7, 1, 0x00,
    0xA8, 1, 0x36,
    0xA9, 1, 0x7E,
    0xAA, 1, 0x7E,
    0xB9, 1, 0x85,
    0xBA, 1, 0x84,
    0xBB, 1, 0x83,
    0xBC, 1, 0x82,
    0xBD, 1, 0x81,
    0xBE, 1, 0x80,
    0xBF, 1, 0x01,
    /* power control */
    0xC0, 1, 0x02,
    0xC1, 1, 0x00,
    0xC2, 1, 0x00,
    0xC3, 1, 0x00,
    0xC4, 1, 0x33,
    0xC5, 1, 0x7E,
    0xC6, 1, 0x7E,
    0xC8, 2, 0x33, 0x33,
    0xC9, 1, 0x68,
    0xCA, 1, 0x69,
    0xCB, 1, 0x6A,
    0xCC, 1, 0x6B,
    0xCD, 2, 0x33, 0x33,
    0xCE, 1, 0x6C,
    0xCF, 1, 0x6D,
    0xD0, 1, 0x6E,
    0xD1, 1, 0x6F,
    0xAB, 2, 0x03, 0x67,
    0xAC, 2, 0x03, 0x6B,
    0xAD, 2, 0x03, 0x68,
    0xAE, 2, 0x03, 0x6C,
    0xB3, 1, 0x00,
    0xB4, 1, 0x00,
    0xB5, 1, 0x00,
    0xB6, 1, 0x32,
    0xB7, 1, 0x7E,
    0xB8, 1, 0x7E,
    /* color correction */
    0xE0, 1, 0x00,
    0xE1, 2, 0x03, 0x0F,
    0xE2, 1, 0x04,
    0xE3, 1, 0x01,
    0xE4, 1, 0x0E,
    0xE5, 1, 0x01,
    0xE6, 1, 0x19,
    0xE7, 1, 0x10,
    0xE8, 1, 0x10,
    0xEA, 1, 0x12,
    0xEB, 1, 0xD0,
    0xEC, 1, 0x04,
    0xED, 1, 0x07,
    0xEE, 1, 0x07,
    0xEF, 1, 0x09,
    0xF0, 1, 0xD0,
    0xF1, 2, 0x0E, 0x17,   /* vendor code sends 0x17 as extra DATA (no 0xF9 cmd) */
    0xF2, 4, 0x2C, 0x1B, 0x0B, 0x20,
    0xE9, 1, 0x29,
    0xEC, 1, 0x04,
    /* TE + vendor exit */
    0x35, 1, 0x00,
    0x44, 2, 0x00, 0x10,
    0x46, 1, 0x10,
    0xFF, 1, 0x00,                 /* exit vendor-specific mode */
    0x3A, 1, 0x05,                 /* 16bpp RGB565 */
    NV3007_SLPOUT, 0,
    NV_DELAY, 22,                  /* 220 ms */
    NV_END
};
#endif

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
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0x30 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 0x31 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 0x32 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 0x33 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 0x34 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 0x35 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 0x36 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 0x37 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 0x38 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 0x39 9 */
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

/* ---- Static helpers ---- */

/**
 * @brief   Send one byte via bit-bang SPI, MSB first, SPI mode 0
 *          (CPOL=0, CPHA=0: SCK idles LOW, data set while SCK is low,
 *          sampled on the rising edge).  B0.8.8 fast path writes the
 *          whole PA_OUT latch once per SCK edge (no RMW); set
 *          NV3007_SLOW_SPI=1 to restore the per-bit NOP debug path.
 */
static void SPI_WriteByte(uint8_t data)
{
#if NV3007_CS_PULSE
    CS_LOW();
#endif
#if NV3007_SLOW_SPI
    /* Debug path: one port op per edge with per-bit NOP delays. */
#define NV3007_BIT_DELAY \
        __nop(); __nop(); __nop(); __nop(); \
        __nop(); __nop(); __nop(); __nop(); \
        __nop(); __nop(); __nop(); __nop();
#define SPI_BIT(bit)                                          \
        R32_PA_CLR = PIN_SCK;                                 \
        if (data & (1u << (7u - (bit))))                      \
            R32_PA_OUT |= PIN_MOSI;                           \
        else                                                  \
            R32_PA_CLR = PIN_MOSI;                            \
        R32_PA_OUT |= PIN_SCK;                                \
        NV3007_BIT_DELAY
    SPI_BIT(0); SPI_BIT(1); SPI_BIT(2); SPI_BIT(3);
    SPI_BIT(4); SPI_BIT(5); SPI_BIT(6); SPI_BIT(7);
#undef SPI_BIT
#undef NV3007_BIT_DELAY
    R32_PA_CLR = PIN_SCK;             /* SCK idle LOW (true mode 0, seller waveform) */
#else
    /* Fast path (B0.8.8): two full-port writes per bit, no RMW.
     * PA9=SCK and PA8=MOSI are the only active PA outputs during a
     * display flush (matrix rows are inputs), so writing the whole
     * PA_OUT latch is safe and roughly 1.5x faster than the RMW path. */
    {
        uint32_t base = R32_PA_OUT & ~(PIN_SCK | PIN_MOSI);
        uint32_t d = data;
        uint32_t sck = PIN_SCK;
        uint32_t mos;
        uint32_t msk = 0x80u;
        /* Fully unrolled: no loop counter/shift/branch per bit. */
#define SPI_FAST_BIT() \
        mos = (d & msk) ? PIN_MOSI : 0u;         \
        R32_PA_OUT = base | mos;                 \
        R32_PA_OUT = base | mos | sck;           \
        d <<= 1
        SPI_FAST_BIT();
        SPI_FAST_BIT();
        SPI_FAST_BIT();
        SPI_FAST_BIT();
        SPI_FAST_BIT();
        SPI_FAST_BIT();
        SPI_FAST_BIT();
        SPI_FAST_BIT();
#undef SPI_FAST_BIT
        R32_PA_OUT = base;                       /* SCK idle low (mode 0) */
    }
#endif
#if NV3007_CS_PULSE
    CS_HIGH();
#endif
}
static void NV3007_WriteCmd(uint8_t cmd)
{
    DC_LOW();
    __nop(); __nop(); __nop(); __nop();   /* DC setup time */
    SPI_WriteByte(cmd);
    DC_HIGH();
}

static void NV3007_WriteData(uint8_t data)
{
    DC_HIGH();
    __nop(); __nop(); __nop(); __nop();   /* DC setup time */
    SPI_WriteByte(data);
}

static void NV3007_WriteData16(uint16_t data)
{
    DC_HIGH();
    __nop(); __nop(); __nop(); __nop();   /* DC setup time */
    SPI_WriteByte((uint8_t)(data >> 8));
    SPI_WriteByte((uint8_t)(data & 0xFF));
}

/* logical landscape row y (0..141) -> physical GRAM column */
static uint16_t NV3007_MapCol(uint16_t y)
{
#if NV3007_ROT_REV_Y
    return (uint16_t)(NV3007_VIS_X1 - y);       /* y=0 -> col 153 */
#else
    return (uint16_t)(NV3007_VIS_X0 + y);       /* y=0 -> col 12 */
#endif
}

/* ---- Public API ---- */

/* ---- NV3007 reset / init helpers (B0.8.7) ---- */

static void NV3007_SoftReset(void)
{
    uint8_t i;

    /* Byte-boundary sync (8x NOP with DC=0), then MIPI SWRESET.
     * This clears the controller registers, but NOT the GOA/GIP latch
     * state - the reason a clean hardware RST pulse (or an RC-delayed
     * RST) is still preferred on this panel. */
    DC_LOW();
    __nop(); __nop(); __nop(); __nop();
    for (i = 0; i < 8; i++) {
        SPI_WriteByte(0x00);
    }
    NV3007_WriteCmd(NV3007_SWRESET);
    DelayMs(150);
}

static void NV3007_SendInitSequence(void)
{
    const uint8_t *p;

    /* Vendor init sequence (ends with SLPOUT + 200/220 ms) */
    p = nv3007_init;
    while (1) {
        uint8_t cmd = *p++;
        if (cmd == NV_END) break;
        if (cmd == NV_DELAY) {
            DelayMs((uint16_t)(*p++) * 10);
            continue;
        }
        {
            uint8_t len = *p++;
            NV3007_WriteCmd(cmd);
            while (len--) NV3007_WriteData(*p++);
        }
    }
}

void NV3007_Init(void)
{
    /* Configure GPIO pins */
    GPIOA_ModeCfg(PIN_SCK | PIN_MOSI, GPIO_ModeOut_PP_5mA);   /* PA11 released (CS grounded, RST not GPIO-driven) */

    GPIOB_ModeCfg(PIN_DC, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(PIN_BL, GPIO_ModeOut_PP_5mA);

    /* Drive SCK/MOSI low before reset - avoid bus glitches with CS tied low */
    SCK_LOW();
    MOSI_LOW();
#if NV3007_CS_PULSE
    CS_HIGH();
#else
    CS_LOW();
#endif
    DC_LOW();
    BL_ON();
    DelayMs(1);

#if NV3007_RST_GPIO
    /* PA11/PA10-driven panel reset, same as the tft_NV3007 reference
     * (tft_nv3007.c): RST low >=100ms -> high >=120ms, then straight
     * into the vendor sequence.  No SWRESET here: the hardware reset is
     * clean and the reference init does not issue one. */
    RST_LOW();
    DelayMs(100);
    RST_HIGH();
    DelayMs(120);
    NV3007_SendInitSequence();
#else
    /* Panel RST is NOT GPIO-driven (B0.8.7).  Wait for panel VDD and the
     * RST net to settle, then soft-reset + init.  SWRESET clears the
     * controller registers but NOT the GOA/GIP latch state, so a clean
     * cold boot still requires the RST net to be RC-delayed (see README
     * B0.8.7); otherwise the manual-reset behavior remains. */
    DelayMs(NV3007_PWR_SETTLE_MS);

    NV3007_SoftReset();
    NV3007_SendInitSequence();

#if NV3007_INIT_RETRY
    /* If pass 1 was sent while panel RST was still low (slow RC release),
     * the panel runs its own POR afterwards and ignores pass 1.  A second
     * pass after the POR window re-applies the full init sequence. */
    DelayMs(300);
    NV3007_SoftReset();
    NV3007_SendInitSequence();
#endif
#endif

#if NV3007_TWEAK
    /* Crosstalk experiment: re-enter vendor mode and override VCOM /
     * inversion registers before the display is enabled. */
    NV3007_WriteCmd(0xFF);
    NV3007_WriteData(0xA5);
#if NV3007_TWEAK == 1
    NV3007_WriteCmd(0xC5); NV3007_WriteData(0x6E);
    NV3007_WriteCmd(0xC6); NV3007_WriteData(0x6E);
#elif NV3007_TWEAK == 2
    NV3007_WriteCmd(0xC5); NV3007_WriteData(0x8E);
    NV3007_WriteCmd(0xC6); NV3007_WriteData(0x8E);
#elif NV3007_TWEAK == 3
    NV3007_WriteCmd(0xE9); NV3007_WriteData(0x00);
#elif NV3007_TWEAK == 4
    NV3007_WriteCmd(0xE9); NV3007_WriteData(0x01);
#elif NV3007_TWEAK == 5
    NV3007_WriteCmd(0xE9); NV3007_WriteData(0x11);
#endif
    NV3007_WriteCmd(0xFF);
    NV3007_WriteData(0x00);
#endif

    /* Portrait memory order + RGB order; flush_cb does the transpose */
    NV3007_WriteCmd(NV3007_MADCTL);
    NV3007_WriteData(NV3007_ROT_180 ? 0xC0 : 0x00);   /* 180 deg on real hardware */

#if NV3007_DEFER_DISPON
    /* B0.8.8: keep the display OFF - the UI draws the first frame into
     * GRAM, then calls NV3007_DisplayOn().  Skips the ~150ms full-screen
     * black clear and avoids any power-on splash. */
#else
    /* Clear GRAM before DISPON - no power-on splash */
    NV3007_Fill(NV3007_BLACK);

    NV3007_WriteCmd(NV3007_DISPON);
    DelayMs(10);
#endif

#if NV3007_SETTLE_MS
    /* B0.7.4: panel bias settle wait (see NV3007.h) */
    DelayMs(NV3007_SETTLE_MS);
#endif

#if NV3007_DEBUG_PATTERN == 1
    /* Bring-up self-test: solid colors prove SPI/init/window are OK before
     * handing over to the UI.  Each fill uses the same SetWindow+stream
     * path as the row-flush API (one physical column per logical row). */
    NV3007_Fill(NV3007_RED);
    DelayMs(600);
    NV3007_Fill(NV3007_GREEN);
    DelayMs(600);
    NV3007_Fill(NV3007_BLUE);
    DelayMs(600);
    NV3007_Fill(NV3007_WHITE);
    DelayMs(600);
    NV3007_Fill(NV3007_BLACK);
    DelayMs(300);
#elif NV3007_DEBUG_PATTERN == 2
    /* Crosstalk diagnostic: white background + black blocks placed at the
     * home page clock / mode-button positions.  If vertical streaks appear
     * through/next to the black blocks, it is panel crosstalk (VCOM /
     * inversion settings).  If the blocks stay clean, the artifact comes
     * from the UI renderer content itself, not from large dark areas. */
    NV3007_Fill(NV3007_WHITE);
    NV3007_FillRect(14, 12, 226, 40, NV3007_BLACK);    /* clock area */
    NV3007_FillRect(258, 8, 160, 106, NV3007_BLACK);   /* buttons area */
    DelayMs(8000);
#elif NV3007_DEBUG_PATTERN == 3
    /* Orientation diagnostic: black background + 2x2 quadrants in logical
     * landscape coords: TL=RED, TR=GREEN, BL=BLUE, BR=WHITE.
     *  - 2x2 grid on screen          => transpose/mapping is correct
     *  - 4 vertical bars              => screen H/V is swapped
     *  - corners swapped left-right   => image mirrored
     *  - corners swapped top-bottom   => image upside-down */
    NV3007_Fill(NV3007_BLACK);
    NV3007_FillRect(0, 0, 214, 71, NV3007_RED);
    NV3007_FillRect(214, 0, 214, 71, NV3007_GREEN);
    NV3007_FillRect(0, 71, 214, 71, NV3007_BLUE);
    NV3007_FillRect(214, 71, 214, 71, NV3007_WHITE);
    DelayMs(10000);
#elif NV3007_DEBUG_PATTERN == 4
    /* Row-stream content test: push a black/white checkerboard through the
     * exact row-flush path used by the bare-metal UI (varied data in
     * a 428px row buffer).  Clean checker => FlushRow+data is fine and the
     * UI mess comes from bm_ui composition; broken stripes => the SPI data
     * path corrupts non-uniform pixel streams. */
    {
        static uint16_t rowbuf[NV3007_WIDTH];
        uint16_t yy, xx;
        for (yy = 0; yy < NV3007_HEIGHT; yy++) {
            for (xx = 0; xx < NV3007_WIDTH; xx++)
                rowbuf[xx] = (((xx / 20) + (yy / 10)) & 1) ? NV3007_BLACK : NV3007_WHITE;
            NV3007_FlushRow(yy, rowbuf);
        }
        DelayMs(8000);
    }
#elif NV3007_DEBUG_PATTERN == 5
    /* C51-style solid colors through a single full-window stream. */
    {
        static const uint16_t cols[5] = {
            NV3007_RED, NV3007_GREEN, NV3007_BLUE, NV3007_WHITE, NV3007_BLACK
        };
        uint8_t ci;
        for (ci = 0; ci < 5; ci++) {
            NV3007_FillRectWin(0, 0, NV3007_WIDTH, NV3007_HEIGHT, cols[ci]);
            DelayMs(1500);
        }
    }
#elif NV3007_DEBUG_PATTERN == 6
    /* C51-style crosstalk blocks: white bg + black blocks at the home page
     * clock / mode-button positions, all through one-window row-major fills. */
    NV3007_FillRectWin(0, 0, NV3007_WIDTH, NV3007_HEIGHT, NV3007_WHITE);
    NV3007_FillRectWin(14, 12, 226, 40, NV3007_BLACK);   /* clock area */
    NV3007_FillRectWin(258, 8, 160, 106, NV3007_BLACK);  /* buttons area */
    DelayMs(8000);
#endif
}

void NV3007_DisplayOn(void)
{
    NV3007_WriteCmd(NV3007_SLPOUT);     /* wake panel from sleep-in */
    DelayMs(120);
    NV3007_WriteCmd(NV3007_DISPON);
    DelayMs(10);
}

void NV3007_DisplayOff(void)
{
    /* Panel lowest power: display off (0x28) then sleep-in (0x10). */
    NV3007_WriteCmd(NV3007_DISPOFF);
    NV3007_WriteCmd(NV3007_SLPIN);
    DelayMs(10);
}

void NV3007_EnterDeepSleep(void)
{
    /* Same as DisplayOff, then release the SPI/backlight pins to
     * high-impedance input so the push-pull outputs do not keep driving
     * the panel inputs while the MCU sleeps (removes a leakage path into
     * the deep-sleep floor). */
    NV3007_WriteCmd(NV3007_DISPOFF);
    NV3007_WriteCmd(NV3007_SLPIN);
    DelayMs(10);

    /* Release SCK / MOSI / DC / backlight -> high-Z input (no pull). */
    GPIOA_ModeCfg(PIN_SCK | PIN_MOSI, GPIO_ModeIN_Floating);
    GPIOB_ModeCfg(PIN_DC | PIN_BL, GPIO_ModeIN_Floating);
}

void NV3007_ExitDeepSleep(void)
{
    /* Reconfigure the panel pins back to push-pull outputs, backlight OFF
     * (BL_OFF = pin low with NV3007_BL_ACTIVE_HIGH=1), then SLPOUT + DISPON. */
    GPIOA_ModeCfg(PIN_SCK | PIN_MOSI, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(PIN_DC, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(PIN_BL, GPIO_ModeOut_PP_5mA);
    SCK_LOW();
    MOSI_LOW();
    DC_LOW();
    BL_OFF();
    NV3007_WriteCmd(NV3007_SLPOUT);
    DelayMs(120);
    NV3007_WriteCmd(NV3007_DISPON);
    DelayMs(10);
}

void NV3007_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    NV3007_WriteCmd(NV3007_CASET);
    NV3007_WriteData16(x0);
    NV3007_WriteData16(x1);

    NV3007_WriteCmd(NV3007_RASET);
    NV3007_WriteData16(y0);
    NV3007_WriteData16(y1);

    NV3007_WriteCmd(NV3007_RAMWR);
}

void NV3007_WritePixel(uint16_t color)
{
    NV3007_WriteData16(color);
}

void NV3007_Fill(uint16_t color)
{
    NV3007_FillRect(0, 0, NV3007_WIDTH, NV3007_HEIGHT, color);
}

void NV3007_FillDots(uint16_t bg, uint16_t dot, uint8_t step)
{
    uint16_t row, i, col;

    if (step < 2) step = 2;
    /* Each logical row becomes one physical column window, like FillRect */
    for (row = 0; row < NV3007_HEIGHT; row++) {
        col = NV3007_MapCol(row);
        NV3007_SetWindow(col, 0, col, (uint16_t)(NV3007_WIDTH - 1));
        DC_HIGH();
        for (i = 0; i < NV3007_WIDTH; i++) {
            uint16_t c = (((row % step) == 1u) && ((i % step) == 1u)) ? dot : bg;
            SPI_WriteByte((uint8_t)(c >> 8));
            SPI_WriteByte((uint8_t)(c & 0xFF));
        }
    }
}
void NV3007_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t row, i;
    uint16_t col;

    if (x >= NV3007_WIDTH || y >= NV3007_HEIGHT) return;
    if (x + w > NV3007_WIDTH)  w = NV3007_WIDTH - x;
    if (y + h > NV3007_HEIGHT) h = NV3007_HEIGHT - y;

    /* Each logical row becomes one physical column window */
    for (row = 0; row < h; row++) {
        col = NV3007_MapCol((uint16_t)(y + row));
        NV3007_SetWindow(col, x, col, (uint16_t)(x + w - 1));
        DC_HIGH();
        for (i = 0; i < w; i++) {
            SPI_WriteByte((uint8_t)(color >> 8));
            SPI_WriteByte((uint8_t)(color & 0xFF));
        }
    }
}

void NV3007_Flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *buf)
{
    uint32_t row, i;
    uint16_t col;
    const uint8_t *p;

    if (x >= NV3007_WIDTH || y >= NV3007_HEIGHT) return;
    if (x + w > NV3007_WIDTH)  w = NV3007_WIDTH - x;
    if (y + h > NV3007_HEIGHT) h = NV3007_HEIGHT - y;

    /* LVGL top-left (x=0..427, y=0..141).  Each LVGL row (w pixels along
     * logical x) maps to physical rows x..x+w-1 inside ONE physical column.
     * Buffer stays row-major; the panel fills the column top->bottom. */
    for (row = 0; row < h; row++) {
        col = NV3007_MapCol((uint16_t)(y + row));
        NV3007_SetWindow(col, x, col, (uint16_t)(x + w - 1));
        DC_HIGH();
        p = (const uint8_t *)buf + row * (uint32_t)w * 2;
        for (i = 0; i < w; i++) {
            SPI_WriteByte(*p++);   /* MSB first (LVGL LV_COLOR_16_SWAP=1) */
            SPI_WriteByte(*p++);
        }
    }
}

void NV3007_FlushRow(uint16_t y, const uint16_t *buf)
{
    uint16_t i;
    uint16_t col;

    if (y >= NV3007_HEIGHT) return;
    col = NV3007_MapCol(y);
    NV3007_SetWindow(col, 0, col, (uint16_t)(NV3007_WIDTH - 1));
    DC_HIGH();
    for (i = 0; i < NV3007_WIDTH; i++) {
        SPI_WriteByte((uint8_t)(buf[i] >> 8));
        SPI_WriteByte((uint8_t)(buf[i] & 0xFF));
    }
}

/* C51 / seller TFT_Clear-style fill: ONE rectangular window covering the
 * logical rect, streamed row-major (RASET outer, CASET inner).  Logical
 * landscape rect -> physical window: cols 153-(y+h-1)..153-y, rows x..x+w-1.
 * For a solid color the stream order inside the window does not matter. */
void NV3007_FillRectWin(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t cy0, cy1;
    uint32_t n;

    if (x >= NV3007_WIDTH || y >= NV3007_HEIGHT) return;
    if (x + w > NV3007_WIDTH)  w = NV3007_WIDTH - x;
    if (y + h > NV3007_HEIGHT) h = NV3007_HEIGHT - y;
    if (w == 0 || h == 0) return;

    cy0 = (uint16_t)(NV3007_VIS_X1 - (y + h - 1));
    cy1 = (uint16_t)(NV3007_VIS_X1 - y);
    NV3007_SetWindow(cy0, x, cy1, (uint16_t)(x + w - 1));
    DC_HIGH();
    n = (uint32_t)w * h;
    while (n--) {
        SPI_WriteByte((uint8_t)(color >> 8));
        SPI_WriteByte((uint8_t)(color & 0xFF));
    }
}

void NV3007_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= NV3007_WIDTH || y >= NV3007_HEIGHT) return;
    NV3007_SetWindow(NV3007_MapCol(y), x, NV3007_MapCol(y), x);
    NV3007_WriteData16(color);
}

static uint8_t font_zoom = 3;   /* default 3x (15x24) */

void NV3007_SetFontZoom(uint8_t zoom)
{
    if (zoom < 1) zoom = 1;
    font_zoom = zoom;
}

void NV3007_DrawChar(char ch, uint16_t x, uint16_t y, uint16_t color, uint16_t bg)
{
    uint8_t i, j, px, py;
    uint8_t idx;
    uint8_t z   = font_zoom;
    uint8_t cw  = 5 * z;   /* char width  */
    uint8_t chh = 8 * z;   /* char height */

    if (ch < 0x20 || ch > 0x7E) ch = ' ';
    idx = ch - 0x20;

    if (x + cw > NV3007_WIDTH || y + chh > NV3007_HEIGHT) return;

    for (j = 0; j < 8; j++) {
        for (py = 0; py < z; py++) {
            uint16_t col = NV3007_MapCol((uint16_t)(y + j * z + py));
            NV3007_SetWindow(col, x, col, (uint16_t)(x + cw - 1));
            DC_HIGH();
            for (i = 0; i < 5; i++) {
                uint8_t on = (font_5x7[idx][i] & (0x80 >> j)) ? 1 : 0;
                for (px = 0; px < z; px++) {
                    if (on) {
                        SPI_WriteByte((uint8_t)(color >> 8));
                        SPI_WriteByte((uint8_t)(color & 0xFF));
                    } else {
                        SPI_WriteByte((uint8_t)(bg >> 8));
                        SPI_WriteByte((uint8_t)(bg & 0xFF));
                    }
                }
            }
        }
    }
}

void NV3007_DrawString(const char *str, uint16_t x, uint16_t y, uint16_t color, uint16_t bg)
{
    uint8_t z   = font_zoom;
    uint8_t cw  = 5 * z;
    uint8_t chh = 8 * z;

    while (*str) {
        NV3007_DrawChar(*str, x, y, color, bg);
        x += cw + z;  /* char + spacing */
        if (x + cw > NV3007_WIDTH) {
            x = 0;
            y += chh + z;
        }
        str++;
    }
}

void NV3007_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    uint16_t i;
    uint16_t col;
    if (x >= NV3007_WIDTH || y >= NV3007_HEIGHT) return;
    if (x + w > NV3007_WIDTH) w = NV3007_WIDTH - x;

    col = NV3007_MapCol(y);
    NV3007_SetWindow(col, x, col, (uint16_t)(x + w - 1));
    DC_HIGH();
    for (i = 0; i < w; i++) {
        SPI_WriteByte((uint8_t)(color >> 8));
        SPI_WriteByte((uint8_t)(color & 0xFF));
    }
}

void NV3007_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    uint16_t row;
    if (x >= NV3007_WIDTH || y >= NV3007_HEIGHT) return;
    if (y + h > NV3007_HEIGHT) h = NV3007_HEIGHT - y;

    for (row = 0; row < h; row++) {
        uint16_t col = NV3007_MapCol((uint16_t)(y + row));
        NV3007_SetWindow(col, x, col, x);
        DC_HIGH();
        SPI_WriteByte((uint8_t)(color >> 8));
        SPI_WriteByte((uint8_t)(color & 0xFF));
    }
}

void NV3007_SetBrightness(uint8_t level)
{
    /* GPIO backlight (simple on/off); polarity from the BL polarity macro */
    if (level)
        BL_ON();
    else
        BL_OFF();
}
