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
#include "CH58x_pwr.h"   /* LowPower_Sleep/RB_PWR_*: BLE-off pure deep-sleep */
#include "VIAL.h"
#include "NV3007.h"
#include "ui.h"
#include "bm_ui.h"         /* bare-metal UI (UI_BM_EN=1) */
#include <string.h>   /* memset for .ble_heap (NOLOAD) */
#include "lp_telemetry.h"

/* V4 Phase 0 telemetry snapshots (T6/D0); declared in lp_telemetry.h */
volatile uint32_t g_tel_power_plan;
volatile uint32_t g_tel_tmr0_ctrl;
volatile uint32_t g_tel_tmr3_ctrl;
/* ws2812.h is intentionally not included: no free pins on WeAct CH582F QFN28.
 * Keep HAL/ws2812b.c in tree for future porting to a larger package. */
/*********************************************************************
 * GLOBAL TYPEDEFS
 */
__attribute__((section(".ble_heap"), aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];   /* shared-RAM overlay (B0.5): .ble_heap NOLOAD, zeroed before BLE init */
/* Boot mode byte (EEPROM 0x3F00): 0x0B=USB, 0xBE=BLE, 0x24=2.4G.
 * Read in main() after USB_DeviceInit (��7.3); TMR3 ISR uses it to route HID. */
uint8_t g_boot_mode = 0xBE;

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
#if DCDC_ENABLE
    PWR_DCDCCfg(ENABLE);   /* V4 D3: actually enable DCDC (was a dead flag) */
#endif

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
    NV3007_Init();

    /* --- Read boot mode (EEPROM 0x3F00, after USB per S7.3) --- */
    {
        uint8_t mode = 0xBE;
        EEPROM_READ(0x3F00, &mode, 1);
        if (mode != 0x0B && mode != 0xBE && mode != 0x24) mode = 0xBE;
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
        ui_bm_init(UI_MODE_BT);             /* bare-metal UI, ��֡ BT */
        ui_set_mode(UI_MODE_BT);
        /* V4 D0: snapshot power plan + timer enable (T6) */
        g_tel_power_plan = R16_POWER_PLAN;
        g_tel_tmr0_ctrl  = R8_TMR0_CTRL_MOD;
        g_tel_tmr3_ctrl  = R8_TMR3_CTRL_MOD;        /* BLE mode: home highlights BT */
        {
            g_last_act_rtc = RTC_GetCycle32k();   /* start idle timer from boot (RTC counts in sleep) */
            uint8_t bl_on = 1;         /* backlight state (low-power idle) */
            while(1) {
                TMOS_SystemProcess();      /* BLE stack (1.25ms) */
                ui_bm_process();
                if (!bl_on) Matrix_DeepSleepConfig();   /* B0.8.9: deep-sleep keeps matrix pure input (leak-free); BLE_MODE restores cols before scan */
                /* B0.8.9 idle: no key activity beyond UI sleep secs -> backlight OFF (battery);
                 * any activity -> backlight ON.  Deep sleep handled by TMOS (HAL_SLEEP). */
                int sp = ui_get_sleep_seconds();
                uint32_t idl = RTC_GetCycle32k() - g_last_act_rtc;   /* RTC counts during deep sleep */
                uint8_t wbl = (sp > 0 && idl >= MS_TO_RTC((uint32_t)sp * 1000u)) ? 0u : 1u;   /* sp unit = seconds (test build); UI shows s */
                if (wbl != bl_on) {
                    bl_on = wbl;
                    if (!wbl) {
#if BM_LP_BLE_OFF
                        /* B0.8.10 BLE-off test: fully stop BLE then deep-sleep to
                         * measure current after BLE is off.  Stop advertising and
                         * links, then STOP calling TMOS_SystemProcess so no stack
                         * event ever fires.  A 60-min RTC keep-alive wakes us so the
                         * chip is never hard-stuck.
                         */
                        NV3007_SetBrightness(0);
                        NV3007_EnterDeepSleep();
                        PFIC_DisableIRQ(TMR0_IRQn);
                        PFIC_DisableIRQ(TMR3_IRQn);
                        PFIC_DisableIRQ(USB_IRQn);  /* BLE-off: USB unit powered down (no EXTEND), kill its IRQ to avoid spurious wake */
                        HidEmu_Shutdown();
                        /* Let the BLE stack process the stop-advertising / terminate
                         * link commands before we stop scheduling it, so those take
                         * effect (RF off) instead of remaining queued.
                         */
                        for (volatile int k = 0; k < 20; k++) TMOS_SystemProcess();
                        Matrix_DeepSleepConfig();
                        while (1) {
                            uint32_t keep = RTC_GetCycle32k() + MS_TO_RTC(60u*60u*1000u);
                            RTC_SetTignTime(keep);            /* 60-min safety wake */
                            /* Minimal RAM retention only; drop RB_PWR_EXTEND (USB/BLE
                             * units) that CH58X_LowPower keeps powered but BLE-off does
                             * not need.  Expect current near the 0.02mA transient floor. */
                            LowPower_Sleep(RB_PWR_RAM2K | RB_PWR_RAM30K);
                        }
#else
                        NV3007_SetBrightness(0); NV3007_EnterDeepSleep();
                        PFIC_DisableIRQ(TMR0_IRQn);
                        PFIC_DisableIRQ(TMR3_IRQn);
#if BM_LP_STOP_ADV
                        HidEmu_AdvEnable(0);
#endif
#if BM_LP_GPIO_WAKE
                        Matrix_SleepWakeCfg();
#endif
#endif
                    } else { NV3007_ExitDeepSleep(); NV3007_SetBrightness(255);
                        PFIC_EnableIRQ(TMR0_IRQn);
                        PFIC_EnableIRQ(TMR3_IRQn);
#if BM_LP_STOP_ADV
                        HidEmu_AdvEnable(1);
#endif
#if BM_LP_GPIO_WAKE
                        Matrix_WakeClear();
#endif
                    }
                }
            }
        }
    } else if (g_boot_mode == 0x24) {
        /* --- 2.4G RF mode (planned, not in B0.2) --- */
        SYS_ResetExecute();            /* fall back to USB for now */
    } else {
        /* --- USB mode (default): bare-metal 3-page UI (B0.8) --- */
        ui_bm_init(UI_MODE_USB);
        ui_set_mode(UI_MODE_USB);       /* USB mode: home highlights USB */
        while(1) {
            ui_bm_process();
        }
    }
}
/******************************** endfile @ main ******************************/
