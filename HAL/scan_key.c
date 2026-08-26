/********************************** (C) COPYRIGHT *******************************
 * File Name          : scan_key.c
 * Author             : WCH.yz
 * Version            : V1.0
 * Date               : 2024/11/19
 * Description        : ï¿½ï¿½ï¿½ï¿½É¨ï¿½ï¿½ï¿½ï¿½
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "scan_key.h"
#include "hiddev.h"
#include "VIAL.h"
#include "CH58x_pwr.h"   /* PWR_PeriphWakeUpCfg: GPIO »½ÐÑÔ´ */

// ASCIIï¿½ï¿½HIDï¿½ï¿½ï¿½ï¿½ï¿½Ó³ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ö£ï¿½
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
    {'\n', HID_KEYBOARD_RETURN}, // ï¿½ï¿½ï¿½è»»ï¿½Ð·ï¿½ï¿½ï¿½Ê¾ï¿½Ø³ï¿½ï¿½ï¿½
    {' ', HID_KEYBOARD_SPACEBAR},
    // ï¿½ï¿½ï¿½ï¿½Ó³ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
    {'\0', 0} // ï¿½ï¿½Ö¹ï¿½ï¿½
};

/*********************************************************************
 * @fn      asciiToHid
 *
 * @brief   ï¿½ï¿½ï¿½ï¿½ASCIIï¿½Ö·ï¿½ï¿½ï¿½Ó¦ï¿½ï¿½HIDï¿½ï¿½ï¿½ï¿½,ï¿½ï¿½Òªï¿½ï¿½ï¿½Ó¸ï¿½ï¿½ï¿½ï¿½Ö·ï¿½Ö»ï¿½ï¿½ï¿½Â±ï¿½ï¿½ï¿½ï¿½ï¿½
 *
 * @param   ascii - ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ö·ï¿?
 *
 * @return      0 - ï¿½ï¿½Æ¥ï¿½ï¿½ï¿½ï¿½
 *             !0 - Æ¥ï¿½ï¿½ï¿½ï¿½ï¿½HIDï¿½ï¿½ï¿?
 */
unsigned char asciiToHid(char ascii) {
    for (int i = 0; asciiToHidMap[i].ascii != '\0'; i++) {
        if (asciiToHidMap[i].ascii == ascii) {
            return asciiToHidMap[i].hid_code;
        }
    }
    // ï¿½ï¿½ï¿½Î´ï¿½Òµï¿½Æ¥ï¿½ï¿½ï¿½î£¬ï¿½ï¿½ï¿½ï¿?ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ô¸ï¿½ï¿½ï¿½ï¿½ï¿½Òªï¿½ï¿½ï¿½Ø´ï¿½ï¿½ï¿½ï¿½ë£?
    return 0;
}
uint32_t io_map_col[] = {col_0,col_1,col_2,col_3};
uint32_t io_map_row[] = {row_0,row_1,row_2,row_3,row_4,row_5};

uint8_t scan_flag = 0;
uint8_t scan_buf[6] = {0};
uint8_t last_buf[6] = {0};
uint8_t scan_modifier = 0;
volatile uint32_t g_last_act_ms = 0;   /* low-power idle: last key activity (g_bm_tick_ms) */
uint16_t change_mode_BLE = 0;
uint16_t change_mode_24 = 0;
uint16_t change_mode_USB = 0;

