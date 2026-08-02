/*
 * st7789.h
 *
 * ST7789V SPI LCD driver for CH582F — 2.25" 76×284, bit-bang SPI.
 *
 * Pin mapping:
 *   PA9  = SCK  (bit-bang clock)
 *   PA8  = MOSI (bit-bang data)
 *   PB7  = DC   (data/command)
 *   PB4  = BL   (backlight, ACTIVE-LOW — low = on)
 *   RST  = floating (no GPIO — uses internal POR + SWRESET)
 *   GND  = CS   (tied low, only SPI device)
 */

#ifndef HAL_ST7789_H_
#define HAL_ST7789_H_

#include <stdint.h>

/* ── Display dimensions (landscape 284×76) ──────────────────────────── */
#define ST7789_WIDTH     284
#define ST7789_HEIGHT    76

/* ── ST7789 framebuffer offsets (seller landscape: C=18, L=82) ──────── */
#define ST7789_COL_OFF   18   /* column offset added in CASET */
#define ST7789_LINE_OFF  82   /* line offset added in RASET */

/* ── Colors (RGB565) ────────────────────────────────────────────────── */
#define ST7789_BLACK      0x0000
#define ST7789_WHITE      0xFFFF
#define ST7789_RED        0xF800
#define ST7789_GREEN      0x07E0
#define ST7789_BLUE       0x001F
#define ST7789_CYAN       0x07FF
#define ST7789_MAGENTA    0xF81F
#define ST7789_YELLOW     0xFFE0
#define ST7789_ORANGE     0xFD20

/* ── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief   Initialize ST7789V and configure for 76×284 portrait.
 *          Configures GPIO pins, sends init sequence, enables display.
 */
void ST7789_Init(void);

/**
 * @brief   Set display window (column and row address range).
 * @param   x0, y0  Start coordinate (inclusive)
 * @param   x1, y1  End coordinate (inclusive)
 */
void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief   Push a single pixel (RGB565) at current cursor position.
 * @param   color  16-bit RGB565 color value
 */
void ST7789_WritePixel(uint16_t color);

/**
 * @brief   Fill the entire screen with one color.
 * @param   color  16-bit RGB565 color value
 */
void ST7789_Fill(uint16_t color);

/**
 * @brief   Fill a rectangular region with one color.
 */
void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief   Draw a single pixel at (x, y).
 */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief   Draw a character (5×7 font) at (x, y) in given color.
 * @param   ch     ASCII character (0x20–0x7E)
 * @param   x, y   Top-left position
 * @param   color  Foreground color (RGB565)
 * @param   bg     Background color (RGB565)
 */
void ST7789_DrawChar(char ch, uint16_t x, uint16_t y, uint16_t color, uint16_t bg);

/**
 * @brief   Draw a null-terminated string at (x, y).
 */
void ST7789_DrawString(const char *str, uint16_t x, uint16_t y, uint16_t color, uint16_t bg);

/**
 * @brief   Set font zoom factor (1 = 5×8, 2 = 10×16, 3 = 15×24).
 */
void ST7789_SetFontZoom(uint8_t zoom);

/**
 * @brief   Draw a horizontal line (fast).
 */
void ST7789_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);

/**
 * @brief   Draw a vertical line (fast).
 */
void ST7789_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);

/**
 * @brief   Set backlight brightness (0–255).
 * @param   level  0 = off, 255 = max brightness
 */
void ST7789_SetBrightness(uint8_t level);

#endif /* HAL_ST7789_H_ */