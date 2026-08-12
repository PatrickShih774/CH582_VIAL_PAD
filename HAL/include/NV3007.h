/*
 * NV3007.h
 *
 * NV3007 driver header - API names kept as ST7789_* for old call-site compatibility
 * color TFT (API prefix kept so all call sites keep working without
 * regenerating the project).
 *
 * NV3007 (NewVision) 168RGBx428 single-chip TFT driver, 4-line SPI
 * (SCK/MOSI/DC, CS tied low), SPI mode 0, RGB565 (16bpp, MSB first).
 *
 * Logical orientation: LANDSCAPE 428x142 (panel rotated 270 deg).
 *   logical x (0..427) -> physical row/page 0..427
 *   logical y (0..141) -> physical column 153 - y   (visible cols 12..153)
 *
 * Pin mapping (unchanged from the previous board):
 *   PA9  = SCK  (bit-bang clock)
 *   PA8  = MOSI (bit-bang data)
 *   PB7  = DC   (data/command)
 *   PB4  = BL   (backlight; polarity selectable via the BL polarity macro)
 *   CS   = GND  (tied low, only SPI device)
 *   RST  = PB23 (shared with MCU reset net, NV3007_RST_GPIO=0)
 *        = PA11 (driver-driven, NV3007_RST_GPIO=1)
 */

#ifndef HAL_NV3007_H_
#define HAL_NV3007_H_

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



/* True hardware rotation 180 (MY|MX): the module is mounted upside-down.
 * Simulator does not compile NV3007.c, so this only affects real hardware. */
#define NV3007_ROT_180 1
/* Orientation switch (0/1). 1 = rotation 270 (logical y=0 -> physical
 * col 153), 0 = rotation 90 (logical y=0 -> physical col 12).  If the
 * image comes out vertically mirrored, flip this bit. */
#define ST7789_ROT_REV_Y 1

/* NV3007 init-sequence variant (bring-up debugging, B0.7.2).
 *   1 = seller T279VJ-C10-01 (2.79" 142x428 panel) sequence  <- default
 *       (verified vendor code: 0xFF 0xA5 vendor mode, 279 gamma,
 *        0xF1 data = 0x0E 0x17, 0x3A=0x05, SLPOUT 220ms, DISPON)
 *   0 = Arduino_GFX default (1.65"/1.68" 142x428 panel, "17" gamma)
 * Use 0 only if the panel is actually the 1.68" variant. */
#define ST7789_INIT_VARIANT 1

/* Dense fine-pattern (text) crosstalk experiments (B0.8):
 *   0 = seller values (C5/C6 = 0x7E/0x7E, E9 = 0x29)
 *   1 = VCOM lower:  C5/C6 = 0x6E/0x6E
 *   2 = VCOM higher: C5/C6 = 0x8E/0x8E
 *   3 = inversion E9 = 0x00
 *   4 = inversion E9 = 0x01
 *   5 = inversion E9 = 0x11
 * Sent in vendor mode after the main init sequence, before DISPON. */
#define NV3007_TWEAK 0

/* Slow down the bit-bang SPI (per-bit NOPs), approximating the seller's
 * 51/STM32 demo speed.  1 = ~1 MHz, 0 = fast (~6 MHz).  Test whether the
 * panel mis-samples data at high SPI speed. */
#define NV3007_SLOW_SPI 0

/* Bring-up self-test / diagnostic (B0.8):
 *   0 = disabled (normal boot)
 *   1 = solid RED -> GREEN -> BLUE -> WHITE -> BLACK (600 ms each)
 *   2 = crosstalk diagnostic: white + black blocks at the home page
 *       clock / button positions (8 s)
 *   3 = orientation diagnostic: 2x2 quadrants (TL=RED, TR=GREEN,
 *       BL=BLUE, BR=WHITE) for 10 s - checks if the landscape transpose
 *       matches the panel display direction
 *   4 = row-stream checkerboard through the row-flush path (the same path
 *       the bare-metal UI uses) - checks if non-uniform pixel data
 *       survives the SPI stream
 *   5 = C51-style solid colors: RED/GREEN/BLUE/WHITE/BLACK (1.5 s each)
 *       through the fill-rect-window path - ONE full rectangular window streamed
 *       row-major, exactly like the seller C51 TFT_Clear (no per-column
 *       windows).  Proves the write-path / SPI-stream order is clean.
 *   6 = C51-style crosstalk blocks: white background + black blocks at the
 *       home page clock / mode-button positions, also via FillRectWin. */
#define ST7789_DEBUG_PATTERN 0

