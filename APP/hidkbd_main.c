/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2024/12/10
 * Description        : ��������Ӧ��������������ϵͳ��ʼ��
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* ͷ�ļ����� */
#include "CONFIG.h"
#include "HAL.h"
#include "hiddev.h"
#include "hidkbd.h"
#include "USB_MODE.h"
#include "RF_MODE.h"
#include "scan_key.h"
#include "VIAL.h"
/* ws2812.h is intentionally not included: no free pins on WeAct CH582F QFN28.
 * Keep HAL/ws2812b.c in tree for future porting to a larger package. */
/*********************************************************************
 * GLOBAL TYPEDEFS
 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

/*********************************************************************
 * @fn      load_keymap_from_flash
 *
 * @brief   Load all 4 keymap layers from EEPROM (magic-byte guarded).
 *          MUST be called AFTER USB_DeviceInit() in USB mode.
 *          Uses non-0xFF merge: only overwrites positions explicitly
 *          saved, preserving compile-time defaults for others.
 *
 * @return  none
 */
static void load_keymap_from_flash(void)
{
    uint8_t i;
    __attribute__((aligned(4))) uint8_t data_buf[48];  /* 24 uint16_t × 2 */
    uint16_t *kc = (uint16_t *)data_buf;                /* overlay for 16-bit access */
    uint8_t magic;

    EEPROM_READ(0x3F01, &magic, 1);
    if (magic != 0xA5) {
        return;  /* no valid keymap in flash, keep compile-time defaults */
    }

    /* ── Layer 0 (base): 0x3000-0x302F (48B) ── */
    EEPROM_READ(0x3000, data_buf, 48);
    for (i = 0; i < 24; i++) {
        if (kc[i] != 0xFFFF) {
            (&key_data_buf[0][0])[i] = kc[i];
        }
    }

    /* ── Layer 1 (Fn): 0x3030-0x305F ── */
    EEPROM_READ(0x3030, data_buf, 48);
    for (i = 0; i < 24; i++) {
        if (kc[i] != 0xFFFF) {
            (&key_data_buf_1[0][0])[i] = kc[i];
        }
    }

    /* ── Layer 2: 0x3060-0x308F ── */
    EEPROM_READ(0x3060, data_buf, 48);
    for (i = 0; i < 24; i++) {
        if (kc[i] != 0xFFFF) {
            (&key_data_buf_2[0][0])[i] = kc[i];
        }
    }

    /* ── Layer 3: 0x3090-0x30BF ── */
    EEPROM_READ(0x3090, data_buf, 48);
    for (i = 0; i < 24; i++) {
        if (kc[i] != 0xFFFF) {
            (&key_data_buf_3[0][0])[i] = kc[i];
        }
    }
}

/*********************************************************************
 * @fn      Main_Circulation
 *
 * @brief   ��ѭ��
 *
 * @return  none
 */
__HIGH_CODE
__attribute__((noinline))
void Main_Circulation()
{
    while(1)
    {
        TMOS_SystemProcess();
    }
}

/*********************************************************************
 * @fn      main
 *
 * @brief   ������
 *
 * @return  none
 */
int main(void)
{
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    /* ============================================================
     * MINIMAL USB TEST - mirrors official openwch/ch583
     * HID_CompliantDev example exactly:
     *   no EEPROM, no GPIO, no TMR3, no keymap loading.
     * Goal: verify the USB1 stack alone (enum + SET_REPORT) works
     * on this hardware/OS. If it works, the fault is in the app
     * additions (Scan_init / load_keymap / TMR3). If it still
     * fails, the fault is in USB_MODE.c descriptors/ISR or HW.
     * ============================================================ */
    extern uint8_t vial_key_done;
    vial_key_done = 1;

    extern uint8_t EP0_Databuf[], EP1_Databuf[], EP2_Databuf[], EP3_Databuf[];
    extern void Main_Circulation_USB(void);

    pEP0_RAM_Addr = EP0_Databuf;
    pEP1_RAM_Addr = EP1_Databuf;
    pEP2_RAM_Addr = EP2_Databuf;
    pEP3_RAM_Addr = EP3_Databuf;

    USB_DeviceInit();
    PFIC_EnableIRQ(USB_IRQn);
    load_keymap_from_flash();  /* restore saved keymap from EEPROM */
    Main_Circulation_USB();    /* empty while(1) - never returns */
}

/******************************** endfile @ main ******************************/
