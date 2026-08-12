/*
 * main.c - SDL2 simulator for the CH582 NV3007 bare-metal UI.
 *
 * Compiles the real HAL/bm_ui.c + HAL/bm_font.c (with BM_SIM) and drives
 * them with the same key routing as the firmware:
 *   Tab + Backspace  : next page (home -> calc -> settings -> ...)
 *   calc page        : 0-9 + - * / .  Enter(=)  Backspace  Esc(C)
 *   settings page    : 1 brightness  2 sleep  3 theme  4 reset
 *   M                : cycle USB / BT / RF mode
 *   T                : cycle theme from any page (quick check)
 *   B                : cycle brightness from any page
 *   Esc (non-calc)   : quit
 */

#include <SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bm_ui.h"
#include "NV3007.h"
#include "sim_st7789.h"

extern volatile uint32_t g_bm_tick_ms;
extern uint16_t sim_fb[428 * 142];

#define SCALE 3

static const char *page_names[] = { "HOME", "CALC", "SETTINGS" };
static const char *mode_names[] = { "USB", "BT", "RF" };

static int tab_down = 0;
static int bs_down = 0;
static int combo_fired = 0;
static uint8_t sim_mode = UI_MODE_USB;

/* Write sim_fb (RGB565) straight into a 24-bit bottom-up BMP, bypassing
 * the SDL renderer (whose headless output can be offset/mangled). */
static int save_bmp_direct(const char *path)
{
    FILE *fp;
    uint32_t row = 428u * 3u;
    uint8_t hdr[54];
    uint32_t img = row * 142u;
    int y;

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    *(uint32_t *)(hdr + 2) = 54u + img;
    *(uint32_t *)(hdr + 10) = 54u;
    *(uint32_t *)(hdr + 14) = 40u;
    *(int32_t *)(hdr + 18) = 428;
    *(int32_t *)(hdr + 22) = 142;
    *(uint16_t *)(hdr + 26) = 1u;
    *(uint16_t *)(hdr + 28) = 24u;
    *(uint32_t *)(hdr + 34) = img;

    fp = fopen(path, "wb");
    if (!fp) { fprintf(stderr, "bmp open: %s\n", path); return 1; }
    fwrite(hdr, 1, sizeof(hdr), fp);
    for (y = 141; y >= 0; y--) {          /* bottom-up rows */
        uint8_t line[428u * 3u];
        int x;
        for (x = 0; x < 428; x++) {
            uint16_t c = sim_fb[(uint32_t)y * 428u + (uint32_t)x];
            uint8_t r5 = (uint8_t)((c >> 11) & 0x1Fu);
            uint8_t g6 = (uint8_t)((c >> 5) & 0x3Fu);
            uint8_t b5 = (uint8_t)(c & 0x1Fu);
            line[x * 3 + 0] = (uint8_t)((b5 * 255u) / 31u);
            line[x * 3 + 1] = (uint8_t)((g6 * 255u) / 63u);
            line[x * 3 + 2] = (uint8_t)((r5 * 255u) / 31u);
        }
        fwrite(line, 1, row, fp);
    }
    fclose(fp);
    fprintf(stderr, "saved %s\n", path);
    return 0;
}

static char calc_char(SDL_Keycode k)
{
    switch (k) {
        case SDLK_KP_DIVIDE: return '/';
        case SDLK_KP_MULTIPLY: return '*';
        case SDLK_KP_MINUS: return '-';
        case SDLK_KP_PLUS: return '+';
        case SDLK_KP_ENTER:
        case SDLK_RETURN: return '=';
        case SDLK_KP_1: return '1'; case SDLK_KP_2: return '2';
        case SDLK_KP_3: return '3'; case SDLK_KP_4: return '4';
        case SDLK_KP_5: return '5'; case SDLK_KP_6: return '6';
        case SDLK_KP_7: return '7'; case SDLK_KP_8: return '8';
        case SDLK_KP_9: return '9'; case SDLK_KP_0: return '0';
        case SDLK_KP_PERIOD: return '.';
        case SDLK_BACKSPACE: return '\b';
        case SDLK_ESCAPE: return 'C';
        default: return 0;
    }
}

