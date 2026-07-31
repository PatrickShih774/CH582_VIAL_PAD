/*
 * RF_MODE.c
 *
 *  Created on: 2024��11��6��
 *      Author: OWNER
 */
#include "CONFIG.h"
#include "RF_MODE.h"
#include "scan_key.h"
#include "ws2812.h"
#include "VIAL.h"
/*********************************************************************
 * GLOBAL TYPEDEFS
 */
uint8_t RF_taskID;
uint8_t TX_DATA[10] = {0x98,0x01,0, 0, 0x00, 0, 0, 0, 0, 0};
uint8_t TX_DATA1[3] = {0x98,0x02,0x00};
uint8_t TX_DATA3[1] = {0x96};
uint8_t TX_DATA4[20] = {0x98,0x03,0, 0, 0x00, 0, 0, 0, 0, 0};
uint16_t dongle_boot_flag = 0;
/*********************************************************************
 * @fn      RF_2G4StatusCallBack
 *
 * @brief   RF ״̬�ص���ע�⣺�����ڴ˺�����ֱ�ӵ���RF���ջ��߷���API����Ҫʹ���¼��ķ�ʽ����
 *
 * @param   sta     - ״̬����
 * @param   crc     - crcУ����
 * @param   rxBuf   - ����bufָ��
 *
 * @return  none
 */
void RF_2G4StatusCallBack(uint8_t sta, uint8_t crc, uint8_t *rxBuf)
{
    switch(sta)
    {
        case TX_MODE_TX_FINISH:
        {
            break;
        }
        case TX_MODE_TX_FAIL:
        {
            break;
        }
        case TX_MODE_RX_DATA:
        {
            if (crc == 0) {
                uint8_t i;

                if (rxBuf[2]==0xC0&&rxBuf[3]==0xC5) {
                }
                else {
                }

            } else {
                if (crc & (1<<0)) {
                }

                if (crc & (1<<1)) {
                }
            }
            break;
        }
        case TX_MODE_RX_TIMEOUT: // Timeout is about 200us
        {

            break;
        }
    }
}

/*********************************************************************
 * @fn      RF_ProcessEvent
 *
 * @brief   RF �¼�����
 *
 * @param   task_id - ����ID
 * @param   events  - �¼���־
 *
 * @return  δ����¼�
 */
uint16_t RF_ProcessEvent(uint8_t task_id, uint16_t events)
{
    if(events & SBP_RF_kbd_data_EVT)
    {
        RF_Shut();
        tmos_memset(scan_buf, 0, 6);
        scan_flag = get_key_fanz(scan_buf);
        if (tmos_memcmp(last_buf,scan_buf,6) == TRUE) {
            tmos_memcpy(&TX_DATA[4], scan_buf, 6);
            RF_Tx(TX_DATA, 10, 0xFF, 0xFF);
            if (scan_flag == 0) {
                change_mode_USB = 0;
                change_mode_BLE = 0;
                dongle_boot_flag = 0;
            }
            else if (scan_flag == 1) {
                if (scan_buf[0]==key_data_buf[2][0]) {
                    //USB MODE
                    change_mode_USB++;
                }
                else if (scan_buf[0]==key_data_buf[2][1]) {
                    //ble MODE
                    change_mode_BLE++;
                }
                else if (scan_buf[0]==key_data_buf[2][2]) {
                    /* 2.4G dongle boot — same key as 2.4G mode toggle */
                    dongle_boot_flag++;
                }
                else {

                }
            }
        }
        else {
            change_mode_USB  = 0;
            change_mode_BLE  = 0;
            dongle_boot_flag = 0;
            tmos_memcpy(&TX_DATA[4], scan_buf, 6);
            RF_Tx(TX_DATA, 10, 0xFF, 0xFF);
        }
        tmos_memcpy(last_buf,scan_buf,6);
        if (change_mode_USB == 313) {
            uint8_t key[1] = {0x0B};
            FLASH_DATA_VIAL_WITE_mode(key);
            DelayMs(1);
            SYS_ResetExecute();
        }
        if (change_mode_BLE == 313) {
            uint8_t key[1] = {0xBE};
            FLASH_DATA_VIAL_WITE_mode(key);
            DelayMs(1);
            SYS_ResetExecute();
        }
        if (dongle_boot_flag == 313) {
            tmos_start_task(RF_taskID, SBP_dongle_boot_EVT, 0);
        }
        tmos_start_task(RF_taskID, SBP_RF_kbd_data_EVT, MS1_TO_SYSTEM_TIME(8));
        return events ^ SBP_RF_kbd_data_EVT;
    }
    if(events & SBP_dongle_boot_EVT)
    {
       RF_Shut();
       RF_Tx(TX_DATA3, 1, 0xFF, 0xFF);
       return (events ^ SBP_dongle_boot_EVT);
    }
    if(events & WS2812_EVENT)
    {
       /* WS2812 disabled: no free pins on WeAct CH582F QFN28.
        * Keep event handler stub for future porting. */
       return (events ^ WS2812_EVENT);
    }

    return 0;
}

/*********************************************************************
 * @fn      RF_Init
 *
 * @brief   RF ��ʼ��
 *
 * @return  none
 */
void RF_Init(void)
{
    uint8_t    state;
    rfConfig_t rfConfig;

    tmos_memset(&rfConfig, 0, sizeof(rfConfig_t));
    RF_taskID = TMOS_ProcessEventRegister(RF_ProcessEvent);
    rfConfig.accessAddress = 0x71764129; // ��ֹʹ��0x55555555�Լ�0xAAAAAAAA ( ���鲻����24��λ��ת���Ҳ�����������6��0��1 )
    rfConfig.CRCInit = 0x555555;
    rfConfig.Channel = 8;
    rfConfig.Frequency = 2405000;
    rfConfig.LLEMode = LLE_MODE_AUTO | LLE_MODE_EX_CHANNEL; // ʹ�� LLE_MODE_EX_CHANNEL ��ʾ ѡ�� rfConfig.Frequency ��Ϊͨ��Ƶ��
    rfConfig.rfStatusCB = RF_2G4StatusCallBack;
    rfConfig.RxMaxlen = 251;
    state = RF_Config(&rfConfig);
    PRINT("rf 2.4g init: %x\n", state);
//    { // RX mode
//        state = RF_Rx(TX_DATA, 10, 0xFF, 0xFF);
//        PRINT("RX mode.state = %x\n", state);
//    }

   // { // TX mode
    tmos_start_task(RF_taskID, SBP_RF_kbd_data_EVT, MS1_TO_SYSTEM_TIME(8));
   // tmos_start_task(RF_taskID, WS2812_EVENT, MS1_TO_SYSTEM_TIME(80));
    //tmos_start_reload_task( RF_taskID , SBP_RF_kbd_data_EVT,MS1_TO_SYSTEM_TIME(2) );
    //}
}

/******************************** endfile @ main ******************************/
