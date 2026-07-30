/********************************** (C) COPYRIGHT *******************************
 * File Name          : scan_key.c
 * Author             : WCH.yz
 * Version            : V1.0
 * Date               : 2024/11/19
 * Description        : ����ɨ����
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "scan_key.h"
#include "hiddev.h"
#include "VIAL.h"

// ASCII��HID�����ӳ��������֣�
typedef struct {
    char ascii;
    unsigned char hid_code;
} AsciiToHidMapping;

AsciiToHidMapping asciiToHidMap[] = {
    {'a', HID_KEYBOARD_A},
    {'b', HID_KEYBOARD_B},
    {'c', HID_KEYBOARD_C},
    {'d', HID_KEYBOARD_D},
    {'e', HID_KEYBOARD_E},
    {'f', HID_KEYBOARD_F},
    {'g', HID_KEYBOARD_G},
    {'h', HID_KEYBOARD_H},
    {'i', HID_KEYBOARD_I},
    {'j', HID_KEYBOARD_J},
    {'k', HID_KEYBOARD_K},
    {'l', HID_KEYBOARD_L},
    {'m', HID_KEYBOARD_M},
    {'n', HID_KEYBOARD_N},
    {'o', HID_KEYBOARD_O},
    {'p', HID_KEYBOARD_P},
    {'q', HID_KEYBOARD_Q},
    {'r', HID_KEYBOARD_R},
    {'s', HID_KEYBOARD_S},
    {'t', HID_KEYBOARD_T},
    {'u', HID_KEYBOARD_U},
    {'v', HID_KEYBOARD_V},
    {'w', HID_KEYBOARD_W},
    {'x', HID_KEYBOARD_X},
    {'y', HID_KEYBOARD_Y},
    {'z', HID_KEYBOARD_Z},
    {'1', HID_KEYBOARD_1},
    {'2', HID_KEYBOARD_2},
    {'3', HID_KEYBOARD_3},
    {'4', HID_KEYBOARD_4},
    {'5', HID_KEYBOARD_5},
    {'6', HID_KEYBOARD_6},
    {'7', HID_KEYBOARD_7},
    {'8', HID_KEYBOARD_8},
    {'9', HID_KEYBOARD_9},
    {'0', HID_KEYBOARD_0},
    {'\n', HID_KEYBOARD_RETURN}, // ���軻�з���ʾ�س���
    {' ', HID_KEYBOARD_SPACEBAR},
    // ����ӳ���������
    {'\0', 0} // ��ֹ��
};

/*********************************************************************
 * @fn      asciiToHid
 *
 * @brief   ����ASCII�ַ���Ӧ��HID����,��Ҫ���Ӹ����ַ�ֻ���±�����
 *
 * @param   ascii - ������ַ�
 *
 * @return      0 - ��ƥ����
 *             !0 - ƥ�����HID���
 */
unsigned char asciiToHid(char ascii) {
    for (int i = 0; asciiToHidMap[i].ascii != '\0'; i++) {
        if (asciiToHidMap[i].ascii == ascii) {
            return asciiToHidMap[i].hid_code;
        }
    }
    // ���δ�ҵ�ƥ�������0������Ը�����Ҫ���ش����룩
    return 0;
}
uint32_t io_map_col[] = {col_0,col_1,col_2,col_3};
uint32_t io_map_row[] = {row_0,row_1,row_2,row_3,row_4};

uint8_t scan_flag = 0;
uint8_t scan_buf[6] = {0};
uint8_t last_buf[6] = {0};
uint16_t change_mode_BLE = 0;
uint16_t change_mode_24 = 0;
uint16_t change_mode_USB = 0;

uint8_t key_data_buf[5][4]={   //[Y][X]
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
};
uint8_t key_data_buf_1[5][4]={   //[Y][X]
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
};
uint8_t key_data_buf_2[5][4]={   //[Y][X]
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
};
uint8_t key_data_buf_3[5][4]={   //[Y][X]
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
};
/*********************************************************************
 * @fn      Scan_init
 *
 * @brief   ����ɨ����ʼ��
 *
 * @param   none
 *
 * @return  none
 */