static char main_key_char(SDL_Keycode k)
{
    switch (k) {
        case SDLK_0: return '0'; case SDLK_1: return '1';
        case SDLK_2: return '2'; case SDLK_3: return '3';
        case SDLK_4: return '4'; case SDLK_5: return '5';
        case SDLK_6: return '6'; case SDLK_7: return '7';
        case SDLK_8: return '8'; case SDLK_9: return '9';
        case SDLK_PLUS: return '+'; case SDLK_MINUS: return '-';
        case SDLK_ASTERISK: return '*'; case SDLK_SLASH: return '/';
        case SDLK_PERIOD: return '.';
        case SDLK_RETURN: return '=';
        case SDLK_BACKSPACE: return '\b';
        case SDLK_ESCAPE: return 'C';
        default: return 0;
    }
}

/* Headless mode: render a page into a BMP and exit.
 *   nv3007_sim.exe --shot out.bmp [page] [frames] [theme_clicks] [expr]
 * theme_clicks: number of ui_settings_apply(2) calls (0 = default MinL,
 *               2 = Pixel Green, 3 = Pixel Amber, ...)
 * expr: calculator keys, '=' is appended automatically.
 */
static int headless_shot(const char *path, int page, int frames,
                         int theme_clicks, const char *expr)
{
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    SDL_Surface *surf;
    uint8_t *pixels;
    int i;

    win = SDL_CreateWindow("headless", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, 428, 142,
                           SDL_WINDOW_HIDDEN);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, 428, 142);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565,
                            SDL_TEXTUREACCESS_STREAMING, 428, 142);
    if (!tex) { fprintf(stderr, "texture: %s\n", SDL_GetError()); return 1; }

    ST7789_Init();
    ui_init();
    if (page >= 0 && page < UI_PAGE_COUNT) ui_set_page((ui_page_t)page);
    for (i = 0; i < theme_clicks; i++) ui_settings_apply(2);
    if (expr) {
        for (i = 0; expr[i]; i++) ui_calc_input(expr[i]);
        ui_calc_input('=');
    }
    g_bm_tick_ms = 0;

    for (i = 0; i < frames; i++) {
        g_bm_tick_ms += 1000u / 60u;
        ui_bm_process();
        if (g_sim_dirty) {
            SDL_UpdateTexture(tex, NULL, sim_fb, 428 * 2);
            g_sim_dirty = 0;
        }
    }

    (void)ren; (void)tex; (void)surf; (void)pixels;
    save_bmp_direct(path);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    return 0;
}

