/*
 * scan_key.h
 *
 *  Created on: 2024��11��6��
 *      Author: OWNER
 */

#ifndef INCLUDE_SCAN_KEY_H_
#define INCLUDE_SCAN_KEY_H_

extern uint8_t scan_flag;
extern uint8_t scan_buf[6];
extern uint8_t last_buf[6];
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
extern uint8_t key_data_buf[6][4];
extern uint8_t key_data_buf_1[6][4];
extern uint8_t key_data_buf_2[6][4];
extern uint8_t key_data_buf_3[6][4];
//extern uint8_t flash_key_data[192];
extern void get_key(uint8_t *buf);
extern uint8_t get_key_fanz(uint8_t *buf);
extern void Scan_init(void);
BOOL find_mode_changekey(uint8_t arr[], uint8_t size, uint8_t num1, uint8_t num2, uint8_t num3);
#endif /* INCLUDE_SCAN_KEY_H_ */