uint16_t key_data_buf[6][4]={   //[Y][X] â€?layer 0, from demo.vil (16-bit QMK keycodes)
        QK_LSFT|HID_KEYBOARD_9,  QK_LSFT|HID_KEYBOARD_0,  HID_KEYBOARD_EQUAL,     HID_KEYBOARD_TAB,       /* R0: LSFT(KC_9), LSFT(KC_0), KC_EQUAL, KC_TAB */
        HID_KEYBPAD_NUM_LOCK,    HID_KEYBPAD_DIVIDE,       HID_KEYBOARD_MULTIPLY,  HID_KEYBOARD_DELETE,     /* R1: KC_NUMLOCK, KC_KP_SLASH, KC_KP_ASTERISK, KC_BSPACE */
        HID_KEYBPAD_7,           HID_KEYBPAD_8,            HID_KEYBPAD_9,          HID_KEYBOARD_SUBTRACT,   /* R2: KC_KP_7, KC_KP_8, KC_KP_9, KC_KP_MINUS */
        HID_KEYBPAD_4,           HID_KEYBPAD_5,            HID_KEYBPAD_6,          HID_KEYBPAD_ADD,         /* R3: KC_KP_4, KC_KP_5, KC_KP_6, KC_KP_PLUS */
        HID_KEYBPAD_1,           HID_KEYBPAD_2,            HID_KEYBPAD_3,          HID_KEYBPAD_ENTER,       /* R4: KC_KP_1, KC_KP_2, KC_KP_3, KC_KP_ENTER */
        HID_KEYBPAD_0,           0x0000,                   HID_KEYBPAD_DOT,        0x0000,                  /* R5: KC_KP_0, KC_NO, KC_KP_DOT, KC_NO */
};
uint16_t key_data_buf_1[6][4]={   //[Y][X] â€?layer 1 (Fn), all KC_NO
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
};
uint16_t key_data_buf_2[6][4]={   //[Y][X] â€?layer 2, all KC_NO
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
};
uint16_t key_data_buf_3[6][4]={   //[Y][X] â€?layer 3, all KC_NO
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,
};
/*********************************************************************
 * @fn      Scan_init
 *
 * @brief   ï¿½ï¿½ï¿½ï¿½É¨ï¿½ï¿½ï¿½ï¿½Ê¼ï¿½ï¿½
 *
 * @param   none
 *
 * @return  none
 */
void Scan_init(void)
{
    /* GPIO-only â€?keymap flash merge is done in main(), BEFORE
     * FLASH_DATA_VIAL_WITE_mode, to keep flash controller state clean
     * for USB_DeviceInit. See README Â§7.3 for root-cause analysis. */
    GPIOA_ModeCfg(row_all, GPIO_ModeIN_PU);    /* rows = inputs (pull-up), scanned by get_key() */
    GPIOB_ModeCfg(col_all, GPIO_ModeOut_PP_5mA); /* cols = outputs, driven LOW one at a time */
}

/* ---- µÍ¹¦ºÄ GPIO »½ÐÑ£¨B0.8.9£©£ºcol0 À­µÍ¼ì²â¸ÃÁÐ°´¼ü£¬ÐÐµÍµçÆ½»½ÐÑ MCU ---- */
void Matrix_SleepWakeCfg(void)
{
    GPIOB_SetBits(col_1 | col_2 | col_3);      /* ÆäÓàÁÐ¸ß£¨¶þ¼«¹Ü×è¶Ï£¬²»Îó´¥·¢£© */
    GPIOB_ResetBits(col_0);                     /* col0 µÍ£º¸ÃÁÐ°´¼ü°´ÏÂ ¡ú ÐÐ±»À­µÍ */
    GPIOA_ModeCfg(row_all, GPIO_ModeIN_PU);     /* ÐÐÉÏÀ­ÊäÈë */
    GPIOA_ClearITFlagBit(row_all);
    GPIOA_ITModeCfg(row_all, GPIO_ITMode_LowLevel);  /* ÐÐµÍµçÆ½×÷»½ÐÑ´¥·¢ */
    PWR_PeriphWakeUpCfg(ENABLE, RB_SLP_GPIO_WAKE, Short_Delay);
}
void Matrix_WakeClear(void)
{
    GPIOA_ClearITFlagBit(row_all);              /* ÇåÐÐÖÐ¶Ï/»½ÐÑ±êÖ¾ */
    PWR_PeriphWakeUpCfg(DISABLE, RB_SLP_GPIO_WAKE, Short_Delay);  /* ¹Ø GPIO »½ÐÑ£¬½»»ØÉ¨Ãè */
}
/*********************************************************************
 * @fn      get_key
 *
 * @brief   ï¿½ï¿½È¡ï¿½ï¿½Öµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Îªï¿½ï¿½ï¿½ï¿½É¨ï¿½ï¿½
 *
 * @param   buf  -  ï¿½ï¿½Å¼ï¿½Öµï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
 *
 * @return  none
 */
