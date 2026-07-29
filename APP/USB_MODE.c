/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2024/11/06
 * Description        : USB模式通信驱动层
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "ws2812.h"
#include "scan_key.h"
#include "USB_MODE.h"
#include "VIAL.h"
#define DevEP0SIZE    0x40
uint8_t USB_VIAL_START = 0;
uint8_t vial_data_count = 0;
// 支持的最大接口数量
#define USB_INTERFACE_MAX_NUM       2
// 接口号的最大值
#define USB_INTERFACE_MAX_INDEX      1

uint8_t key_chang_data[32] = {0};
//uint8_t scan_flag = 0;
uint8_t rgb_flag = 0;

// 设备描述符
const uint8_t MyDevDescr[] = {0x12,0x01,0x00,0x02,0x00,0x00,
                              0x00,0x40,0x73,0x92,0x57,0x91,
                              0x00,0x01,0x01,0x02,0x03,0x01};
// 配置描述符
const uint8_t MyCfgDescr[] = {
        0x09,        //   bLength
        0x02,        //   bDescriptorType (Configuration)
        0x5B, 0x00,  //   wTotalLength 91
        0x03,        //   bNumInterfaces 3
        0x01,        //   bConfigurationValue
        0x00,        //   iConfiguration (String Index)
        0xA0,        //   bmAttributes Remote Wakeup
        0xFA,        //   bMaxPower 500mA

        0x09,        //   bLength
        0x04,        //   bDescriptorType (Interface)
        0x00,        //   bInterfaceNumber 0
        0x00,        //   bAlternateSetting
        0x01,        //   bNumEndpoints 1
        0x03,        //   bInterfaceClass
        0x01,        //   bInterfaceSubClass
        0x01,        //   bInterfaceProtocol
        0x00,        //   iInterface (String Index)

        0x09,        //   bLength
        0x21,        //   bDescriptorType (HID)
        0x11, 0x01,  //   bcdHID 1.11
        0x00,        //   bCountryCode
        0x01,        //   bNumDescriptors
        0x22,        //   bDescriptorType[0] (HID)
        0x44, 0x00,  //   wDescriptorLength[0] 68

        0x07,        //   bLength
        0x05,        //   bDescriptorType (Endpoint)
        0x81,        //   bEndpointAddress (IN/D2H)
        0x03,        //   bmAttributes (Interrupt)
        0x08, 0x00,  //   wMaxPacketSize 8
        0x01,        //   bInterval 1 (unit depends on device speed)

        0x09,        //   bLength
        0x04,        //   bDescriptorType (Interface)
        0x01,        //   bInterfaceNumber 1
        0x00,        //   bAlternateSetting
        0x02,        //   bNumEndpoints 2
        0x03,        //   bInterfaceClass
        0x00,        //   bInterfaceSubClass
        0x00,        //   bInterfaceProtocol
        0x00,        //   iInterface (String Index)

        0x09,        //   bLength
        0x21,        //   bDescriptorType (HID)
        0x11, 0x01,  //   bcdHID 1.11
        0x00,        //   bCountryCode
        0x01,        //   bNumDescriptors
        0x22,        //   bDescriptorType[0] (HID)
        0x22, 0x00,  //   wDescriptorLength[0] 34

        0x07,        //   bLength
        0x05,        //   bDescriptorType (Endpoint)
        0x82,        //   bEndpointAddress (IN/D2H)
        0x03,        //   bmAttributes (Interrupt)
        0x20, 0x00,  //   wMaxPacketSize 32
        0x01,        //   bInterval 1 (unit depends on device speed)

        0x07,        //   bLength
        0x05,        //   bDescriptorType (Endpoint)
        0x03,        //   bEndpointAddress (OUT/H2D)
        0x03,        //   bmAttributes (Interrupt)
        0x20, 0x00,  //   wMaxPacketSize 32
        0x01,        //   bInterval 1 (unit depends on device speed)

        0x09,        //   bLength
        0x04,        //   bDescriptorType (Interface)
        0x02,        //   bInterfaceNumber 2
        0x00,        //   bAlternateSetting
        0x01,        //   bNumEndpoints 1
        0x03,        //   bInterfaceClass
        0x00,        //   bInterfaceSubClass
        0x00,        //   bInterfaceProtocol
        0x00,        //   iInterface (String Index)

        0x09,        //   bLength
        0x21,        //   bDescriptorType (HID)
        0x11, 0x01,  //   bcdHID 1.11
        0x00,        //   bCountryCode
        0x01,        //   bNumDescriptors
        0x22,        //   bDescriptorType[0] (HID)
        0x7B, 0x00,  //   wDescriptorLength[0] 123

        0x07,        //   bLength
        0x05,        //   bDescriptorType (Endpoint)
        0x84,        //   bEndpointAddress (IN/D2H)
        0x03,        //   bmAttributes (Interrupt)
        0x20, 0x00,  //   wMaxPacketSize 32
        0x01,        //   bInterval 1 (unit depends on device speed)

};
/* USB速度匹配描述符 */
const uint8_t My_QueDescr[] = {0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00};

