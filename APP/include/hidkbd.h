/********************************** (C) COPYRIGHT *******************************
 * File Name          : hidkbd.h
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2018/12/10
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#ifndef HIDKBD_H
#define HIDKBD_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */

// Task Events
#define START_DEVICE_EVT          0x0001
#define START_REPORT_EVT          0x0002
#define START_PARAM_UPDATE_EVT    0x0004
#define START_PHY_UPDATE_EVT      0x0008
/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*
 * Task Initialization for the BLE Application
 */
extern void HidEmu_Init(void);

/*
 * Task Event Processor for the BLE Application
 */
extern uint16_t HidEmu_ProcessEvent(uint8_t task_id, uint16_t events);

/*
 * Enable/disable BLE advertising (0=stop advertising for deep-sleep, 1=resume)
 */
extern void HidEmu_AdvEnable(uint8_t en);

/*
 * Fully stop BLE (BM_LP_BLE_OFF current test): stop advertising, kill scan/
 * report/param/phy tasks, terminate any active link.  Caller must also stop
 * scheduling TMOS_SystemProcess() for a true deep-sleep floor.
 */
void HidEmu_Shutdown(void);

/* V4 state machine: 1=active(short interval) 0=idle(long interval) */
extern void HidEmu_SetLpMode(uint8_t active);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif
