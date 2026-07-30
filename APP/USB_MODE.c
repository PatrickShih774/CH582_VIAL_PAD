/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2024/11/06
 * Description        : USBģʽͨ��������
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
#include "vial_protocol.h"
#include "vial_definition.h"
#define DevEP0SIZE    0x40
uint8_t USB_VIAL_START = 0;
uint8_t vial_data_count = 0;
// ֧�ֵ����ӿ�����
#define USB_INTERFACE_MAX_NUM       2
// �ӿںŵ����ֵ
#define USB_INTERFACE_MAX_INDEX      1

uint8_t key_chang_data[32] = {0};
//uint8_t scan_flag = 0;
uint8_t rgb_flag = 0;

// �豸������
const uint8_t MyDevDescr[] = {0x12,0x01,0x00,0x02,0x00,0x00,
                              0x00,0x40,0x73,0x92,0x57,0x91,
                              0x00,0x01,0x01,0x02,0x03,0x01};
// ����������
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
/* USB�ٶ�ƥ�������� */
const uint8_t My_QueDescr[] = {0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00};

/* USBȫ��ģʽ,�����ٶ����������� */
uint8_t USB_FS_OSC_DESC[sizeof(MyCfgDescr)] = {
    0x09, 0x07, /* ��������ͨ�������� */
};

// ����������
const uint8_t MyLangDescr[] = {0x04, 0x03, 0x09, 0x04};
// ������Ϣ
const uint8_t MyManuInfo[] = {0x1C,0x03,0x76,0x00,0x69,0x00,0x61,0x00,0x6C,0x00,
                              0x3A,0x00,0x66,0x00,0x36,0x00,0x34,0x00,0x63,0x00,
                              0x32,0x00,0x62,0x00,0x33,0x00,0x63,0x00};
