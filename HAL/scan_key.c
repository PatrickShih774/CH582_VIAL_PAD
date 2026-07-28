/********************************** (C) COPYRIGHT *******************************
 * File Name          : scan_key.c
 * Author             : WCH.yz
 * Version            : V1.0
 * Date               : 2024/11/19
 * Description        : 键盘扫键层
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "scan_key.h"
#include "hiddev.h"
#include "VIAL.h"

// ASCII到HID键码的映射表（部分）
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
    {'\n', HID_KEYBOARD_RETURN}, // 假设换行符表示回车键
    {' ', HID_KEYBOARD_SPACEBAR},
    // 更多映射可以添加
    {'\0', 0} // 终止符
};

/*********************************************************************
 * @fn      asciiToHid
 *
 * @brief   查找ASCII字符对应的HID键码,若要添加更多字符只更新表即可
 *
 * @param   ascii - 待查表字符
 *
 * @return      0 - 无匹配项
 *             !0 - 匹配项的HID输出
 */
unsigned char asciiToHid(char ascii) {
    for (int i = 0; asciiToHidMap[i].ascii != '\0'; i++) {
        if (asciiToHidMap[i].ascii == ascii) {
            return asciiToHidMap[i].hid_code;
        }
    }
    // 如果未找到匹配项，返回0（或可以根据需要返回错误码）
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
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
};
uint8_t key_data_buf_1[5][4]={   //[Y][X]
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
};
uint8_t key_data_buf_2[5][4]={   //[Y][X]
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
};
uint8_t key_data_buf_3[5][4]={   //[Y][X]
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,
};
/*********************************************************************
 * @fn      Scan_init
 *
 * @brief   键盘扫描层初始化
 *
 * @param   none
 *
 * @return  none
 */
void Scan_init(void)
{
    GPIOA_ModeCfg(row_all, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(col_all, GPIO_ModeIN_PU);
    uint8_t data_buf[20];
    FLASH_DATA_KEY(data_buf);
    memcpy(key_data_buf,data_buf,20);
}

/*********************************************************************
 * @fn      get_key
 *
 * @brief   获取键值，本函数为按列扫描
 *
 * @param   buf  -  存放键值的数组
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
          GPIOB_SetBits(io_map_col[var]);  //col拉高
      }
      GPIOB_SetBits(col_all);  //全线拉高
}
/*********************************************************************
 * @fn      get_key_fanz
 *
 * @brief   获取键值，本函数为按行扫描
 *
 * @param   buf  -  存放键值的数组
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
          GPIOA_SetBits(io_map_row[var]);  //ROW拉高
      }
      GPIOA_SetBits(row_all);  //ROW全线拉高
      return i;
}

/*********************************************************************
 * @fn      find_mode_changekey
 *
 * @brief   寻找连接模式改变的标志
 *
 * @param   arr  - 键盘扫描数组
 *          size - 寻找的总长度
 *          num1 - 寻找的值
 *          num2 - 寻找的值
 *          num3 - 寻找的值
 * @return  0    - 未同时找到
 *          1    - 同时找到准备改变连接模式
 */
BOOL find_mode_changekey(uint8_t arr[], uint8_t size, uint8_t num1, uint8_t num2, uint8_t num3) {
    int count = 0;

    for (int i = 0; i < size; i++) {
        if (arr[i] == num1) count++;
        if (arr[i] == num2) count++;
        if (arr[i] == num3) count++;
        // 如果已经找到了三个数，则立即返回true
        if (count == 3) {
            return 1;
        }
    }
    // 如果没有找到三个数，则返回false
    return 0;
}