/* USB全速模式,其他速度配置描述符 */
uint8_t USB_FS_OSC_DESC[sizeof(MyCfgDescr)] = {
    0x09, 0x07, /* 其他部分通过程序复制 */
};

// 语言描述符
const uint8_t MyLangDescr[] = {0x04, 0x03, 0x09, 0x04};
// 厂家信息
const uint8_t MyManuInfo[] = {0x1C,0x03,0x76,0x00,0x69,0x00,0x61,0x00,0x6C,0x00,
                              0x3A,0x00,0x66,0x00,0x36,0x00,0x34,0x00,0x63,0x00,
                              0x32,0x00,0x62,0x00,0x33,0x00,0x63,0x00};
// 产品信息
const uint8_t MyProdInfo[] = {0x0C, 0x03, 'E', 0, 'B', 0, 'P', 0, '1', 0, '7', 0};
/*HID类报表描述符*/
const uint8_t KeyRepDesc[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x06,        // Usage (Keyboard)
        0xA1, 0x01,        // Collection (Application)
        0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
        0x19, 0xE0,        //   Usage Minimum (0xE0)
        0x29, 0xE7,        //   Usage Maximum (0xE7)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x95, 0x08,        //   Report Count (8)
        0x75, 0x01,        //   Report Size (1)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x95, 0x01,        //   Report Count (1)
        0x75, 0x08,        //   Report Size (8)
        0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
        0x19, 0x00,        //   Usage Minimum (0x00)
        0x29, 0xFF,        //   Usage Maximum (0xFF)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x95, 0x06,        //   Report Count (6)
        0x75, 0x08,        //   Report Size (8)
        0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x08,        //   Usage Page (LEDs)
        0x19, 0x01,        //   Usage Minimum (Num Lock)
        0x29, 0x05,        //   Usage Maximum (Kana)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x95, 0x05,        //   Report Count (5)
        0x75, 0x01,        //   Report Size (1)
        0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0x95, 0x01,        //   Report Count (1)
        0x75, 0x03,        //   Report Size (3)
        0x91, 0x01,        //   Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0xC0,              // End Collection
};


const uint8_t vial_Desc[] = {
        0x06, 0x60, 0xFF,  // Usage Page (Vendor Defined 0xFF60)
        0x09, 0x61,        // Usage (0x61)
        0xA1, 0x01,        // Collection (Application)
        0x09, 0x62,        //   Usage (0x62)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x95, 0x20,        //   Report Count (32)
        0x75, 0x08,        //   Report Size (8)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x09, 0x63,        //   Usage (0x63)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x95, 0x20,        //   Report Count (32)
        0x75, 0x08,        //   Report Size (8)
        0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0xC0,              // End Collection
};
const uint8_t Consumer_Desc[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x02,        // Usage (Mouse)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x02,        //   Report ID (2)
        0x09, 0x01,        //   Usage (Pointer)
        0xA1, 0x00,        //   Collection (Physical)
        0x05, 0x09,        //     Usage Page (Button)
        0x19, 0x01,        //     Usage Minimum (0x01)
        0x29, 0x08,        //     Usage Maximum (0x08)
        0x15, 0x00,        //     Logical Minimum (0)
        0x25, 0x01,        //     Logical Maximum (1)
        0x95, 0x08,        //     Report Count (8)
        0x75, 0x01,        //     Report Size (1)
        0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
        0x09, 0x30,        //     Usage (X)
        0x09, 0x31,        //     Usage (Y)
        0x15, 0x81,        //     Logical Minimum (-127)
        0x25, 0x7F,        //     Logical Maximum (127)
        0x95, 0x02,        //     Report Count (2)
        0x75, 0x08,        //     Report Size (8)
        0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
        0x09, 0x38,        //     Usage (Wheel)
        0x15, 0x81,        //     Logical Minimum (-127)
        0x25, 0x7F,        //     Logical Maximum (127)
        0x95, 0x01,        //     Report Count (1)
        0x75, 0x08,        //     Report Size (8)
        0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x0C,        //     Usage Page (Consumer)
        0x0A, 0x38, 0x02,  //     Usage (AC Pan)
        0x15, 0x81,        //     Logical Minimum (-127)
        0x25, 0x7F,        //     Logical Maximum (127)
        0x95, 0x01,        //     Report Count (1)
        0x75, 0x08,        //     Report Size (8)
        0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
        0xC0,              //   End Collection
        0xC0,              // End Collection
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x80,        // Usage (Sys Control)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x03,        //   Report ID (3)
        0x19, 0x01,        //   Usage Minimum (Pointer)
        0x2A, 0xB7, 0x00,  //   Usage Maximum (Sys Display LCD Autoscale)
        0x15, 0x01,        //   Logical Minimum (1)
        0x26, 0xB7, 0x00,  //   Logical Maximum (183)
        0x95, 0x01,        //   Report Count (1)
        0x75, 0x10,        //   Report Size (16)
        0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xC0,              // End Collection
        0x05, 0x0C,        // Usage Page (Consumer)
        0x09, 0x01,        // Usage (Consumer Control)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x04,        //   Report ID (4)
        0x19, 0x01,        //   Usage Minimum (Consumer Control)
        0x2A, 0xA0, 0x02,  //   Usage Maximum (0x02A0)
        0x15, 0x01,        //   Logical Minimum (1)
        0x26, 0xA0, 0x02,  //   Logical Maximum (672)
        0x95, 0x01,        //   Report Count (1)
        0x75, 0x10,        //   Report Size (16)
        0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xC0,              // End Collection
};

