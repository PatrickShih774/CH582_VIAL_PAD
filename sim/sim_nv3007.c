/*
 * sim_nv3007.c - PC simulator backend for the NV3007 driver API.
 *
 * Implements the same NV3007_* API surface as HAL/NV3007.c (which is
 * bit-bang SPI on the CH582) by drawing directly into a 428x142 RGB565
 * framebuffer.  bm_ui.c / bm_font.c are compiled unmodified with BM_SIM.
 */

#include "sim_nv3007.h"
#include "NV3007.h"

#include <windows.h>
#include <string.h>

uint16_t sim_fb[428 * 142];
volatile int g_sim_dirty = 0;

/* Physical window state.  On the NV3007 the window parameters are
 * (column, page): bm_ui direct writers pass column = 153 - logical_y
 * and page = logical_x.  The cursor streams columns first, then pages,
 * exactly like the panel RAMWR. */
static uint16_t g_c0, g_r0, g_c1, g_r1;
static uint16_t g_cc, g_rc;

static void sim_mark(void)
{
    g_sim_dirty = 1;
}

void NV3007_Init(void)
{
    memset(sim_fb, 0, sizeof(sim_fb));
    sim_mark();
}

void NV3007_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    g_c0 = x0; g_r0 = y0;
    g_c1 = x1; g_r1 = y1;
    g_cc = x0; g_rc = y0;
}

void NV3007_WritePixel(uint16_t color)
{
    if (g_cc <= NV3007_VIS_X1 && g_rc <= 427) {
        uint16_t lx = g_rc;                       /* logical x = page */
        uint16_t ly = (uint16_t)(NV3007_VIS_X1 - g_cc); /* logical y = 153 - column */
        if (lx < 428 && ly < 142) {
            sim_fb[ly * 428 + lx] = color;
            sim_mark();
        }
    }
    if (g_cc < g_c1) {
        g_cc++;
    } else {
        g_cc = g_c0;
        if (g_rc < g_r1) g_rc++;
    }
}

void NV3007_Flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *buf)
{
    uint16_t r;
    for (r = 0; r < h; r++) {
        if (y + r >= 142 || x >= 428) continue;
        memcpy(&sim_fb[(y + r) * 428 + x], &buf[r * w], (size_t)w * 2u);
    }
    sim_mark();
}

void NV3007_FlushRow(uint16_t y, const uint16_t *buf)
{
    if (y >= 142) return;
    memcpy(&sim_fb[y * 428], buf, 428u * 2u);
    sim_mark();
}

void NV3007_Fill(uint16_t color)
{
    uint32_t i;
    for (i = 0; i < 428u * 142u; i++) sim_fb[i] = color;
    sim_mark();
}

void NV3007_FillDots(uint16_t bg, uint16_t dot, uint8_t step)
{
    uint16_t yy, xx;
    if (step < 2) step = 2;
    for (yy = 0; yy < 142; yy++)
        for (xx = 0; xx < 428; xx++)
            sim_fb[yy * 428 + xx] =
                (((yy % step) == 1u) && ((xx % step) == 1u)) ? dot : bg;
    sim_mark();
}
void NV3007_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t yy, xx;
    if (x >= 428 || y >= 142) return;
    if (x + w > 428) w = 428 - x;
    if (y + h > 142) h = 142 - y;
    for (yy = 0; yy < h; yy++)
        for (xx = 0; xx < w; xx++)
            sim_fb[(y + yy) * 428 + (x + xx)] = color;
    sim_mark();
}

void NV3007_FillRectWin(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    NV3007_FillRect(x, y, w, h, color);
}

void NV3007_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= 428 || y >= 142) return;
    sim_fb[y * 428 + x] = color;
    sim_mark();
}

void NV3007_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    NV3007_FillRect(x, y, w, 1, color);
}

void NV3007_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    NV3007_FillRect(x, y, 1, h, color);
}

/* The bare-metal UI only uses the direct window/text renderers, so the
 * legacy character API can be a no-op in the simulator. */
void NV3007_SetFontZoom(uint8_t zoom) { (void)zoom; }
void NV3007_DrawChar(char ch, uint16_t x, uint16_t y, uint16_t color, uint16_t bg)
{ (void)ch; (void)x; (void)y; (void)color; (void)bg; }
void NV3007_DrawString(const char *str, uint16_t x, uint16_t y, uint16_t color, uint16_t bg)
{ (void)str; (void)x; (void)y; (void)color; (void)bg; }

void NV3007_SetBrightness(uint8_t level)
{
    (void)level;   /* window brightness is not simulated */
}

void DelayMs(uint16_t ms)
{
    Sleep((DWORD)ms);
}
