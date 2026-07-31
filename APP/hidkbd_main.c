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
    // vial_init() 在空 flash 下校验失败会死循环（校验 0x3E00/0x7F018），故彻底不调。
    // 手动设 vial_key_done=1，所有 vial 库函数可用；模式直接从 EEPROM 读。
    uint8_t key_mode = 0;
    extern uint8_t vial_key_done;
    vial_key_done = 1;
    {
        uint8_t buf[2];  // uint8_t array, NOT 4-byte aligned → slow path?

        /* Theory: aligned buffers (uint32_t) trigger a DMA/fast-path bug
         * in the flash library. Unaligned uint8_t uses slow byte path → safe.
         * Test: ONE call reading 2 bytes into unaligned uint8_t[2].
         * If works → we can read ALL keymap in a single call with uint8_t buf. */
        EEPROM_READ(0x3F00, buf, 2);

        if (buf[0] != 0x0B && buf[0] != 0xBE && buf[0] != 0x24) {
            buf[0] = 0x0B;
            FLASH_DATA_VIAL_WITE_mode(&buf[0]);
        }
        key_mode = buf[0];
    }
    if (key_mode != 0x0B && key_mode != 0xBE && key_mode != 0x24) {
        key_mode = 0x0B;
    }
    Scan_init();
    if (key_mode==0x0B)
    {
        USB_INIT();
    }
    else if (key_mode==0xBE) {  //BLE MODE
        PWR_DCDCCfg(ENABLE);
        CH58X_BLEInit();
        HAL_Init();
        GAPRole_PeripheralInit();
        HidDev_Init();
        HidEmu_Init();
    }
    else if (key_mode==0x24) {  //2.4G mode
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
