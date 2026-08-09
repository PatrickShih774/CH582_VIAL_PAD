/*
 * st7789.h
 *
 * ST7789_* API compatibility header - hardware is now a NV3007 142x428
 * color TFT (file name / API prefix kept so the MRS build system and all
 * call sites keep working without regenerating the project).
 *
 * NV3007 (NewVision) 168RGBx428 single-chip TFT driver, 4-line SPI
 * (SCK/MOSI/DC, CS tied low), SPI mode 0, RGB565 (16bpp, MSB first).
 *
 * Logical orientation: LANDSCAPE 428x142 (panel rotated 270 deg).
 *   logical x (0..427) -> physical row/page 0..427
 *   logical y (0..141) -> physical column 153 - y   (visible cols 12..153)
 *
 * Pin mapping (unchanged from the ST7789 board):
 *   PA9  = SCK  (bit-bang clock)
 *   PA8  = MOSI (bit-bang data)
 *   PB7  = DC   (data/command)
 *   PB4  = BL   (backlight; polarity selectable via ST7789_BL_ACTIVE_HIGH)
 *   CS   = GND  (tied low, only SPI device)
 *   RST  = PB23 (shared with MCU reset net - driver does not drive it)
 */

#ifndef HAL_ST7789_H_
#define HAL_ST7789_H_

#include <stdint.h>

/* ---- Logical (landscape) resolution: 428x142 ---- */
#define ST7789_WIDTH     428
#define ST7789_HEIGHT    142

/* ---- NV3007 GRAM / visible panel geometry ----
 * GRAM is 168 cols x 428 rows.  The 142-column panel window starts at
 * column 12 (ESPHome offset_width=12 / Arduino_GFX col offset 12..14). */
#define ST7789_PANEL_W   168
#define ST7789_PANEL_H   428
#define ST7789_VIS_X0    12
#define ST7789_VIS_X1    153

/* Orientation switch (0/1). 1 = rotation 270 (logical y=0 -> physical
 * col 153), 0 = rotation 90 (logical y=0 -> physical col 12).  If the
 * image comes out vertically mirrored, flip this bit. */
#define ST7789_ROT_REV_Y 1

/* NV3007 init-sequence variant (bring-up debugging, B0.7.2).
 *   0 = Arduino_GFX default (1.65"/1.68" 142x428 panel)   <- default
 *   1 = LVGL lv_nv3007.c (2.79" 142x428 panel, "279" gamma)
 * If the solid-color self-test is garbled, try the other variant. */
#define ST7789_INIT_VARIANT 0

/* Bring-up self-test (B0.7.2): after init, fill solid RED -> GREEN -> BLUE
 * -> WHITE -> BLACK (600 ms each), then hand over to LVGL.
 *   1 = enabled (use while debugging a garbled / blank panel)
 *   0 = disabled (normal boot, set to 0 once the panel is verified) */
#define ST7789_DEBUG_PATTERN 1

/* Backlight polarity.
 *   1 = ACTIVE-HIGH (PB4 high = backlight ON)  - NV3007 1.68" modules,
 *       matches the LVGL NV3007 Arduino example (BL HIGH to turn on).
 *   0 = ACTIVE-LOW  (PB4 low = backlight ON)   - old ST7789 2.25" board.
 * If the screen stays dark, measure PB4 while running: it must be HIGH
 * with ST7789_BL_ACTIVE_HIGH=1.  Flip this bit if the module is inverted. */
#define ST7789_BL_ACTIVE_HIGH 1

/* ---- Colors (RGB565) ---- */
#define ST7789_BLACK      0x0000
#define ST7789_WHITE      0xFFFF
#define ST7789_RED        0xF800
#define ST7789_GREEN      0x07E0
#define ST7789_BLUE       0x001F
#define ST7789_CYAN       0x07FF
#define ST7789_MAGENTA    0xF81F
#define ST7789_YELLOW     0xFFE0
#define ST7789_ORANGE     0xFD20

/* ---- Public API (kept ST7789_* for compatibility) ---- */

/**
 * @brief   Initialize the NV3007 and configure for 428x142 landscape.
 *          Configures GPIO pins, sends the vendor init sequence, clears
 *          GRAM and enables the display.
 */
void ST7789_Init(void);

/**
 * @brief   Set display window (physical column/page address range).
 * @param   x0, y0  Start coordinate (inclusive)
 * @param   x1, y1  End coordinate (inclusive)
 */
void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief   Push a single pixel (RGB565) at the current cursor position.
 */
void ST7789_WritePixel(uint16_t color);

/**
 * @brief   Bulk-flush an RGB565 framebuffer region (LVGL flush_cb).
 *          Takes LVGL top-left landscape coords (x=0..427, y=0..141) and
 *          transposes each logical row into one physical column window,
 *          so the NV3007 receives the buffer without a rotation register.
 *          Buffer bytes are sent in memory order (LVGL LV_COLOR_16_SWAP=1
 *          stores MSB-first, matching the panel).
 */
void ST7789_Flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *buf);

/**
 * @brief   Fill the entire screen with one color.
 */
void ST7789_Fill(uint16_t color);

/**
 * @brief   Fill a rectangular region with one color (landscape coords).
 */
void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief   Draw a single pixel at (x, y) (landscape coords).
 */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief   Draw a character (5x7 font) at (x, y) in given color.
 */
void ST7789_DrawChar(char ch, uint16_t x, uint16_t y, uint16_t color, uint16_t bg);

/**
 * @brief   Draw a null-terminated string at (x, y).
 */
void ST7789_DrawString(const char *str, uint16_t x, uint16_t y, uint16_t color, uint16_t bg);

/**
 * @brief   Set font zoom factor (1 = 5x8, 2 = 10x16, 3 = 15x24).
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
 * @brief   Set backlight (0 = off, nonzero = on; GPIO active-low).
 */
void ST7789_SetBrightness(uint8_t level);

#endif /* HAL_ST7789_H_ */