__HIGH_CODE
uint8_t get_key(uint8_t *buf)
{
     uint8_t i = 0;
     scan_modifier = 0;
      for (int var = 0; var < 4; ++var) {
          GPIOB_ResetBits(io_map_col[var]);   /* drive one column LOW */
          __nop();__nop();
          for (int var2 = 0; var2 < 6; ++var2) {
              if (GPIOA_ReadPortPin(io_map_row[var2]) == 0) {  /* read row */
                    if (i>=6) {
                       break;
                    }
                    uint16_t kc = key_data_buf[var2][var];
                    scan_modifier |= qmk_mods(kc);
                    buf[i] = qmk_usage(kc);
                    i++;
                }
          }
          if (i>=6) {
             break;
          }
          GPIOB_SetBits(io_map_col[var]);  /* restore column HIGH */
          mDelayuS(2);                      /* let rows recover through 40k PU (RCâ‰?Âµs, 2Ï„ margin) */
      }
      GPIOB_SetBits(col_all);  /* all columns HIGH */
        if (i) g_last_act_ms = g_bm_tick_ms;
      return i;
}
/*********************************************************************
 * @fn      get_key_fanz
 *
 * @brief   ï¿½ï¿½È¡ï¿½ï¿½Öµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Îªï¿½ï¿½ï¿½ï¿½É¨ï¿½ï¿½
 *
 * @param   buf  -  ï¿½ï¿½Å¼ï¿½Öµï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
 *
 * @return  none
 */
__HIGH_CODE
uint8_t get_key_fanz(uint8_t *buf)
{
     uint8_t i = 0;
     scan_modifier = 0;
      for (int var = 0; var < 6; ++var) {
          GPIOA_ResetBits(io_map_row[var]);
          __nop();__nop();
          for (int var2 = 0; var2 < 4; ++var2) {
              if (GPIOB_ReadPortPin(io_map_col[var2]) == 0) {
                    if (i>=6) {
                       break;
                    }
                    uint16_t kc = key_data_buf[var][var2];
                    scan_modifier |= qmk_mods(kc);
                    buf[i] = qmk_usage(kc);
                    i++;
                }
          }
          if (i>=6) {
             break;
          }
          GPIOA_SetBits(io_map_row[var]);  //ROWï¿½ï¿½ï¿½ï¿½
      }
      GPIOA_SetBits(row_all);  //ROWÈ«ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
        if (i) g_last_act_ms = g_bm_tick_ms;
      return i;
}

/*********************************************************************
 * @fn      find_mode_changekey
 *
 * @brief   Ñ°ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ä£Ê½ï¿½Ä±ï¿½Ä±ï¿½Ö?
 *
 * @param   arr  - ï¿½ï¿½ï¿½ï¿½É¨ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
 *          size - Ñ°ï¿½Òµï¿½ï¿½Ü³ï¿½ï¿½ï¿½
 *          num1 - Ñ°ï¿½Òµï¿½Öµ
 *          num2 - Ñ°ï¿½Òµï¿½Öµ
 *          num3 - Ñ°ï¿½Òµï¿½Öµ
 * @return  0    - Î´Í¬Ê±ï¿½Òµï¿½
 *          1    - Í¬Ê±ï¿½Òµï¿½×¼ï¿½ï¿½ï¿½Ä±ï¿½ï¿½ï¿½ï¿½ï¿½Ä£Ê½
 */
BOOL find_mode_changekey(uint8_t arr[], uint8_t size, uint16_t num1, uint16_t num2, uint16_t num3) {
    int count = 0;

    for (int i = 0; i < size; i++) {
        if (arr[i] == num1) count++;
        if (arr[i] == num2) count++;
        if (arr[i] == num3) count++;
        // ï¿½ï¿½ï¿½ï¿½Ñ¾ï¿½ï¿½Òµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½true
        if (count == 3) {
            return 1;
        }
    }
    // ï¿½ï¿½ï¿½Ã»ï¿½ï¿½ï¿½Òµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ò·µ»ï¿½false
    return 0;
}



