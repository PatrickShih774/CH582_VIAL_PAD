#ifndef SIM_ST7789_H
#define SIM_ST7789_H

#include <stdint.h>

/* 428x142 logical landscape framebuffer (RGB565, row-major).
 * The SDL main loop reads this buffer every frame. */
extern uint16_t sim_fb[428 * 142];

/* Set to 1 by any ST7789_* draw call; the main loop clears it after
 * uploading the framebuffer to the SDL texture. */
extern volatile int g_sim_dirty;

#endif /* SIM_ST7789_H */
