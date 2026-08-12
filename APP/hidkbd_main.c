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
#include "NV3007.h"
#include "ui.h"
#include "bm_ui.h"         /* bare-metal UI (UI_BM_EN=1) */
#include <string.h>   /* memset for .ble_heap (NOLOAD) */
/* ws2812.h is intentionally not included: no free pins on WeAct CH582F QFN28.
 * Keep HAL/ws2812b.c in tree for future porting to a larger package. */
/*********************************************************************
 * GLOBAL TYPEDEFS
 */
__attribute__((section(".ble_heap"), aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];   /* shared-RAM overlay (B0.5): .ble_heap NOLOAD, zeroed before BLE init */
/* Boot mode byte (EEPROM 0x3F00): 0x0B=USB, 0xBE=BLE, 0x24=2.4G.
 * Read in main() after USB_DeviceInit (��7.3); TMR3 ISR uses it to route HID. */
uint8_t g_boot_mode = 0x0B;

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

    extern uint8_t vial_key_done;
    vial_key_done = 1;

    extern uint8_t EP0_Databuf[], EP1_Databuf[], EP2_Databuf[], EP3_Databuf[];
    extern void Main_Circulation_USB(void);

    pEP0_RAM_Addr = EP0_Databuf;
    pEP1_RAM_Addr = EP1_Databuf;
    pEP2_RAM_Addr = EP2_Databuf;
    pEP3_RAM_Addr = EP3_Databuf;

    Scan_init();                    /* config row/col GPIOs */
    USB_DeviceInit();
    PFIC_EnableIRQ(USB_IRQn);
    TMR3_TimerInit(90000);          /* 1.5ms scan timer */
    TMR3_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_EnableIRQ(TMR3_IRQn);
    load_keymap_from_flash();       /* restore saved keymap from EEPROM */

    /* ── NV3007 display init (both renderers) ──────────────────────── */
    ST7789_Init();

    /* --- Read boot mode (EEPROM 0x3F00, after USB per S7.3) --- */
    {
        uint8_t mode = 0x0B;
        EEPROM_READ(0x3F00, &mode, 1);
        if (mode != 0x0B && mode != 0xBE && mode != 0x24) mode = 0x0B;
        g_boot_mode = mode;
    }

    /* --- Boot escape (B0.4): hold USB switch key (7) during power-on to force USB. ---
     * The mode byte lives in data flash and survives ISP reflash, so a stuck 0xBE/0x24
     * would otherwise keep booting BLE/RF forever. get_key matches Scan_init (cols-out). */
    if (g_boot_mode != 0x0B) {
        uint8_t bootbuf[6] = {0};
        if (get_key(bootbuf) == 1 && bootbuf[0] == (uint8_t)(key_data_buf[2][0] & 0xFF)) {
            uint8_t key[1] = {0x0B};
            FLASH_DATA_VIAL_WITE_mode(key);
            g_boot_mode = 0x0B;
        }
    }

    if (g_boot_mode == 0xBE) {
        /* --- BLE mode: bare-metal UI (B0.8) --- */
        extern void HidEmu_Init(void);
        memset(MEM_BUF, 0, sizeof(MEM_BUF));   /* .ble_heap is NOLOAD (not zeroed at boot) */
        CH58X_BLEInit();
        HAL_Init();
        GAPRole_PeripheralInit();
        HidDev_Init();
        HidEmu_Init();
        ui_bm_init();                   /* bare-metal UI */
        while(1) {
            TMOS_SystemProcess();      /* BLE stack (1.25ms) */
            ui_bm_process();
        }
    } else if (g_boot_mode == 0x24) {
        /* --- 2.4G RF mode (planned, not in B0.2) --- */
        SYS_ResetExecute();            /* fall back to USB for now */
    } else {
        /* --- USB mode (default): bare-metal 3-page UI (B0.8) --- */
        ui_bm_init();
        while(1) {
            ui_bm_process();
        }
    }
}
/******************************** endfile @ main ******************************/
