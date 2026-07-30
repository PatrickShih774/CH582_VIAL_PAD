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

#define row_0 GPIO_Pin_1
#define row_1 GPIO_Pin_2
#define row_2 GPIO_Pin_3
#define row_3 GPIO_Pin_15
#define row_4 GPIO_Pin_14
#define row_5 GPIO_Pin_13

#define col_0 GPIO_Pin_0
#define col_1 GPIO_Pin_1
#define col_2 GPIO_Pin_2
#define col_3 GPIO_Pin_3
#define col_4 GPIO_Pin_4
#define col_5 GPIO_Pin_5
#define col_6 GPIO_Pin_6
#define col_7 GPIO_Pin_7

#define row_all row_0|row_1|row_2|row_3|row_4|row_5  //6�� row PAϵ��
#define col_all col_0|col_1|col_2|col_3              //4�� col PBϵ��
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