static void handle_key(SDL_Keycode k)
{
    if (tab_down && bs_down) return;    /* combo handled separately */

    if (ui_get_page() == UI_PAGE_CALC) {
        char c = main_key_char(k);
        if (!c) c = calc_char(k);
        if (c) ui_calc_input(c);
    } else if (ui_get_page() == UI_PAGE_SETTINGS) {
        if (k >= SDLK_1 && k <= SDLK_4)
            ui_settings_apply((uint8_t)(k - SDLK_1));
    } else if (k == SDLK_m || k == SDLK_f) {
        sim_mode = (uint8_t)((sim_mode + 1) % UI_MODE_COUNT);
        ui_set_mode((ui_mode_t)sim_mode);
    } else if (k == SDLK_t) {
        ui_settings_apply(2);            /* cycle theme */
    } else if (k == SDLK_b) {
        ui_settings_apply(0);            /* cycle brightness */
    } else if (k == SDLK_ESCAPE) {
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    SDL_Event ev;
    int running = 1;
    uint32_t last;

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (argc >= 3 && strcmp(argv[1], "--shot") == 0) {
        int page = argc >= 4 ? atoi(argv[3]) : -1;
        int frames = argc >= 5 ? atoi(argv[4]) : 6;
        int theme = argc >= 6 ? atoi(argv[5]) : 0;
        const char *expr = argc >= 7 ? argv[6] : NULL;
        int rc = headless_shot(argv[2], page, frames, theme, expr);
        SDL_Quit();
        return rc;
    }

    if (argc >= 3 && strcmp(argv[1], "--dir") == 0) {
        SDL_Window *win2;
        SDL_Renderer *ren2;
        SDL_Texture *tex2;
        SDL_Surface *surf2;
        uint8_t *px;
        int rc = 1;
        win2 = SDL_CreateWindow("dir", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, 428, 142,
                                SDL_WINDOW_HIDDEN);
        if (win2) ren2 = SDL_CreateRenderer(win2, -1, SDL_RENDERER_SOFTWARE);
        if (win2 && ren2) {
            SDL_RenderSetLogicalSize(ren2, 428, 142);
            tex2 = SDL_CreateTexture(ren2, SDL_PIXELFORMAT_RGB565,
                                     SDL_TEXTUREACCESS_STREAMING, 428, 142);
            if (tex2) {
                ST7789_Init();
                ui_bm_direction_test();
                SDL_UpdateTexture(tex2, NULL, sim_fb, 428 * 2);
                SDL_RenderClear(ren2);
                SDL_RenderCopy(ren2, tex2, NULL, NULL);
                px = (uint8_t *)malloc(428u * 142u * 3u);
                if (px) {
                    SDL_RenderReadPixels(ren2, NULL, SDL_PIXELFORMAT_RGB24,
                                         px, 428 * 3);
                    surf2 = SDL_CreateRGBSurfaceFrom(px, 428, 142, 24, 428 * 3,
                                                     0x0000FFu, 0x00FF00u, 0xFF0000u, 0);
                    if (surf2) {
                        SDL_SaveBMP(surf2, argv[2]);
                        SDL_FreeSurface(surf2);
                        rc = 0;
                    }
                    free(px);
                }
                SDL_DestroyTexture(tex2);
            }
            SDL_DestroyRenderer(ren2);
        }
        if (win2) SDL_DestroyWindow(win2);
        SDL_Quit();
        return rc;
    }

    win = SDL_CreateWindow("CH582 NV3007 UI Simulator",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           428 * SCALE, 142 * SCALE, 0);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(ren, 428, 142);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565,
                            SDL_TEXTUREACCESS_STREAMING, 428, 142);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return 1;
    }

    ST7789_Init();
    ui_init();
    g_bm_tick_ms = 0;
    last = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.repeat) continue;
                if (ev.key.keysym.sym == SDLK_TAB) tab_down = 1;
                else if (ev.key.keysym.sym == SDLK_BACKSPACE) bs_down = 1;
                handle_key(ev.key.keysym.sym);
            } else if (ev.type == SDL_KEYUP) {
                if (ev.key.keysym.sym == SDLK_TAB) tab_down = 0;
                else if (ev.key.keysym.sym == SDLK_BACKSPACE) bs_down = 0;
                if (!tab_down || !bs_down) combo_fired = 0;
            }
        }

        /* Firmware combo: Tab + Backspace -> next page */
        if (tab_down && bs_down && !combo_fired) {
            combo_fired = 1;
            ui_set_page((ui_page_t)((ui_get_page() + 1) % UI_PAGE_COUNT));
        }

        {
            uint32_t now = SDL_GetTicks();
            uint32_t dt = now - last;
            last = now;
            g_bm_tick_ms += dt;
        }
        ui_bm_process();

        if (g_sim_dirty) {
            SDL_UpdateTexture(tex, NULL, sim_fb, 428 * 2);
            g_sim_dirty = 0;
        }

        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        {
            char title[128];
            snprintf(title, sizeof(title),
                     "NV3007 sim | %s | mode %s | theme %d | brightness %u | Tab+BS page, M mode, T theme, B bright, Esc quit",
                     page_names[ui_get_page()], mode_names[sim_mode],
                     (int)(ui_get_page() == UI_PAGE_SETTINGS), ui_get_brightness());
            SDL_SetWindowTitle(win, title);
        }

        SDL_Delay(4);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