// ��Ʒ��Ϣ
const uint8_t MyProdInfo[] = {0x0C, 0x03, 'E', 0, 'B', 0, 'P', 0, '1', 0, '7', 0};
/*HID�౨��������*/
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
uint8_t        USB_SleepStatus = 0x00; /* USB˯��״̬ */
void Debonding_layer_cfg(uint8_t *pbuf);
/*����������*/
uint8_t HIDMouse[4] = {0x0, 0x0, 0x0, 0x0};
uint8_t HIDKey[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/******** �û��Զ������˵�RAM ****************************************/
__attribute__((aligned(4))) uint8_t EP0_Databuf[64 + 64 + 64]; //ep0(64)+ep4_out(64)+ep4_in(64)
__attribute__((aligned(4))) uint8_t EP1_Databuf[64 + 64];      //ep1_out(64)+ep1_in(64)
__attribute__((aligned(4))) uint8_t EP2_Databuf[64 + 64];      //ep2_out(64)+ep2_in(64)
__attribute__((aligned(4))) uint8_t EP3_Databuf[64 + 64];      //ep3_out(64)+ep3_in(64)

/*********************************************************************
 * @fn      USB_DevTransProcess
 *
 * @brief   USB ���䴦������
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
        if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN) // �ǿ���
        {
            switch(R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
            // �����������ƺͶ˵��
            {
                case UIS_TOKEN_IN:
                {
                    switch(SetupReqCode)
                    {
                        case USB_GET_DESCRIPTOR:
                            len = SetupReqLen >= DevEP0SIZE ? DevEP0SIZE : SetupReqLen; // ���δ��䳤��
                            memcpy(pEP0_DataBuf, pDescr, len);                        /* �����ϴ����� */
                            SetupReqLen -= len;
                            pDescr += len;
                            R8_UEP0_T_LEN = len;
                            R8_UEP0_CTRL ^= RB_UEP_T_TOG; // ��ת
                            break;
                        case USB_SET_ADDRESS:
                            R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | SetupReqLen;
                            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                            break;

                        case USB_SET_FEATURE:
                            break;

                        default:
                            R8_UEP0_T_LEN = 0; // ״̬�׶�����жϻ�����ǿ���ϴ�0�������ݰ��������ƴ���
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
                    { // ��ͬ�������ݰ�������
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
                    { // ��ͬ�������ݰ�������
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
                    { // ��ͬ�������ݰ�������
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
        if(R8_USB_INT_ST & RB_UIS_SETUP_ACT) // Setup������
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
            SetupReqLen = pSetupReqPak->wLength;
            SetupReqCode = pSetupReqPak->bRequest;
            chtype = pSetupReqPak->bRequestType;

            len = 0;
            errflag = 0;
            if((pSetupReqPak->bRequestType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
            {
                /* �Ǳ�׼���� */
                /* ��������,�������󣬲�������� */
                if(pSetupReqPak->bRequestType & 0x40)
                {
                    /* �������� */
                }
                else if(pSetupReqPak->bRequestType & 0x20)
                {
                    switch(SetupReqCode)
                    {
                        case DEF_USB_SET_IDLE: /* 0x0A: SET_IDLE */         //����������HID�豸�ض����뱨���Ŀ���ʱ����
                            Idle_Value[pSetupReqPak->wIndex] = (uint8_t)(pSetupReqPak->wValue>>8);
                            break; //���һ��Ҫ��

                        case DEF_USB_SET_REPORT: /* 0x09: SET_REPORT */     //����������HID�豸�ı���������
                            break;

                        case DEF_USB_SET_PROTOCOL: /* 0x0B: SET_PROTOCOL */ //����������HID�豸��ǰ��ʹ�õ�Э��
                            Report_Value[pSetupReqPak->wIndex] = (uint8_t)(pSetupReqPak->wValue);
                            break;

                        case DEF_USB_GET_IDLE: /* 0x02: GET_IDLE */         //�������ȡHID�豸�ض����뱨���ĵ�ǰ�Ŀ��б���
                            EP0_Databuf[0] = Idle_Value[pSetupReqPak->wIndex];
                            len = 1;
                            break;

                        case DEF_USB_GET_PROTOCOL: /* 0x03: GET_PROTOCOL */     //��������HID�豸��ǰ��ʹ�õ�Э��
                            EP0_Databuf[0] = Report_Value[pSetupReqPak->wIndex];
                            len = 1;
                            break;

                        default:
                            errflag = 0xFF;
                    }
                }
            }
            else /* ��׼���� */
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
                                    /* ѡ��ӿ� */
                                    case 0:
                                        pDescr = (uint8_t *)(&MyCfgDescr[18]);
                                        len = 9;
                                        break;

                                    case 1:
                                        pDescr = (uint8_t *)(&MyCfgDescr[43]);
                                        len = 9;
                                        break;

                                    default:
                                        /* ��֧�ֵ��ַ��������� */
                                        errflag = 0xff;
                                        break;
                                }
                                break;

                            case USB_DESCR_TYP_REPORT:
                            {
                                if(((pSetupReqPak->wIndex) & 0xff) == 0) //�ӿ�0����������
                                {
                                    pDescr = KeyRepDesc; //����׼���ϴ�
                                    len = sizeof(KeyRepDesc);
                                }

                                else if(((pSetupReqPak->wIndex) & 0xff) == 1) //�ӿ�1����������
                                {
                                    pDescr = vial_Desc; //����׼���ϴ�
                                    len = sizeof(vial_Desc);
                                }
                                else if(((pSetupReqPak->wIndex) & 0xff) == 2) //�ӿ�1����������
                                {
                                    pDescr = Consumer_Desc; //����׼���ϴ�
                                    len = sizeof(Consumer_Desc);
                                    Ready = 1; //����и���ӿڣ��ñ�׼λӦ�������һ���ӿ�������ɺ���Ч
                                }
                                else
                                    len = 0xff; //������ֻ��2���ӿڣ���仰����������ִ��
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
                                        errflag = 0xFF; // ��֧�ֵ��ַ���������
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
                            SetupReqLen = len; //ʵ�����ϴ��ܳ���
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
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) // �˵�
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
                                    errflag = 0xFF; // ��֧�ֵĶ˵�
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
                            /* �˵� */
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
                                    /* ��֧�ֵĶ˵� */
                                    errflag = 0xFF; // ��֧�ֵĶ˵�
                                    break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            if(pSetupReqPak->wValue == 1)
                            {
                                /* ����˯�� */
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
                            /* �˵� */
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
            if(errflag == 0xff) // �����֧��
            {
                //                  SetupReqCode = 0xFF;
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
            }
            else
            {
                if(chtype & 0x80) // �ϴ�
                {
                    len = (SetupReqLen > DevEP0SIZE) ? DevEP0SIZE : SetupReqLen;
                    SetupReqLen -= len;
                }
                else
                    len = 0; // �´�
                R8_UEP0_T_LEN = len;
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; // Ĭ�����ݰ���DATA1
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
        } // ����
        else
        {
            ;
        } // ����
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
 * @brief   �ϱ��������
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
 * @brief   �ϱ���������
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
 * @brief   �豸ģʽ��������
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
 * @brief   ���Գ�ʼ��
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
 * @brief   USBģʽ��ѭ��
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
 * @brief   ������
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
    TMR3_TimerInit(90000);         // ���ö�ʱʱ�� 1.5ms
    TMR3_ITCfg(ENABLE, TMR0_3_IT_CYC_END); // �����ж�
    PFIC_EnableIRQ(TMR3_IRQn);
    Main_Circulation_USB();
}
/*********************************************************************
 * @fn      DevEP1_OUT_Deal
 *
 * @brief   �˵�1���ݴ���
 *
 * @return  none
 */
void DevEP1_OUT_Deal(uint8_t l)
{ /* �û����Զ��� */
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
 * @brief   �˵�2���ݴ���
 *
 * @return  none
 */
void DevEP2_OUT_Deal(uint8_t l)
{ /* �û����Զ��� */
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
 * @brief   Standard VIA/Vial protocol handler (EP3 = vial raw HID)
 *          - VIA commands: 0x01–0x0D (protocol version, keymap get/set, …)
 *          - Vial commands: 0xFE prefix + sub-command (keyboard_id,
 *            definition size/pages, unlock, …)
 *
 * @return  none
 */

/* ── Per-layer keycode helpers ─────────────────────────────────────── */
static uint8_t *layer_keymaps[VIAL_LAYER_COUNT] = {
    (uint8_t *)key_data_buf,
    (uint8_t *)key_data_buf_1,
    (uint8_t *)key_data_buf_2,
    (uint8_t *)key_data_buf_3,
};

static uint8_t via_get_keycode(uint8_t layer, uint8_t row, uint8_t col)
{
    if (layer >= VIAL_LAYER_COUNT || row >= VIAL_MATRIX_ROWS || col >= VIAL_MATRIX_COLS)
        return 0x00;
    return layer_keymaps[layer][row * VIAL_MATRIX_COLS + col];
}

static void via_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint8_t kc)
{
    if (layer >= VIAL_LAYER_COUNT || row >= VIAL_MATRIX_ROWS || col >= VIAL_MATRIX_COLS)
        return;
    layer_keymaps[layer][row * VIAL_MATRIX_COLS + col] = kc;
}

static void via_save_layer(uint8_t layer)
{
    uint8_t buf[20];
    memcpy(buf, layer_keymaps[layer], 20);
    FLASH_DATA_VIAL_WITE_key(layer, buf, 20);
}

void DevEP3_OUT_Deal(uint8_t l)
{
    uint8_t cmd = pEP3_OUT_DataBuf[0];
    memset(pEP2_IN_DataBuf, 0, 32);

    if (cmd == VIAL_CMD_PREFIX) {
        /* ── Vial-specific commands ────────────────────────────────
         * Response format: handlers write data at offset 0 (no prefix).
         * The host knows which command it sent and parses accordingly.
         */
        uint8_t vial_cmd = pEP3_OUT_DataBuf[1];

        switch (vial_cmd) {
        case VIAL_GET_KEYBOARD_ID: {   /* 0x00 */
            /* response: [pv0..pv3, uid0..uid7, flags] = 13 bytes */
            uint8_t uid[] = VIAL_KEYBOARD_UID;
            uint32_t pv = VIAL_PROTOCOL_VERSION;
            pEP2_IN_DataBuf[0] = (uint8_t)(pv & 0xFF);
            pEP2_IN_DataBuf[1] = (uint8_t)((pv >> 8) & 0xFF);
            pEP2_IN_DataBuf[2] = (uint8_t)((pv >> 16) & 0xFF);
            pEP2_IN_DataBuf[3] = (uint8_t)((pv >> 24) & 0xFF);
            memcpy(&pEP2_IN_DataBuf[4], uid, 8);
            pEP2_IN_DataBuf[12] = 0; /* flags: no vialrgb */
            break;
        }
        case VIAL_GET_SIZE: {          /* 0x01 */
            /* response: [sz0..sz3] — 4 bytes LE at offset 0 */
            uint32_t sz = VIAL_DEFINITION_SIZE;
            pEP2_IN_DataBuf[0] = (uint8_t)(sz & 0xFF);
            pEP2_IN_DataBuf[1] = (uint8_t)((sz >> 8) & 0xFF);
            pEP2_IN_DataBuf[2] = (uint8_t)((sz >> 16) & 0xFF);
            pEP2_IN_DataBuf[3] = (uint8_t)((sz >> 24) & 0xFF);
            break;
        }
        case VIAL_GET_DEFINITION: {    /* 0x02 */
            /* request: [0xFE, 0x02, page_lo, page_hi]
             * response: 32 raw bytes of definition, NO header */
            uint32_t page = (uint32_t)pEP3_OUT_DataBuf[2]
                         | ((uint32_t)pEP3_OUT_DataBuf[3] << 8);
            uint32_t offset = page * 32;
            uint32_t sz = VIAL_DEFINITION_SIZE;
            uint8_t len = 32;
            if (offset + len > sz) len = (uint8_t)(sz - offset);
            if (offset < sz) {
                memcpy(pEP2_IN_DataBuf, &vial_definition_data[offset], len);
            }
            break;
        }
        case VIAL_GET_ENCODER:         /* 0x03 — no encoders */
            pEP2_IN_DataBuf[0] = 0;
            break;
        case VIAL_SET_ENCODER:         /* 0x04 — no-op */
            break;
        case VIAL_GET_UNLOCK_STATUS:   /* 0x05 — always unlocked */
            pEP2_IN_DataBuf[0] = 0x01; /* unlocked */
            break;
        case VIAL_UNLOCK:              /* 0x06 */
            pEP2_IN_DataBuf[0] = 0x00; /* success */
            break;
        case VIAL_GET_LAYER_OPTIONS:   /* 0x07 — no layer options */
            /* return 4 zero bytes (no options for any layer) */
            break;
        case VIAL_SET_LAYER_OPTIONS:   /* 0x08 — no-op */
            break;
        case VIAL_QMK_SETTINGS_QUERY:  /* 0x09 — no QMK settings */
            /* return 0xFFFF as first qsid → end of list immediately */
            pEP2_IN_DataBuf[0] = 0xFF;
            pEP2_IN_DataBuf[1] = 0xFF;
            break;
        case VIAL_QMK_SETTINGS_GET:    /* 0x0A — no settings to get */
        case VIAL_QMK_SETTINGS_SET:    /* 0x0B — no-op */
        case VIAL_QMK_SETTINGS_RESET:  /* 0x0C — no-op */
        case VIAL_DYNAMIC_ENTRY_OP:    /* 0x0D — no dynamic entries */
            break;
        default:
            memcpy(pEP2_IN_DataBuf, pEP3_OUT_DataBuf, 32); /* echo */
            break;
        }
    } else {
        /* ── VIA commands ───────────────────────────────────────── */
        switch (cmd) {
        case VIA_GET_PROTOCOL_VERSION: {   /* 0x01 */
            /* VIA protocol: msg[0]=cmd, msg[1]=hi, msg[2]=lo (big-endian) */
            pEP2_IN_DataBuf[0] = VIA_GET_PROTOCOL_VERSION;
            pEP2_IN_DataBuf[1] = (uint8_t)((VIA_PROTOCOL_VERSION >> 8) & 0xFF);
            pEP2_IN_DataBuf[2] = (uint8_t)(VIA_PROTOCOL_VERSION & 0xFF);
            break;
        }
        case VIA_GET_KEYBOARD_VALUE: {     /* 0x02 */
            pEP2_IN_DataBuf[0] = VIA_GET_KEYBOARD_VALUE;
            pEP2_IN_DataBuf[1] = pEP3_OUT_DataBuf[1]; /* value_id */
            /* Most values are zero (no lighting, no matrix state) */
            break;
        }
        case VIA_DYNAMIC_KEYMAP_GET_KEYCODE: { /* 0x04 */
            uint8_t layer = pEP3_OUT_DataBuf[1];
            uint8_t row   = pEP3_OUT_DataBuf[2];
            uint8_t col   = pEP3_OUT_DataBuf[3];
            uint8_t kc    = via_get_keycode(layer, row, col);

            pEP2_IN_DataBuf[0] = VIA_DYNAMIC_KEYMAP_GET_KEYCODE;
            pEP2_IN_DataBuf[1] = layer;
            pEP2_IN_DataBuf[2] = row;
            pEP2_IN_DataBuf[3] = col;
            pEP2_IN_DataBuf[4] = 0x00;  /* keycode hi */
            pEP2_IN_DataBuf[5] = kc;    /* keycode lo */
            break;
        }
        case VIA_DYNAMIC_KEYMAP_SET_KEYCODE: { /* 0x05 */
            uint8_t layer = pEP3_OUT_DataBuf[1];
            uint8_t row   = pEP3_OUT_DataBuf[2];
            uint8_t col   = pEP3_OUT_DataBuf[3];
            uint8_t kc    = pEP3_OUT_DataBuf[5]; /* keycode lo */

            via_set_keycode(layer, row, col, kc);
            via_save_layer(layer);

            /* echo back */
            memcpy(pEP2_IN_DataBuf, pEP3_OUT_DataBuf, 32);
            break;
        }
        case VIA_MACRO_GET_COUNT: {            /* 0x0C — 0 macros */
            pEP2_IN_DataBuf[0] = VIA_MACRO_GET_COUNT;
            pEP2_IN_DataBuf[1] = 0x00; /* count = 0 */
            break;
        }
        case VIA_DYNAMIC_KEYMAP_GET_BUFFER: {   /* 0x11 — bulk read keymap buffer */
            /* request: [0x11, offset_lo, offset_hi, size]
             * response: [0x11, offset_lo, offset_hi, size, data[size]] */
            uint16_t offset = (uint16_t)pEP3_OUT_DataBuf[1]
                           | ((uint16_t)pEP3_OUT_DataBuf[2] << 8);
            uint8_t size = pEP3_OUT_DataBuf[3];
            uint8_t i;
            if (size > 28) size = 28;
            pEP2_IN_DataBuf[0] = VIA_DYNAMIC_KEYMAP_GET_BUFFER;
            pEP2_IN_DataBuf[1] = (uint8_t)(offset & 0xFF);
            pEP2_IN_DataBuf[2] = (uint8_t)((offset >> 8) & 0xFF);
            pEP2_IN_DataBuf[3] = size;
            for (i = 0; i < size; i++) {
                uint16_t pos = offset + (uint16_t)i;
                uint8_t layer = pos / VIAL_MATRIX_SIZE;
                uint8_t rc   = pos % VIAL_MATRIX_SIZE;
                uint8_t row  = rc / VIAL_MATRIX_COLS;
                uint8_t col  = rc % VIAL_MATRIX_COLS;
                if (layer < VIAL_LAYER_COUNT && row < VIAL_MATRIX_ROWS && col < VIAL_MATRIX_COLS)
                    pEP2_IN_DataBuf[4 + i] = via_get_keycode(layer, row, col);
            }
            break;
        }
        case VIA_DYNAMIC_KEYMAP_SET_BUFFER:  /* 0x12 — echo, no-op */
        case VIA_MACRO_GET_BUFFER:           /* 0x0D — 0 macros, no data */
        case VIA_SET_KEYBOARD_VALUE:         /* 0x03 */
        case VIA_DYNAMIC_KEYMAP_RESET:       /* 0x06 */
        case VIA_LIGHTING_SET_VALUE:         /* 0x07 */
        case VIA_LIGHTING_GET_VALUE:         /* 0x08 */
        case VIA_LIGHTING_SAVE:              /* 0x09 */
        case VIA_EEPROM_RESET:               /* 0x0A */
        case VIA_BOOTLOADER_JUMP:            /* 0x0B */
        default:
            memcpy(pEP2_IN_DataBuf, pEP3_OUT_DataBuf, 32); /* echo */
            break;
        }
    }

    DevEP2_IN_Deal(32);
}

/*********************************************************************
 * @fn      DevEP4_OUT_Deal
 *
 * @brief   �˵�4���ݴ���
 *
 * @return  none
 */
void DevEP4_OUT_Deal(uint8_t l)
{ /* �û����Զ��� */
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
 * @brief   USB2�жϺ���
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void) /* USB�жϷ������,ʹ�üĴ�����1 */
{
    USB_DevTransProcess();
}
/*********************************************************************
 * @fn      TMR3_IRQHandler
 *
 * @brief   TMR3�жϺ���
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void TMR3_IRQHandler(void) // TMR3 ��ʱ�ж�
{
    if(TMR3_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR3_ClearITFlag(TMR0_3_IT_CYC_END); // ����жϱ�־
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
 * @brief   �ļ������ú���,���ļ������·��ļ�ֵ���data flash��
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
    else{//���ⰴ��������������

    }
    uint8_t data_buf[20];
    memcpy(data_buf,key_data_buf,20);
    FLASH_DATA_VIAL_WITE_key(key_add, data_buf, Key_length);
}



