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
    __attribute__((aligned(4))) uint8_t data_buf[24];
    uint8_t magic;

    EEPROM_READ(0x3F01, &magic, 1);
    if (magic != 0xA5) {
        return;  /* no valid keymap in flash, keep compile-time defaults */
    }

    /* ── Layer 0 (base): 0x3000-0x3017 ── */
    FLASH_DATA_KEY(data_buf);                        /* reads 20B from 0x3000 */
    EEPROM_READ(0x3014, &data_buf[20], 4);           /* row 5 at 0x3014 */
    for (i = 0; i < 24; i++) {
        if (data_buf[i] != 0xFF) {
            (&key_data_buf[0][0])[i] = data_buf[i];
        }
    }

    /* ── Layer 1 (Fn): 0x3018-0x302F ── */
    EEPROM_READ(0x3018, data_buf, 24);
    for (i = 0; i < 24; i++) {
        if (data_buf[i] != 0xFF) {
            (&key_data_buf_1[0][0])[i] = data_buf[i];
        }
    }

    /* ── Layer 2: 0x3030-0x3047 ── */
    EEPROM_READ(0x3030, data_buf, 24);
    for (i = 0; i < 24; i++) {
        if (data_buf[i] != 0xFF) {
            (&key_data_buf_2[0][0])[i] = data_buf[i];
        }
    }

    /* ── Layer 3: 0x3048-0x305F ── */
    EEPROM_READ(0x3048, data_buf, 24);
    for (i = 0; i < 24; i++) {
        if (data_buf[i] != 0xFF) {
            (&key_data_buf_3[0][0])[i] = data_buf[i];
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

#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif

/* PA8/PA9 now used for ST7789_SCK/MOSI — debug UART moved off.
 * Uncomment and adjust pins if serial debug is needed on another GPIO pair.
    #ifdef DEBUG
        GPIOA_SetBits(bTXD1);
        GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
        UART1_DefInit();
    #endif
 */
    /* ── Phase 1: Read mode byte (ONLY EEPROM op before USB_DeviceInit) ──
     * vial_init() is skipped: it dead-loops on empty flash (validates
     * 0x3E00/0x7F018). vial_key_done=1 enables all vial lib functions.
     *
     * CRITICAL (see README §7.3): do NOT add EEPROM reads beyond this
     * single 1-byte uint8_t read before USB_DeviceInit(). See the 10+
     * test matrix confirming any extra EEPROM access breaks USB enum.  */
    uint8_t key_mode = 0;
    extern uint8_t vial_key_done;
    vial_key_done = 1;
    {
        uint8_t mode;
        EEPROM_READ(0x3F00, &mode, 1);
        if (mode != 0x0B && mode != 0xBE && mode != 0x24) {
            mode = 0x0B;                           /* empty flash → default USB */
            FLASH_DATA_VIAL_WITE_mode(&mode);
        }
        key_mode = mode;
    }
    if (key_mode != 0x0B && key_mode != 0xBE && key_mode != 0x24) {
        key_mode = 0x0B;                           /* safety net */
    }

    /* GPIO-only: no flash/EEPROM ops — see README §7.3 */
    Scan_init();

    if (key_mode == 0x0B) {
        /* ── USB MODE ── */
        extern uint8_t EP0_Databuf[], EP1_Databuf[], EP2_Databuf[], EP3_Databuf[];
        extern void Main_Circulation_USB(void);

        pEP0_RAM_Addr = EP0_Databuf;
        pEP1_RAM_Addr = EP1_Databuf;
        pEP2_RAM_Addr = EP2_Databuf;
        pEP3_RAM_Addr = EP3_Databuf;
        USB_DeviceInit();

        /* ── Phase 2: Keymap load AFTER USB_DeviceInit ──
         * All EEPROM reads beyond the initial mode byte MUST happen
         * after USB_DeviceInit() but before PFIC_EnableIRQ(USB_IRQn).
         * This is the only window where multi-byte EEPROM accesses
         * don't break USB enumeration.                    */
        load_keymap_from_flash();

        PFIC_EnableIRQ(USB_IRQn);
        TMR3_TimerInit(90000);                         /* 1.5 ms timer */
        TMR3_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
        PFIC_EnableIRQ(TMR3_IRQn);
        Main_Circulation_USB();                        /* never returns */
    }
    else if (key_mode == 0xBE) {
        /* ── BLE MODE ── */
        load_keymap_from_flash();
        PWR_DCDCCfg(ENABLE);
        CH58X_BLEInit();
        HAL_Init();
        GAPRole_PeripheralInit();
        HidDev_Init();
        HidEmu_Init();
    }
    else if (key_mode == 0x24) {
        /* ── 2.4G RF MODE ── */
        load_keymap_from_flash();
        PWR_DCDCCfg(ENABLE);
        CH58X_BLEInit();
        HAL_Init();
        RF_RoleInit();
        RF_Init();
    }
    else {
        SYS_ResetExecute();
    }
    Main_Circulation();
}

/******************************** endfile @ main ******************************/
