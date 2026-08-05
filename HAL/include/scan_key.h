/*
 * scan_key.h
 *
 *  Created on: 2024��11��6��
 *      Author: OWNER
 */

#ifndef INCLUDE_SCAN_KEY_H_
#define INCLUDE_SCAN_KEY_H_

/* ── QMK modifier masks (for uint16_t keycodes) ─────────────────────── */
#define QK_LCTL  0x0100
#define QK_LSFT  0x0200
#define QK_LALT  0x0400
#define QK_LGUI  0x0800
#define QK_RCTL  0x1000
#define QK_RSFT  0x2000
#define QK_RALT  0x4000
#define QK_RGUI  0x8000

/* QMK keycode decomposition helpers */
static inline uint8_t qmk_mods(uint16_t kc) {
    uint8_t m = 0;
    if (kc & 0x0100) m |= 0x01;  /* LCTL */
    if (kc & 0x0200) m |= 0x02;  /* LSFT */
    if (kc & 0x0400) m |= 0x04;  /* LALT */
    if (kc & 0x0800) m |= 0x08;  /* LGUI */
    if (kc & 0x1000) m |= 0x10;  /* RCTL */
    if (kc & 0x2000) m |= 0x20;  /* RSFT */
    if (kc & 0x4000) m |= 0x40;  /* RALT */
    if (kc & 0x8000) m |= 0x80;  /* RGUI */
    return m;
}
static inline uint8_t qmk_usage(uint16_t kc) { return (uint8_t)(kc & 0xFF); }

extern uint8_t scan_flag;
extern uint8_t scan_buf[6];
extern uint8_t last_buf[6];
extern uint8_t scan_modifier;    /* |='d QMK modifier bits during scan */
extern uint16_t change_mode_BLE;
extern uint16_t change_mode_24;
extern uint16_t change_mode_USB;

/* Rows on GPIOA (outputs, active low scan) — layout optimized for PCB routing */
#define row_0 GPIO_Pin_4    /* PA4 */
#define row_1 GPIO_Pin_5    /* PA5 */
#define row_2 GPIO_Pin_15   /* PA15 */
#define row_3 GPIO_Pin_14   /* PA14 */
#define row_4 GPIO_Pin_13   /* PA13 */
#define row_5 GPIO_Pin_12   /* PA12 */

/* Columns on GPIOB (inputs with pull-up) — PB12~PB15 contiguous */
#define col_0 GPIO_Pin_12   /* PB12 */
#define col_1 GPIO_Pin_13   /* PB13 */
#define col_2 GPIO_Pin_14   /* PB14 */
#define col_3 GPIO_Pin_15   /* PB15 */

#define row_all row_0|row_1|row_2|row_3|row_4|row_5  /* 6 rows on PA */
#define col_all col_0|col_1|col_2|col_3              /* 4 cols on PB */
extern uint16_t key_data_buf[6][4];
extern uint16_t key_data_buf_1[6][4];
extern uint16_t key_data_buf_2[6][4];
extern uint16_t key_data_buf_3[6][4];
//extern uint8_t flash_key_data[192];
extern uint8_t get_key(uint8_t *buf);
extern uint8_t get_key_fanz(uint8_t *buf);
extern void Scan_init(void);
BOOL find_mode_changekey(uint8_t arr[], uint8_t size, uint16_t num1, uint16_t num2, uint16_t num3);
#endif /* INCLUDE_SCAN_KEY_H_ */
