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
        uint8_t mode;
        EEPROM_READ(0x3F00, &mode, 1);            // 直接硬件读，空 flash 返回 0xFF
        if (mode != 0x0B && mode != 0xBE && mode != 0x24) {
            mode = 0x0B;                           // 非法 → 默认 USB
            FLASH_DATA_VIAL_WITE_mode(&mode);
        }
        key_mode = mode;
    }
    if (key_mode != 0x0B && key_mode != 0xBE && key_mode != 0x24) {
        key_mode = 0x0B;                           // 兜底
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