/* Backlight polarity.
 *   1 = ACTIVE-HIGH (PB4 high = backlight ON)  - NV3007 1.68" modules,
 *       matches the LVGL NV3007 Arduino example (BL HIGH to turn on).
 *   0 = ACTIVE-LOW  (PB4 low = backlight ON)   - old 2.25" board.
 * If the screen stays dark, measure PB4 while running: it must be HIGH
 * with the BL polarity macro =1.  Flip this bit if the module is inverted. */
#define ST7789_BL_ACTIVE_HIGH 1

/* Panel RST control.
 *   1 = (default, B0.8) PA11 drives the panel RST with the seller /
 *       tft_NV3007 reference timing (low 100ms -> high 120ms), then the
 *       vendor sequence runs WITHOUT SWRESET - matches tft_nv3007.c.
 *       Hardware: PA11 was the unused CS pin (CS stays grounded); wire
 *       panel RST to PA11.  If RST is still on the MCU reset net, set 0.
 *   0 = RST tied to the MCU reset net (PB23, shared 10K pull-up + 100nF).
 *       The driver does NOT drive it and falls back to a SWRESET after
 *       power-on.  A short MCU-internal reset pulse is not enough for a
 *       clean NV3007 GOA reset (faded band), so cold boot relies on the
 *       manual reset button (or switch to 1). */
#define NV3007_RST_GPIO 0   /* 0 = panel RST not GPIO-driven; PA11 released */

/* Panel settle delay (ms) after DISPON, before the debug pattern / LVGL.
 * B0.7.4 experiment: if the top-to-middle faded band only appears during
 * the first seconds after power-on (e.g. it disappeared after the ~13s
 * LVGL_SOLID_TEST warm-up), wait here for the panel VCOM/GOA bias to
 * stabilize.  0 = disabled.  Try 5000 first, then tune down. */
#define NV3007_SETTLE_MS 0

/* 直写文字/图标行序翻转（B0.9 方向开关）�? *   0 = 当前方向（模拟器已逐像素验证正立）
 *   1 = 上下翻转（仅当真机自检确认所有直写字符颠倒时启用�? *       FillRect/几何方向不受影响，只翻文字与图标字形�?*/
#define NV3007_TEXT_FLIP 1

/* LVGL flush diagnostics (B0.7.4 debug, default off).
 *
 * LVGL_FULL_REFRESH:
 *   0 = normal partial refresh (only changed areas are flushed)
 *   1 = lv_disp_drv.full_refresh = 1: every frame redraws the whole
 *       screen, so every flush window is the full 428-pixel row width.
 *       Use to check whether the "faded rows" come from narrow partial-
 *       width RAMWR windows (clock/status/button updates) or not.
 *
 * LVGL_SOLID_TEST:
 *   0 = normal UI
 *   1 = before loading the UI, show solid WHITE/RED/GREEN/BLUE through
 *       the LVGL flush path (2 s each) - verifies full-width LVGL flush
 *   2 = 1 + draw a 100x40 BLACK block at top-left twice:
 *       first with PARTIAL refresh, then with FULL-screen refresh.
 *       If the faded band appears only in the partial phase, narrow
 *       RAMWR windows are the cause.
 */
#define LVGL_FULL_REFRESH 0
#define LVGL_SOLID_TEST   0

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

/* ---- Public API (names kept from the previous driver for compatibility) ---- */

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
 * @brief   Flush one full logical row (428 px) as a single physical-column
 *          window - same large-window pattern as the solid-color self-test.
 *          Used by the bare-metal UI line-buffer composer (bm_ui.c).
 * @param   y     logical row (0..141)
 * @param   buf   428 RGB565 pixels, high byte first on the wire
 */
void ST7789_FlushRow(uint16_t y, const uint16_t *buf);

/**
 * @brief   Fill the entire screen with one color.
 */
void ST7789_Fill(uint16_t color);

/**
 * @brief   Fill a rectangular region with one color (landscape coords).
 */
void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
/**
 * @brief   Fill the whole screen with a dot-matrix texture: base color with
 *          slightly darker dots every step pixels (LCD pager look).
 *          One window per logical row, same stream as ST7789_FillRect.
 * @param   bg    base RGB565 color
 * @param   dot   RGB565 color for the texture dots
 * @param   step  dot grid spacing (>=2; dots at x%step==1 && y%step==1)
 */
void ST7789_FillDots(uint16_t bg, uint16_t dot, uint8_t step);

/**
 * @brief   Fill a rectangular region via ONE full rectangular window,
 *          streamed row-major (C51 / seller TFT_Clear style).
 */
void ST7789_FillRectWin(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

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

#endif /* HAL_NV3007_H_ */