/**********************************************************/
uint8_t        DevConfig, Ready;
uint8_t        SetupReqCode;
uint16_t       SetupReqLen;
const uint8_t *pDescr;
uint8_t        Report_Value[USB_INTERFACE_MAX_INDEX+1] = {0x00};
uint8_t        Idle_Value[USB_INTERFACE_MAX_INDEX+1] = {0x00};
uint8_t        USB_SleepStatus = 0x00; /* USB睡眠状态 */
void Debonding_layer_cfg(uint8_t *pbuf);
/*鼠标键盘数据*/
uint8_t HIDMouse[4] = {0x0, 0x0, 0x0, 0x0};
uint8_t HIDKey[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/******** 用户自定义分配端点RAM ****************************************/
__attribute__((aligned(4))) uint8_t EP0_Databuf[64 + 64 + 64]; //ep0(64)+ep4_out(64)+ep4_in(64)
__attribute__((aligned(4))) uint8_t EP1_Databuf[64 + 64];      //ep1_out(64)+ep1_in(64)
__attribute__((aligned(4))) uint8_t EP2_Databuf[64 + 64];      //ep2_out(64)+ep2_in(64)
__attribute__((aligned(4))) uint8_t EP3_Databuf[64 + 64];      //ep3_out(64)+ep3_in(64)

/*********************************************************************
 * @fn      USB_DevTransProcess
 *
 * @brief   USB 传输处理函数
 *
 * @return  none
 */
void USB_DevTransProcess(void)
{
    uint8_t len, chtype;
    uint8_t intflag, errflag = 0;

    intflag = R8_USB_INT_FG;
    if(intflag & RB_UIF_TRANSFER)
    {
        if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN) // 非空闲
        {
            switch(R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
            // 分析操作令牌和端点号
            {
                case UIS_TOKEN_IN:
                {
                    switch(SetupReqCode)
                    {
                        case USB_GET_DESCRIPTOR:
                            len = SetupReqLen >= DevEP0SIZE ? DevEP0SIZE : SetupReqLen; // 本次传输长度
                            memcpy(pEP0_DataBuf, pDescr, len);                        /* 加载上传数据 */
                            SetupReqLen -= len;
                            pDescr += len;
                            R8_UEP0_T_LEN = len;
                            R8_UEP0_CTRL ^= RB_UEP_T_TOG; // 翻转
                            break;
                        case USB_SET_ADDRESS:
                            R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | SetupReqLen;
                            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                            break;

                        case USB_SET_FEATURE:
                            break;

                        default:
                            R8_UEP0_T_LEN = 0; // 状态阶段完成中断或者是强制上传0长度数据包结束控制传输
                            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                            break;
                    }
                }
                break;

                case UIS_TOKEN_OUT:
                {
                    len = R8_USB_RX_LEN;
                    if(SetupReqCode == 0x09)
                    {
                        PRINT("[%s] Num Lock\t", (pEP0_DataBuf[0] & (1<<0)) ? "*" : " ");
                        PRINT("[%s] Caps Lock\t", (pEP0_DataBuf[0] & (1<<1)) ? "*" : " ");
                        PRINT("[%s] Scroll Lock\n", (pEP0_DataBuf[0] & (1<<2)) ? "*" : " ");
                    }
                }
                break;

                case UIS_TOKEN_OUT | 1:
                {
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    { // 不同步的数据包将丢弃
                        R8_UEP1_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP1_OUT_Deal(len);
                    }
                }
                break;

                case UIS_TOKEN_IN | 1:
                    R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                case UIS_TOKEN_OUT | 2:
                {
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    { // 不同步的数据包将丢弃
                        R8_UEP2_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP2_OUT_Deal(len);
                    }
                }
                break;

                case UIS_TOKEN_IN | 2:
                    R8_UEP2_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                case UIS_TOKEN_OUT | 3:
                {
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    { // 不同步的数据包将丢弃
                        R8_UEP3_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP3_OUT_Deal(len);
                    }
                }
                break;

                case UIS_TOKEN_IN | 3:
                    R8_UEP3_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                case UIS_TOKEN_OUT | 4:
                {
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    {
                        R8_UEP4_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP4_OUT_Deal(len);
                    }
                }
                break;

                case UIS_TOKEN_IN | 4:
                    R8_UEP4_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP4_CTRL = (R8_UEP4_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                default:
                    break;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
        if(R8_USB_INT_ST & RB_UIS_SETUP_ACT) // Setup包处理
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
            SetupReqLen = pSetupReqPak->wLength;
            SetupReqCode = pSetupReqPak->bRequest;
            chtype = pSetupReqPak->bRequestType;

            len = 0;
            errflag = 0;
            if((pSetupReqPak->bRequestType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
            {
                /* 非标准请求 */
                /* 其它请求,如类请求，产商请求等 */
                if(pSetupReqPak->bRequestType & 0x40)
                {
                    /* 厂商请求 */
                }
                else if(pSetupReqPak->bRequestType & 0x20)
                {
                    switch(SetupReqCode)
                    {
                        case DEF_USB_SET_IDLE: /* 0x0A: SET_IDLE */         //主机想设置HID设备特定输入报表的空闲时间间隔
                            Idle_Value[pSetupReqPak->wIndex] = (uint8_t)(pSetupReqPak->wValue>>8);
                            break; //这个一定要有

                        case DEF_USB_SET_REPORT: /* 0x09: SET_REPORT */     //主机想设置HID设备的报表描述符
                            break;

                        case DEF_USB_SET_PROTOCOL: /* 0x0B: SET_PROTOCOL */ //主机想设置HID设备当前所使用的协议
                            Report_Value[pSetupReqPak->wIndex] = (uint8_t)(pSetupReqPak->wValue);
                            break;

                        case DEF_USB_GET_IDLE: /* 0x02: GET_IDLE */         //主机想读取HID设备特定输入报表的当前的空闲比率
                            EP0_Databuf[0] = Idle_Value[pSetupReqPak->wIndex];
                            len = 1;
                            break;

                        case DEF_USB_GET_PROTOCOL: /* 0x03: GET_PROTOCOL */     //主机想获得HID设备当前所使用的协议
                            EP0_Databuf[0] = Report_Value[pSetupReqPak->wIndex];
                            len = 1;
                            break;

                        default:
                            errflag = 0xFF;
                    }
                }
            }
            else /* 标准请求 */
            {
                switch(SetupReqCode)
                {
                    case USB_GET_DESCRIPTOR:
                    {
                        switch(((pSetupReqPak->wValue) >> 8))
                        {
                            case USB_DESCR_TYP_DEVICE:
                            {
                                pDescr = MyDevDescr;
                                len = MyDevDescr[0];
                            }
                            break;

                            case USB_DESCR_TYP_CONFIG:
                            {
                                pDescr = MyCfgDescr;
                                len = MyCfgDescr[2];
                            }
                            break;

                            case USB_DESCR_TYP_HID:
                                switch((pSetupReqPak->wIndex) & 0xff)
                                {
                                    /* 选择接口 */
                                    case 0:
                                        pDescr = (uint8_t *)(&MyCfgDescr[18]);
                                        len = 9;
                                        break;

                                    case 1:
                                        pDescr = (uint8_t *)(&MyCfgDescr[43]);
                                        len = 9;
                                        break;

                                    default:
                                        /* 不支持的字符串描述符 */
                                        errflag = 0xff;
                                        break;
                                }
                                break;

                            case USB_DESCR_TYP_REPORT:
                            {
                                if(((pSetupReqPak->wIndex) & 0xff) == 0) //接口0报表描述符
                                {
                                    pDescr = KeyRepDesc; //数据准备上传
                                    len = sizeof(KeyRepDesc);
                                }

                                else if(((pSetupReqPak->wIndex) & 0xff) == 1) //接口1报表描述符
                                {
                                    pDescr = vial_Desc; //数据准备上传
                                    len = sizeof(vial_Desc);
                                }
                                else if(((pSetupReqPak->wIndex) & 0xff) == 2) //接口1报表描述符
                                {
                                    pDescr = Consumer_Desc; //数据准备上传
                                    len = sizeof(Consumer_Desc);
                                    Ready = 1; //如果有更多接口，该标准位应该在最后一个接口配置完成后有效
                                }
                                else
                                    len = 0xff; //本程序只有2个接口，这句话正常不可能执行
                            }
                            break;

                            case USB_DESCR_TYP_STRING:
                            {
                                switch((pSetupReqPak->wValue) & 0xff)
                                {
                                    case 1:
                                        pDescr = MyManuInfo;
                                        len = MyManuInfo[0];
                                        break;
                                    case 2:
                                        pDescr = MyProdInfo;
                                        len = MyProdInfo[0];
                                        break;
                                    case 0:
                                        pDescr = MyLangDescr;
                                        len = MyLangDescr[0];
                                        break;
                                    case 3:
                                        pDescr = MyManuInfo;
                                        len = MyManuInfo[0];
                                        break;
                                    default:
                                        errflag = 0xFF; // 不支持的字符串描述符
                                        break;
                                }
                            }
                            break;

                            case 0x06:
                                pDescr = (uint8_t *)(&My_QueDescr[0]);
                                len = sizeof(My_QueDescr);
                                break;

                            case 0x07:
                                memcpy(&USB_FS_OSC_DESC[2], &MyCfgDescr[2], sizeof(MyCfgDescr) - 2);
                                pDescr = (uint8_t *)(&USB_FS_OSC_DESC[0]);
                                len = sizeof(USB_FS_OSC_DESC);
                                break;

                            default:
                                errflag = 0xff;
                                break;
                        }
                        if(SetupReqLen > len)
                            SetupReqLen = len; //实际需上传总长度
                        len = (SetupReqLen >= DevEP0SIZE) ? DevEP0SIZE : SetupReqLen;
                        memcpy(pEP0_DataBuf, pDescr, len);
                        pDescr += len;
                    }
                    break;

                    case USB_SET_ADDRESS:
                        SetupReqLen = (pSetupReqPak->wValue) & 0xff;
                        break;

                    case USB_GET_CONFIGURATION:
                        pEP0_DataBuf[0] = DevConfig;
                        if(SetupReqLen > 1)
                            SetupReqLen = 1;
                        break;

                    case USB_SET_CONFIGURATION:
                        DevConfig = (pSetupReqPak->wValue) & 0xff;
                        break;

                    case USB_CLEAR_FEATURE:
                    {
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) // 端点
                        {
                            switch((pSetupReqPak->wIndex) & 0xff)
                            {
                                case 0x83:
                                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x03:
                                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                case 0x82:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x02:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                case 0x81:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x01:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                default:
                                    errflag = 0xFF; // 不支持的端点
                                    break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            if(pSetupReqPak->wValue == 1)
                            {
                                USB_SleepStatus &= ~0x01;
                            }
                        }
                        else
                        {
                            errflag = 0xFF;
                        }
                    }
                    break;

                    case USB_SET_FEATURE:
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                        {
                            /* 端点 */
                            switch(pSetupReqPak->wIndex)
                            {
                                case 0x83:
                                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
                                    break;
                                case 0x03:
                                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
                                    break;
                                case 0x82:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
                                    break;
                                case 0x02:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
                                    break;
                                case 0x81:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
                                    break;
                                case 0x01:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
                                    break;
                                default:
                                    /* 不支持的端点 */
                                    errflag = 0xFF; // 不支持的端点
                                    break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            if(pSetupReqPak->wValue == 1)
                            {
                                /* 设置睡眠 */
                                USB_SleepStatus |= 0x01;
                            }
                        }
                        else
                        {
                            errflag = 0xFF;
                        }
                        break;

                    case USB_GET_INTERFACE:
                        pEP0_DataBuf[0] = 0x00;
                        if(SetupReqLen > 1)
                            SetupReqLen = 1;
                        break;

                    case USB_SET_INTERFACE:
                        break;

                    case USB_GET_STATUS:
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                        {
                            /* 端点 */
                            pEP0_DataBuf[0] = 0x00;
                            switch(pSetupReqPak->wIndex)
                            {
                                case 0x83:
                                    if((R8_UEP3_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                                    {
                                        pEP0_DataBuf[0] = 0x01;
                                    }
                                    break;

                                case 0x03:
                                    if((R8_UEP3_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                                    {
                                        pEP0_DataBuf[0] = 0x01;
                                    }
                                    break;

                                case 0x82:
                                    if((R8_UEP2_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                                    {
                                        pEP0_DataBuf[0] = 0x01;
                                    }
                                    break;

                                case 0x02:
                                    if((R8_UEP2_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                                    {
                                        pEP0_DataBuf[0] = 0x01;
                                    }
                                    break;

                                case 0x81:
                                    if((R8_UEP1_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                                    {
                                        pEP0_DataBuf[0] = 0x01;
                                    }
                                    break;

                                case 0x01:
                                    if((R8_UEP1_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                                    {
                                        pEP0_DataBuf[0] = 0x01;
                                    }
                                    break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            pEP0_DataBuf[0] = 0x00;
                            if(USB_SleepStatus)
                            {
                                pEP0_DataBuf[0] = 0x02;
                            }
                            else
                            {
                                pEP0_DataBuf[0] = 0x00;
                            }
                        }
                        pEP0_DataBuf[1] = 0;
                        if(SetupReqLen >= 2)
                        {
                            SetupReqLen = 2;
                        }
                        break;

                    default:
                        errflag = 0xff;
                        break;
                }
            }
            if(errflag == 0xff) // 错误或不支持
            {
                //                  SetupReqCode = 0xFF;
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
            }
            else
            {
                if(chtype & 0x80) // 上传
                {
                    len = (SetupReqLen > DevEP0SIZE) ? DevEP0SIZE : SetupReqLen;
                    SetupReqLen -= len;
                }
                else
                    len = 0; // 下传
                R8_UEP0_T_LEN = len;
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; // 默认数据包是DATA1
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if(intflag & RB_UIF_BUS_RST)
    {
        R8_USB_DEV_AD = 0;
        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if(intflag & RB_UIF_SUSPEND)
    {
        if(R8_USB_MIS_ST & RB_UMS_SUSPEND)
        {
            ;
        } // 挂起
        else
        {
            ;
        } // 唤醒
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
    else
    {
        R8_USB_INT_FG = intflag;
    }
}

/*********************************************************************
 * @fn      DevHIDMouseReport
 *
 * @brief   上报鼠标数据
 *
 * @return  none
 */
void U2DevHIDMouseReport(uint8_t mouse)
{
    HIDMouse[0] = mouse;
    memcpy(pEP2_IN_DataBuf, HIDMouse, sizeof(HIDMouse));
    DevEP2_IN_Deal(sizeof(HIDMouse));
}

/*********************************************************************
 * @fn      U2DevHIDKeyReport
 *
 * @brief   上报键盘数据
 *
 * @return  none
 */
void U2DevHIDKeyReport(uint8_t *key)
{
//    HIDKey[2] = key;
    memcpy(&HIDKey[2], key, 6);
    memcpy(pEP1_IN_DataBuf, HIDKey, 8);
    DevEP1_IN_Deal(8);
}

/*********************************************************************
 * @fn      U2DevWakeup
 *
 * @brief   设备模式唤醒主机
 *
 * @return  none
 */
void U2DevWakeup(void)
{
    R16_PIN_ANALOG_IE &= ~(RB_PIN_USB_DP_PU);
    R8_UDEV_CTRL |= RB_UD_LOW_SPEED;
    mDelaymS(2);
    R8_UDEV_CTRL &= ~RB_UD_LOW_SPEED;
    R16_PIN_ANALOG_IE |= RB_PIN_USB_DP_PU;
}

/*********************************************************************
 * @fn      DebugInit
 *
 * @brief   调试初始化
 *
 * @return  none
 */
void DebugInit(void)
{
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
}
/*********************************************************************
 * @fn      Main_Circulation_USB
 *
 * @brief   USB模式主循环
 *
 * @return  none
 */
__HIGH_CODE
__attribute__((noinline))
void Main_Circulation_USB()
{
    while(1)
    {
//        if (rgb_flag > 50) {
//            PWM_DATA_DMA_send(Pwmout_buf,sizeof(Pwmout_buf));
//            rgb_flag = 0;
//        }
//        rgb_flag++;
//        if (rgb_flag>=40) {
//            process_RGB_to_pwm(flowing_buf, 17, Pwmout_buf);
//           // Ws2812_move_control(flowing_buf_usb,RGB_Left_flowing_water,17);
//            rgb_flag = 51;
//        }
//        else {
//            DelayMs(4);
//        }
    }
}
/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
void USB_INIT(void)
{
    PRINT("USB MODE!!\r\n");
    pEP0_RAM_Addr = EP0_Databuf;
    pEP1_RAM_Addr = EP1_Databuf;
    pEP2_RAM_Addr = EP2_Databuf;
    pEP3_RAM_Addr = EP3_Databuf;
    USB_DeviceInit();
    PFIC_EnableIRQ(USB_IRQn);
    TMR3_TimerInit(90000);         // 设置定时时间 1.5ms
    TMR3_ITCfg(ENABLE, TMR0_3_IT_CYC_END); // 开启中断
    PFIC_EnableIRQ(TMR3_IRQn);
    Main_Circulation_USB();
}
/*********************************************************************
 * @fn      DevEP1_OUT_Deal
 *
 * @brief   端点1数据处理
 *
 * @return  none
 */
void DevEP1_OUT_Deal(uint8_t l)
{ /* 用户可自定义 */
    uint8_t i;

    for(i = 0; i < l; i++)
    {
        pEP1_IN_DataBuf[i] = ~pEP1_OUT_DataBuf[i];
    }
    DevEP1_IN_Deal(l);
}

/*********************************************************************
 * @fn      DevEP2_OUT_Deal
 *
 * @brief   端点2数据处理
 *
 * @return  none
 */
void DevEP2_OUT_Deal(uint8_t l)
{ /* 用户可自定义 */
    uint8_t i;

    for(i = 0; i < l; i++)
    {
        pEP2_IN_DataBuf[i] = ~pEP2_OUT_DataBuf[i];
    }
    DevEP2_IN_Deal(l);
}

/*********************************************************************
 * @fn      DevEP3_OUT_Deal
 *
 * @brief   端点3数据处理
 *
 * @return  none
 */

void DevEP3_OUT_Deal(uint8_t l)
{
    memset(pEP2_IN_DataBuf,0,32);
    memset(key_chang_data,0,32);
    if (pEP3_OUT_DataBuf[0] == 0x01) {
        USB_VIAL_START = 1;
    }
    if (vial_data_count == 77) {
        USB_VIAL_START = 0;
        vial_data_count = 0;
    }
    if (USB_VIAL_START == 1) { //vial支持处理，闭源
        if (pEP3_OUT_DataBuf[0] == 0x12) {
            if (pEP3_OUT_DataBuf[2] == 0) {
                key_chang_data[0] = 0x12;
                key_chang_data[3] = 0x1c;
                key_chang_data[5] = key_data_buf[0][0];
                key_chang_data[7] = key_data_buf[0][1];
                key_chang_data[9] = key_data_buf[0][2];
                key_chang_data[11] = key_data_buf[0][3];

                key_chang_data[13] = key_data_buf[1][0];
                key_chang_data[15] = key_data_buf[1][1];
                key_chang_data[17] = key_data_buf[1][2];

                key_chang_data[21] = key_data_buf[2][0];
                key_chang_data[23] = key_data_buf[2][1];
                key_chang_data[25] = key_data_buf[2][2];
                key_chang_data[27] = key_data_buf[1][3];

                key_chang_data[29] = key_data_buf[3][0];
                key_chang_data[31] = key_data_buf[3][1];
                memcpy(pEP2_IN_DataBuf,key_chang_data,32);
                vial_data_count ++;
            }
            else if (pEP3_OUT_DataBuf[2] == 0x1c) {
                key_chang_data[0] = 0x12;
                key_chang_data[2] = 0x1c;
                key_chang_data[3] = 0x1c;
                key_chang_data[5] = key_data_buf[3][2];
                key_chang_data[9] = key_data_buf[4][0];
                key_chang_data[11] = key_data_buf[4][2];
                key_chang_data[13] = key_data_buf[4][3];
                key_chang_data[15] = 0x04;
                key_chang_data[17] = 0x04;
                key_chang_data[21] = 0x04;
                key_chang_data[23] = 0x04;
                key_chang_data[25] = 0x04;
                key_chang_data[27] = 0x04;
                key_chang_data[29] = 0x04;
                key_chang_data[31] = key_data_buf[3][1];
                memcpy(pEP2_IN_DataBuf,key_chang_data,32);
                vial_data_count ++;
            }
            else {
                FLASH_DATA_VIAL((uint32_t)(vial_data_count*32), pEP2_IN_DataBuf);
                vial_data_count ++;
            }
        }
        else {
             FLASH_DATA_VIAL((vial_data_count*32), pEP2_IN_DataBuf);
             vial_data_count ++;
        }
    }
    else {//改键层处理
        switch(pEP3_OUT_DataBuf[0])
        {
           case 0x05:
              Debonding_layer_cfg(pEP3_OUT_DataBuf);
              memcpy(pEP2_IN_DataBuf,pEP3_OUT_DataBuf,32);
              break;
           default:
           {
               memcpy(pEP2_IN_DataBuf,pEP3_OUT_DataBuf,32);
           }
               break;
        }
    }
    DevEP2_IN_Deal(32);
}

/*********************************************************************
 * @fn      DevEP4_OUT_Deal
 *
 * @brief   端点4数据处理
 *
 * @return  none
 */
void DevEP4_OUT_Deal(uint8_t l)
{ /* 用户可自定义 */
    uint8_t i;

    for(i = 0; i < l; i++)
    {
        pEP4_IN_DataBuf[i] = ~pEP4_OUT_DataBuf[i];
    }
    DevEP4_IN_Deal(l);
}

/*********************************************************************
 * @fn      USB_IRQHandler
 *
 * @brief   USB2中断函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void) /* USB中断服务程序,使用寄存器组1 */
{
    USB_DevTransProcess();
}
/*********************************************************************
 * @fn      TMR3_IRQHandler
 *
 * @brief   TMR3中断函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void TMR3_IRQHandler(void) // TMR3 定时中断
{
    if(TMR3_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR3_ClearITFlag(TMR0_3_IT_CYC_END); // 清除中断标志
        memset(scan_buf,0,6);
        scan_flag = get_key_fanz(scan_buf);
        if (memcmp(scan_buf,last_buf,6) == 0) {
            if (scan_flag == 0) {
                change_mode_BLE = 0;
                change_mode_24 = 0;
                change_mode_USB = 0;
            }
            else if (scan_flag == 1) {
                if (scan_buf[0]==key_data_buf[1][0]) {
                    change_mode_USB++;

                }
                else if (scan_buf[0]==key_data_buf[1][1]) {
                    //BLE MODE
                    change_mode_BLE++;
                }
                else if (scan_buf[0]==key_data_buf[1][2]) {
                    //2.4 MODE
                    change_mode_24++;
                }
                else {

                }
            }
        }
        else {
            change_mode_BLE = 0;
            change_mode_24 = 0;
            change_mode_USB = 0;
            U2DevHIDKeyReport(scan_buf);

        }
        memcpy(last_buf,scan_buf,6);
        if (change_mode_BLE == 1500) {
            uint8_t key[1] = {0xBE};
            FLASH_DATA_VIAL_WITE_mode(key);
            DelayMs(1);
            SYS_ResetExecute();
        }
        if (change_mode_24 == 1500) {
            uint8_t key[1] = {0x24};
            FLASH_DATA_VIAL_WITE_mode(key);
            DelayMs(1);
            SYS_ResetExecute();
        }
        if (change_mode_USB == 1500) {
            uint8_t key[1] = {0x0B};
            FLASH_DATA_VIAL_WITE_mode(key);
            DelayMs(1);
            SYS_ResetExecute();
        }
    }
}

/*********************************************************************
 * @fn      Debonding_layer_cfg
 *
 * @brief   改键层配置函数,将改键软件下发的键值存进data flash里
 *
 * @return  none
 */
#define Key_position_0  0
#define Key_position_1  1
#define Key_position_2  2
#define Key_position_3  3
#define Key_length    20
__HIGH_CODE
void Debonding_layer_cfg(uint8_t *pbuf)
{
    uint32_t key_add;
    switch (pbuf[1]) {
        case 0:
            key_add = Key_position_0;
            break;
        case 1:
            key_add = Key_position_1;
            break;
        case 2:
            key_add = Key_position_2;
            break;
        case 3:
            key_add = Key_position_3;
            break;
        default:
            break;
    }
    if (pbuf[4] == 0) {
        if (pbuf[2]==2&&pbuf[3]==3) {
            key_data_buf[1][3]= pbuf[5];
        }
        else if (pbuf[2]==4&&pbuf[3]==1) {
            key_data_buf[4][2]= pbuf[5];
        }
        else if (pbuf[2]==4&&pbuf[3]==2) {
            key_data_buf[4][3]= pbuf[5];
        }
        else {
            key_data_buf[pbuf[2]][pbuf[3]]= pbuf[5];
        }
    }
    else if(pbuf[4] == 2)
    {
        key_data_buf[pbuf[2]][pbuf[3]] = pbuf[5];
    }
    else{//特殊按键处理，待添加

    }
    uint8_t data_buf[20];
    memcpy(data_buf,key_data_buf,20);
    FLASH_DATA_VIAL_WITE_key(key_add, data_buf, Key_length);
}



