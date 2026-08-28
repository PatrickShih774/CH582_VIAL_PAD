#ifndef LP_TELEMETRY_H
#define LP_TELEMETRY_H

#include <stdint.h>

/*
 * Low-power telemetry (V4 Phase 0 / D0 + D1).
 *
 * These are RAM snapshots only.  Read them with a debugger (or a later UART
 * dump) while the current meter runs; no PRINT/UART traffic is generated here
 * so the measured current is not disturbed.
 */

/* T1: GAPRole_PeripheralConnParamUpdateReq return value (bStatus_t). */
extern volatile uint32_t g_tel_req_ret;

/* T2: GAP_LINK_PARAM_UPDATE_EVENT fields. */
extern volatile uint32_t g_tel_upd_status;
extern volatile uint32_t g_tel_upd_interval;
extern volatile uint32_t g_tel_upd_latency;
extern volatile uint32_t g_tel_upd_timeout;
extern volatile uint32_t g_tel_upd_evt_cnt;
extern volatile uint32_t g_tel_last_evt;
extern volatile uint32_t g_tel_state;
extern volatile uint32_t g_tel_opcode;

/* T3: initial connection parameters (GAP_LINK_ESTABLISHED_EVENT). */
extern volatile uint32_t g_tel_conn_interval;
extern volatile uint32_t g_tel_conn_latency;
extern volatile uint32_t g_tel_conn_timeout;

/* T5: sleep statistics (CH58X_LowPower). */
extern volatile uint32_t g_tel_sleep_cnt;
extern volatile uint32_t g_tel_sleep_ms;
extern volatile uint32_t g_tel_nosleep_cnt;

/* T6/D0: power plan / timer enable snapshot. */
extern volatile uint32_t g_tel_power_plan;
extern volatile uint32_t g_tel_tmr0_ctrl;
extern volatile uint32_t g_tel_tmr3_ctrl;

#endif /* LP_TELEMETRY_H */
