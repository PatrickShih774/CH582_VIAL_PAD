/*
 * RF_MODE.h
 *
 *  Created on: 2024年12月13日
 *      Author: OWNER
 */

#ifndef INCLUDE_RF_MODE_H_
#define INCLUDE_RF_MODE_H_

#define SBP_RF_START_DEVICE_EVT    1
#define SBP_RF_kbd_data_EVT        2
#define SBP_RF_vol_data_EVT        4
#define SBP_dongle_boot_EVT        8
#define LLE_MODE_ORIGINAL_RX       (0x80) //如果配置LLEMODE时加上此宏，则接收第一字节为原始数据（原来为RSSI）

extern void RF_Init(void);

#endif /* INCLUDE_RF_MODE_H_ */
