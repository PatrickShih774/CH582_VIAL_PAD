/*
 * ui_mem.h
 *
 * LVGL custom memory hooks (B0.6): LV_MEM_CUSTOM=1 routes LVGL allocations
 * to ui_lvgl_* which carve from a mode-selected pool:
 *   USB mode -> 16KB pool (.lvgl_shared, RAM base overlay region)
 *   BLE mode -> 6KB pool (.lvgl_shared_ble, tail of the shared region)
 * Implementation in HAL/lvgl_port.c.
 */

#ifndef HAL_UI_MEM_H_
#define HAL_UI_MEM_H_

#include <stddef.h>

void * ui_lvgl_alloc(size_t size);
void   ui_lvgl_free(void * ptr);
void * ui_lvgl_realloc(void * ptr, size_t new_size);

#endif /* HAL_UI_MEM_H_ */
