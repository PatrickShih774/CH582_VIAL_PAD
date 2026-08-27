/********************************** (C) COPYRIGHT *******************************
 * File Name          : RTC.c
 * Author             : WCH
 * Version            : V1.2
 * Date               : 2022/01/18
 * Description        : RTC配置及其初始化
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "HAL.h"

/*********************************************************************
 * CONSTANTS
 */
#define RTC_INIT_TIME_HOUR      0
#define RTC_INIT_TIME_MINUTE    0
#define RTC_INIT_TIME_SECEND    0

/***************************************************
 * Global variables
 */
volatile uint32_t RTCTigFlag;

/*******************************************************************************
 * @fn      RTC_SetTignTime
 *
 * @brief   配置RTC触发时间
 *
 * @param   time    - 触发时间.
 *
 * @return  None.
 */
void RTC_SetTignTime(uint32_t time)
{
    sys_safe_access_enable();
    R32_RTC_TRIG = time;
    sys_safe_access_disable();
    RTCTigFlag = 0;
}

/*******************************************************************************
 * @fn      RTC_IRQHandler
 *
 * @brief   RTC中断处理
 *
 * @param   None.
 *
 * @return  None.
 */
__INTERRUPT
__HIGH_CODE
void RTC_IRQHandler(void)
{
    R8_RTC_FLAG_CTRL = (RB_RTC_TMR_CLR | RB_RTC_TRIG_CLR);
    RTCTigFlag = 1;
}

/*******************************************************************************
 * @fn      HAL_Time0Init
 *
 * @brief   系统定时器初始化
 *
 * @param   None.
 *
 * @return  None.
 */
void HAL_TimeInit(void)
{
#if(CLK_OSC32K)
    sys_safe_access_enable();
    R8_CK32K_CONFIG &= ~(RB_CLK_OSC32K_XT | RB_CLK_XT32K_PON);
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_CK32K_CONFIG |= RB_CLK_INT32K_PON;
    sys_safe_access_disable();
    LSECFG_Current(LSE_RCur_100);
    Lib_Calibration_LSI();
#else
    sys_safe_access_enable();
    R8_CK32K_CONFIG |= RB_CLK_OSC32K_XT | RB_CLK_INT32K_PON | RB_CLK_XT32K_PON;
    LSECFG_Current(LSE_RCur_100);  /* standard LSE drive (WCH default) */
    /* Note: LSE_RCur_70 is too weak to reliably drive the 32.768k crystal
     * / panel; keep 100 for stable source and clock accuracy. */
    sys_safe_access_disable();
    /* Wait for the external 32.768k crystal to start oscillating (RB_32K_CLK_PIN
     * goes high once stable) before TMOS_TimerInit consumes the 32K clock.  Without
     * this, TMOS init can hang on a slow/not-yet-oscillating crystal (white screen,
     * no UI).  Bounded wait: fall back to a short fixed delay if the pin never sets. */
    {
        volatile uint8_t clk_pin;
        uint32_t wcnt = 0;
        do {
            clk_pin = (R8_CK32K_CONFIG & RB_32K_CLK_PIN);
            if (++wcnt > 2000000u) break;   /* ~safety timeout, avoid hard hang */
        } while (clk_pin == 0);
    }
#endif
    RTC_InitTime(2020, 1, 1, 0, 0, 0); //RTC时钟初始化当前时间
    TMOS_TimerInit(0);
}

/******************************** endfile @ time ******************************/