void Scan_init(void)
{
    uint8_t i;
    GPIOA_ModeCfg(row_all, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(col_all, GPIO_ModeIN_PU);
    uint8_t data_buf[20];
    /* layer 0 via vial library */
    FLASH_DATA_KEY(data_buf);
    /* convert 0xFF (empty flash) to 0x00 (KC_NO) for Vial compat */
    for (i = 0; i < 20; i++) { if (data_buf[i] == 0xFF) data_buf[i] = 0x00; }
    memcpy(key_data_buf, data_buf, 20);
    /* layers 1–3 via direct EEPROM read (empty flash → 0xFF → KC_NO) */
    EEPROM_READ(0x3014, data_buf, 20);
    for (i = 0; i < 20; i++) { if (data_buf[i] == 0xFF) data_buf[i] = 0x00; }
    memcpy(key_data_buf_1, data_buf, 20);
    EEPROM_READ(0x3028, data_buf, 20);
    for (i = 0; i < 20; i++) { if (data_buf[i] == 0xFF) data_buf[i] = 0x00; }
    memcpy(key_data_buf_2, data_buf, 20);
    EEPROM_READ(0x303C, data_buf, 20);
    for (i = 0; i < 20; i++) { if (data_buf[i] == 0xFF) data_buf[i] = 0x00; }
    memcpy(key_data_buf_3, data_buf, 20);
}

/*********************************************************************
 * @fn      get_key
 *
 * @brief   ��ȡ��ֵ��������Ϊ����ɨ��
 *
 * @param   buf  -  ��ż�ֵ������
 *
 * @return  none
 */
__HIGH_CODE
void get_key(uint8_t *buf)
{
     uint8_t i = 0;
      for (int var = 0; var < 4; ++var) {
          GPIOB_ResetBits(io_map_col[var]);
          __nop();__nop();
          for (int var2 = 0; var2 < 5; ++var2) {
              if (GPIOA_ReadPortPin(io_map_row[var2]) == 0) {
                    if (i>=6) {
                       break;
                    }
                    buf[i] = key_data_buf[var2][var];
                    i++;
                }
          }
          if (i>=6) {
             break;
          }
          GPIOB_SetBits(io_map_col[var]);  //col����
      }
      GPIOB_SetBits(col_all);  //ȫ������
}
/*********************************************************************
 * @fn      get_key_fanz
 *
 * @brief   ��ȡ��ֵ��������Ϊ����ɨ��
 *
 * @param   buf  -  ��ż�ֵ������
 *
 * @return  none
 */
__HIGH_CODE
uint8_t get_key_fanz(uint8_t *buf)
{
     uint8_t i = 0;
      for (int var = 0; var < 5; ++var) {
          GPIOA_ResetBits(io_map_row[var]);
          __nop();__nop();
          for (int var2 = 0; var2 < 4; ++var2) {
              if (GPIOB_ReadPortPin(io_map_col[var2]) == 0) {
                    if (i>=6) {
                       break;
                    }
                    buf[i] = key_data_buf[var][var2];
                    i++;
                }
          }
          if (i>=6) {
             break;
          }
          GPIOA_SetBits(io_map_row[var]);  //ROW����
      }
      GPIOA_SetBits(row_all);  //ROWȫ������
      return i;
}

/*********************************************************************
 * @fn      find_mode_changekey
 *
 * @brief   Ѱ������ģʽ�ı�ı�־
 *
 * @param   arr  - ����ɨ������
 *          size - Ѱ�ҵ��ܳ���
 *          num1 - Ѱ�ҵ�ֵ
 *          num2 - Ѱ�ҵ�ֵ
 *          num3 - Ѱ�ҵ�ֵ
 * @return  0    - δͬʱ�ҵ�
 *          1    - ͬʱ�ҵ�׼���ı�����ģʽ
 */
BOOL find_mode_changekey(uint8_t arr[], uint8_t size, uint8_t num1, uint8_t num2, uint8_t num3) {
    int count = 0;

    for (int i = 0; i < size; i++) {
        if (arr[i] == num1) count++;
        if (arr[i] == num2) count++;
        if (arr[i] == num3) count++;
        // ����Ѿ��ҵ���������������������true
        if (count == 3) {
            return 1;
        }
    }
    // ���û���ҵ����������򷵻�false
    return 0;
}



