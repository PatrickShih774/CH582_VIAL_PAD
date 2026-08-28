# CH582F FinPad22 低功耗 Debug 与续航优化完整方案

- **版本**：v3.1（Apple R24 规则确认修正版）
- **日期**：2026-08-28
- **状态**：待评审 / Phase 0 可启动（纯诊断，不改业务逻辑，可回退）
- **原则**：先验证再设计。所有历史电流数字均为"嫌疑值"，待 Phase 1-3 重新校准后再定标模式参数。

---

## §0 方案立场

README 把"连接态 330µA"定性为 **"iOS 拒绝 1s 间隔，软件无解的物理下限"**。代码审查证明这一结论**建立在 bug 与未捕获的遥测之上**，必须推翻并重新验证：

- 连接参数请求本身**违反 Apple R24 规则**（`1s×17=17s` 超 Apple `interval×(lat+1)≤2s` 上限；`timeout=5s` 不满足 `> interval×(lat+1)×3`；`min=max=1s` 违反 `max ≥ min+15ms` 或 `both 15ms`），iOS/协议栈拒绝的是**违规参数集**，不是"1s 太长"。
- 参数更新结果**从未被代码捕获**，"iOS 拒绝"是从电流反推的猜测。
- DCDC 在源码中**从未被显式开启**，所有电流数字待 D0 终审确认电源模式。

**因此 330µA 不是已证实的物理下限，而是"从未成功协商参数 + DCDC 状态不明 + 定时器空转"的叠加嫌疑值。** 且 v3.1 确认即使 A1 修好，iOS 受 Apple R24 规则 7（`interval×(lat+1)≤2s`）+ 规则 1（`lat≤30`）+ iOS 倾向选 min 间隔三重夹击，**连接态保活 sleep window 在 iOS 上有上限**（见 §4/§10 推导）。故"始终连接能否省过深睡"不取决于参数技巧，而取决于 **D3+D4 能把 EXTEND-on 地板压到多低** + **EXTEND 域决策⑥**。

---

## §1 现状基线（嫌疑标注）

| 测量值 | README 定性 | v3.1 定性 | 嫌疑原因 |
|---|---|---|---|
| 连接态 330µA | 物理下限、软件无解 | **未证实，待推翻** | A1 违规致协商从未成功；A3 DCDC 状态待 D0 终审；A4/A5 定时器空转 |
| 广播态 80µA | 广播+深睡平均 | 嫌疑值 | A3；A4/A5；测量条件未记 |
| 深睡 34µA | 里程碑 | 部分可信 | BLE-off 路径定时器已关，较干净；仍含 A3 |
| EXTEND-on 深睡 160µA | "保留 USB/BLE 单元" | **嫌疑伪地板** | bug 全开下测得；可能非真地板 |

> 关键：连"EXTEND-on 深睡地板 160µA"本身都是嫌疑值。连接态地板 ≈ EXTEND-on 地板 + 稀疏保活事件。若修好 bug 后 EXTEND-on 地板大幅下降，连接态才可能接近 <50µA。这是后续"始终连接 vs 深睡断连"决策的硬前提。

---

## §2 代码审查发现

### 🔴 A1｜连接参数请求违反 Apple R24 规则——iOS/协议栈必然拒绝（主因）

**证据**：`APP/BLE_MODE.c:49-58`、`:364-367`
```
DEFAULT_DESIRED_MIN_CONN_INTERVAL = 800  -> 800×1.25ms = 1000ms = 1s
DEFAULT_DESIRED_MAX_CONN_INTERVAL = 800  -> 1s
DEFAULT_DESIRED_SLAVE_LATENCY     = 16
DEFAULT_DESIRED_CONN_TIMEOUT      = 500  -> 500×10ms = 5000ms = 5s
```

**校验用 Apple Accessory Design Guidelines R24 §52.6（附录 B 完整版）**：
1. `Slave Latency ≤ 30` → 16 ≤ 30 ✅
2. `2s ≤ Supervision Timeout ≤ 6s` → 5s ∈ [2,6] ✅
3. `Interval Min ≥ 15ms` 且为 15ms 倍数 → 1000ms ≥ 15ms ✅
4. `Interval Min ≤ 2s` → 1000ms ≤ 2s ✅
5. **`Interval Max ≥ Interval Min + 15ms` 或 `Interval Max = Interval Min = 15ms`** → 1000ms ≥ 1000ms+15ms ❌，且 1000ms ≠ 15ms ❌ → **违反规则 6**
6. **`Interval Max × (Slave Latency + 1) ≤ 2s`** → 1000ms × 17 = 17s > 2s ❌ → **违反规则 7**
7. **`Supervision Timeout > Interval Max × (Slave Latency + 1) × 3`** → 5s > 51s ❌ → **违反规则 8**

**定性**：iOS/协议栈拒的不是"1s 太长"，而是 **参数集本身在 Apple R24 规则下完全不合规**。一个 `1s×1=1s≤2s` 且 `max ≥ min+15ms` 且 `timeout > 3s` 的合规请求（如 C2 修正版）理论可过。这直接证伪 README"1s 物理下限"。

另：`GAPRole_PeripheralConnParamUpdateReq` 文档化返回值含 `bleInvalidRange`，**协议栈可能本地就拒、不发包**；`BLE_MODE.c:364` 调用时**未查返回值**。

### 🔴 A2｜参数更新结果从未被捕获——"被拒"是推断非实测

**证据**：`APP/BLE_MODE.c:471` `hidEmu_ProcessTMOSMsg` 的 `default: break;` 为空。
`GAP_LINK_PARAM_UPDATE_EVENT`（`gapLinkUpdateEvent_t`，含 `status/connInterval/latency/timeout`）经 `SYS_EVENT_MSG -> hidEmu_ProcessTMOSMsg -> default` 被**静默丢弃**。PHY 更新结果同样只走空 `PRINT`。

### 🔴 A3｜DCDC 在源码中从未显式开启

**证据**（全树 `*.c/*.h/*.S/*.ld` 搜索）：
- `config.h:82` `#define DCDC_ENABLE TRUE`，但**无任何 `#if DCDC_ENABLE` 消费块**去调用 `PWR_DCDCCfg(ENABLE)`。
- `PWR_DCDCCfg` 全库**零调用点**（仅 `CH58x_pwr.c` 实现）。
- `bleConfig_t`（`CH58xBLE_LIB.h:127-157`）**无 DCDC 字段**，库不经配置接管。
- `Startup/startup_CH583.S` 无电源初始化。
- `LowPower_Sleep`（`CH58x_pwr.c:312`）只 **保持** 既有位，**从不把关->开**。
- `LowPower_Sleep` 注释明示"DCDC 会被强制关闭，唤醒后需手动重开"，但 `SLEEP.c` 唤醒后未重开。

**唯一无法从源码判定**：芯片上电复位默认值 / ROM 库内部是否写寄存器。按 WCH 官方惯例（所有例程都显式 `PWR_DCDCCfg(ENABLE)`），默认极可能为关。
**终审**：运行期读 `R16_POWER_PLAN & RB_PWR_DCDC_EN`（只读不改，1 行）+ 硬件确认 VDCID/VDCIA 外围 + ROM_CFG_ADR_HW bit13（见 D0）。

### 🟠 A4｜BLE 模式下 TMR3 空转 666 次/秒

**证据**：`USB_MODE.c:1341` `TMR3_IRQHandler`：`if (g_boot_mode != 0x0B) { return; }`--BLE 模式进 ISR 直接返回、什么都不做。但 `main()`（`hidkbd_main.c`）在 boot 判断**之前**就 `TMR3_TimerInit(90000)`(1.5ms)+使能 IRQ，且仅在背光关时才 `DisableIRQ(TMR3)`。

### 🟠 A5｜TMR0 1ms tick 在 BLE 活跃态持续运行（1000 次/秒）

**证据**：`bm_ui.c:1406` `ui_bm_init` -> `TMR0_TimerInit(60000)`(1ms)+使能；`bm_ui.c:46` `g_bm_tick_ms++`。仅背光关时禁。
`SLEEP_RTC_MIN_TIME = US_TO_RTC(1000)`=1ms 卡在边界，唤醒稳定耗 `WAKE_UP_RTC_MAX_TIME`=1.4ms > 1ms 睡眠 -> 形成"半睡半醒"抖动，深睡无法真正完成。与 TMOS 睡眠调度机制冲突（见 D4）。

### 🟡 A6｜测量条件未控制/记录

`NV3007_SetBrightness`（`NV3007.c:980`）是 GPIO 纯开关（无 PWM）；`EnterDeepSleep`（`NV3007.c:731-737`）已验证把 SCK/MOSI/DC/BL 切 `Floating` 高阻（漏电路径已消除）。但 README"330µA"未说明背光/屏/校准峰值状态。

### 🟡 A7｜校准与温度采样周期峰值

`MCU.c` `HAL_REG_INIT_EVENT` 每 `BLE_CALIBRATION_PERIOD`=120s 跑 `BLE_RegInit()`+温度采样；`cfg.tsCB=HAL_GetInterTempValue` 每~7 连接采样。

### 🟢 A8｜scanRsp 广播连接间隔范围写死 1s/1s（优先级已降级）

`BLE_MODE.c:101-107` `scanRspData` 用 `DEFAULT_DESIRED_*`=1s/1s。
**修正（基于 Apple R24 规则 10）**：Apple 官方明确 **"The device will not read or use the parameters in the Peripheral Preferred Connection Parameters (PPCP) characteristic"**。即 iOS **完全忽略** PPCP，A8 从"可能影响初始参数"降级为"纯装饰性遗留代码"。修复优先级降至 **P4 以下**。

### 🟢 A9｜PHY 更新结果未捕获

`START_PHY_UPDATE_EVT` 调 `GAPRole_UpdatePHY` 只走空 `PRINT`，未记录实际 PHY。

---

## §3 遥测缺口（T2 改 RAM 快照）

| ID | 遥测点 | 位置 | 输出方式 |
|---|---|---|---|
| T1 | `GAPRole_PeripheralConnParamUpdateReq` **返回值** | `BLE_MODE.c:364` | RAM 快照 |
| T2 | `GAP_LINK_PARAM_UPDATE_EVENT`（status/interval/latency/timeout 4 字段） | `BLE_MODE.c:471` default | **RAM 保留 struct，事后 debugger/UART 读**（电流测量期零开销） |
| T3 | 建连初始参数 | `BLE_MODE.c` GAPROLE_CONNECTED | RAM 快照 |
| T5 | 睡眠计数/累计 ms | `SLEEP.c` `CH58X_LowPower` | RAM 计数（GPIO 翻转仅留 T5 睡/醒边沿示波器用途） |
| T6 | DCDC/定时器状态 | 运行期读 `R16_POWER_PLAN`、TMR0/3 使能位 | RAM 快照 |

> `DEBUG` 未定义（`.cproject` 仅 `-DHAL_SLEEP=1`，`PRINT` 为空宏）。遥测统一 RAM 快照；GPIO 翻转仅用于睡/醒边沿示波器。临时开 `DEBUG` 走 UART 仅用于测电流（README §7.11：DEBUG 会致 Vial 通信失败）。

---

## §4 参数候选表（Apple R24 §52.6 规则重写）

> 间隔单位 1.25ms，超时单位 10ms。所有请求均已过 Apple R24 §52.6 完整规则校验（含 R6 `max ≥ min+15ms` 或 `both 15ms`，以及 R8 `timeout > interval×(lat+1)×3`）。

| 候选 | min-max(单位) | interval | lat | timeout | Apple 校验 | 用途/预期 |
|---|---|---|---|---|---|---|
| C-base(现) | 800-800 | 1s | 16 | 5s(500) | R6 1000≠15 且 1000≱1015 ✗; R7 17s>2s ✗; R8 51s>5s ✗ | 证明现状多重违规 |
| **C1′** | 12-24 (15-30ms) | iOS 选 15ms | 29 | 6s(600) | R6 30≥30(踩线) ✓; R7 30×30=900ms≤2s ✓; R8 2.7s<6s ✓ | A 组：iOS 选 min -> sleep window **450ms**，延迟优、不省电 |
| **C1′-safe** | 12-29 (15-36.25ms) | iOS 选 15ms | 29 | 6s(600) | R6 36.25≥30(余量 6.25ms) ✓; 其余同 C1′ | A 组安全版，规避 R6 踩线风险 |
| **C2** | 800-812 (1000-1015ms) | ~1s | 0 | 6s(600) | R6 1015≥1015(踩线) ✓; R7 1015ms≤2s ✓; R8 3.045s<6s ✓ | B 组：iOS 是否**接受**强制长间隔 -> window ~1s |
| **C2-safe** | 640-652 (800-815ms) | ~800ms | 0 | 6s(600) | R6 815≥815(踩线) ✓; R7 815ms≤2s ✓; R8 2.445s<6s ✓ | B 组保守版 |
| **C2+** | 1584-1596 (1980-1995ms) | ~1.98s | 0 | 6s(600) | R6 1995≥1995(踩线) ✓; R7 1995ms≤2s ✓; R8 5.985s<6s(余量 15ms) | B 组上限：若 iOS 接受强制长间隔，window 可近 2s |
| **C2+-safe** | 1200-1212 (1500-1515ms) | ~1.5s | 0 | 6s(600) | R6 1515≥1515(踩线) ✓; R7 1515ms≤2s ✓; R8 4.545s<6s(余量 1.5s) | B 组安全版，余量充足 |
| C1′max | 12-24 | iOS 选 15ms | 30 | 6s(600) | R7 450ms≤2s ✓; R8 1.35s<6s ✓ | 高延迟极限：window 465ms |

**关键推导（决定 §6⑤ 与 §10）**：
- iOS 倾向选 min 间隔 -> 带 15ms 的请求实际协商成 **15ms**。
- 受 Apple R7(`≤2s`)+R1(`lat≤30`) -> **高延迟路线 window 上限 = 15ms×31 = 465ms**。
- 而**强制长间隔路线**（min=max=长，C2/C2+）若 iOS 接受 -> window 可达 **~2s**（R7 上限）。
- **故 iOS 上省电靠"强制长间隔（B 组）"，不靠"高延迟（A 组）"。A 组价值在延迟不在电流。**
- 但"iOS 是否接受强制长间隔"**未验证**（README 证据是违规的 C-base）。D2 的 B 组是胜负手。
- 短间隔+高延迟 = 低输入延迟(15ms)+中等省电(450ms)；强制长间隔+低延迟 = 最佳省电(~2s)+高输入延迟(~2s)。二者不可在单一静态参数下兼得 -> 需动态切换（§10）。

> **R9 补充**：Apple R24 明确 "If BLE HID is a connected service, a connection interval down to 11.25 ms may be accepted"。M1 竞技模式可利用此规则，但 Apple 同时警告部分设备会将 15ms 上调到 30ms，需以 T2 实测为准。

---

## §5 验证实验序列

### D0（只读终审 + 硬件前置，无业务改动）-- 第一优先
1. 读 `R16_POWER_PLAN & RB_PWR_DCDC_EN`（A3 终审）。
2. **硬件确认 VDCID(Pin1)/VDCIA(Pin22) 外接电感+电容是否在板**（D3 硬前提）。
3. **读 `ROM_CFG_ADR_HW` bit13**：若为 1，DCDC 被强制旁路（D3 即使调 `PWR_DCDCCfg` 也不生效）。
4. 读 TMR0/3 使能位（A4/A5 终审）。

### D1（仪表化，不改参数）
补 T1/T2/T3/T5（RAM 快照）。连 iOS，读真实协商值。
- 返回 `bleInvalidRange` -> **证实 A1：请求本地就被拒，iOS 从未收到**。
- 返回 `SUCCESS` 但 event status=拒 -> 看 iOS 实际给值。

### D2（A+B 组）
- A 组：发 **C1′-safe**（15-36.25ms, lat29, 6s）-> 抓 iOS 实际选的 interval（预期 15ms，window 450ms）。
- B 组：发 **C2-safe**（800ms, lat0, 6s）-> **iOS 是否接受强制长间隔**（若接受 window ~800ms；若拒看 iOS 给什么）。再试 **C2**（~1s）和 **C2+-safe**（~1.5s）探上限。

### D3（DCDC）-- 前置已由 D0 确认
`main()` 加 `PWR_DCDCCfg(ENABLE)` + `CH58X_LowPower` 唤醒后重开。重测连接/广播/深睡。

### D4（TMR0 迁移，非门控）
1. `ui_bm_init` 不再 `TMR0_TimerInit`；UI tick 改 **TMOS event 循环**（`tmos_start_task` 自重排），TMR0 彻底关。
2. BLE 模式从 `main()` 起就**不 init TMR3**。
3. 用 T5 睡眠计数确认 TMOS 空闲确能进深睡。

### D5｜E6 分离背光/屏。### D6｜B 组探完上限后重新定模式。

### 重新测量矩阵（控制变量）

基线：外部 32K LSE、HAL_SLEEP=1、iOS HID 主机、万用表 µA 档（平均+峰值）。

| # | 连接 | 参数请求 | DCDC | TMR0/3 | 屏/背光 | 测量目标 |
|---|---|---|---|---|---|---|
| E0 | 否(广播) | - | 关(现状) | 运行 | 关 | 重测广播基线，对比 80µA |
| E1 | 是(iOS) | C-base(违规) | 关 | 运行 | 关 | 重测"330µA"，记录 T2 实际值 |
| E2 | 是(iOS) | C1′-safe 合规 | 关 | 运行 | 关 | **A 组：iOS 是否接受 Latency** |
| E3 | 是(iOS) | C-base | **开启** | 运行 | 关 | 量化 DCDC 收益 |
| E4 | 是(iOS) | C-base | 关 | **禁 TMR3** | 关 | 量化 TMR3 空转 |
| E5 | 是(iOS) | C-base | 关 | 禁 TMR3+**门控 TMR0** | 关 | 量化 TMR0 |
| E6 | 是(iOS) | C-base | 关 | 禁 TMR0/3 | **开** | 分离背光/屏电流 |
| E7 | 否(深睡) | - | 开/关 | 关 | 关 | 重测深睡地板，含/不含 DCDC、EXTEND-on 地板 |

---

## §6 战略决策

**① 推翻"无解"定性**：是。A1+A2+A3 是硬事实，"无解"建立在 bug 之上。

**② 实验优先级**：同意 A 组>B 组，但**先插 D0+D1 仪表化**（A/B 在无 T2 时是盲测）。序列：D0->D1->D2(A 组)->D6(B 组)。

**③ 动态 vs 静态**：**需动态切换**。v2 的"静态优先、Latency 自带动态"在 Apple 规则下不成立--短间隔+高延迟仅 450ms window（不省电），最佳省电需强制长间隔（高输入延迟），二者不可兼得 -> 键盘需打字短间隔、空闲长间隔的动态切换（§10）。状态机结构可先实现，参数待校准。

**④ iOS 专一性**：按多平台、以 iOS 为最严基准、不做运行期平台探测。通过 iOS 的参数集在 Android/Windows 必通过。仅当验证发现 iOS 完全拒 Latency 而 Android 能靠长间隔大幅省电时才考虑平台特化。

**⑤ 深睡取舍（重写）**：
- v2 误把"始终连接 ~35-45µA"归功于 C1 省电。**v3.1 修正**：iOS 上 A 组 window 仅 450-465ms、且不省电；**始终连接能否省过深睡，取决于 D3+D4 把 EXTEND-on 地板压到多低 + EXTEND 决策⑥，与 C1 无关**。
- 若 D3+D4 后 EXTEND-on 地板降到 ~15-30µA -> 始终连接（B 组 1-2s window）≈ 地板+稀疏保活 -> 可与 BLE-off 34µA 竞争，且无重连/漏键 -> 选始终连接。
- 若 EXTEND-on 地板仍 ~160µA -> 始终连接 ~160µA+ -> 远劣于 BLE-off -> 两段式。

**⑥ EXTEND 域是否必须保留（新增独立决策）**：
- EXTEND = USB+BLE 单元供电。**连接态必开**（BLE 保活需 BLE 单元）。
- 该决策只作用于 **BLE-off 深睡态**：USB 域是否要在深睡时保留？
  - 若 Vial 配置只做一次、之后拔线纯 BLE -> 深睡可 EXTEND 全关 -> **BLE-off 地板 34µA**（已实测）。
  - 若 Vial 必须随时可插 -> 深睡须留 USB 域 -> EXTEND on -> **BLE-off 地板 ~160µA**。
- 这决定两段式 idle 的地板（34µA vs 160µA），是"始终连接 vs 两段式"分母的根。**Phase 4 前必须定，不混入"地板能否降"。**

---

## §7 收敛路径（决策树）

```
D0 读DCDC位+VDCID/VDCIA硬件+ROM bit13 ; D1 仪表化(T1/T2/T3/T5 RAM)
        │ 读真实协商值 ─ 证伪 A1
        │
   D2 A组(C1′-safe) ─ iOS 选 15ms -> window 450ms（延迟优/不省电，证 #2）
   D2 B组(C2-safe/C2/C2+-safe) ─ iOS 是否接受强制长间隔？
        │
   ┌────┴───────────────────┐
   │ iOS 接受长间隔           │ iOS 强制 15ms
   │ window 可达 ~2s         │ window 上限 465ms
   │                         │
   D3 DCDC + D4 TMR0迁移      D3 DCDC + D4 TMR0迁移
   E7 重测 EXTEND-on 地板     E7 重测 EXTEND-on 地板
        │                         │
   决策⑥ EXTEND 是否必须保留
   ├ ⑥否(深睡可关)->两段式idle=34µA ; 始终连接需地板<34µA才赢
   └ ⑥是(须留USB)->两段式idle=160µA; 始终连接地板<160µA即赢->倾向始终连接
```

---

## §8 优先级与风险

| 优先级 | 项 |
|---|---|
| **P0** | A1+A2+A3 + D0(含硬件/ROM位) + D1 |
| **P1** | D3 DCDC（硬前提已由 D0 锁定） |
| **P2** | D4 TMR0 迁移 TMOS（非门控，避免与 TMOS 睡眠冲突） |
| **P3** | D5 测量方法论 |
| **P4** | D6 B 组上限 + A7/A9 |
| **P5** | A8（PPCP 无效，修复优先级已降级） |

**风险**：
- D3 启 DCDC 后 `LowPower_Sleep` 每次唤醒需 `PWR_DCDCCfg(ENABLE)` 重开，否则睡一次回 LDO。
- D4 迁 TMR0 后 UI 节拍改 TMOS/RTC 驱动，不能简单长关。
- D1 需临时开 `DEBUG`（UART），仅测电流期，量产必须关。

---

## §9 落地步骤

| Phase | 动作 | 修正点 |
|---|---|---|
| 0 | D0 读 `R16_POWER_PLAN`+**VDCID/VDCIA 硬件+ROM_CFG_ADR_HW bit13** + D1 加 T1/T2/T3/T5（**RAM 快照**） | 新增硬件/ROM 位；遥测改 RAM |
| 1 | D2 用 **C1′-safe(15-36.25ms,lat29,6s)** + B 组 **C2-safe(800ms,lat0,6s)/C2(~1s)/C2+-safe(~1.5s)**，抓 iOS 实际选值 | 候选按 Apple R24 完整规则；B 组定胜负 |
| 2 | D3 启 DCDC（**先确认电感在板+ROM 位**）+ D4 TMR0 迁 TMOS、TMR3 不再 init | TMR0 门控->迁移 |
| 3 | E7 重测 EXTEND-on 地板 + **决策⑥ EXTEND 是否必须** | 新增 EXTEND 决策点 |
| 4 | 据 Phase1-3 真实协商+地板，校准 §10 模式参数与阈值后定标 | - |

---

## §10 四档模式与状态机（初步设计，待 Phase 1-3 校准）

> **设计基线**：所有模式参数预先通过 Apple R24 §52.6 规则校验。iOS 会选请求的 min 间隔，故"实际间隔 = 请求 min"。在 R6(`max≥min+15`)+R7(`max×(lat+1)≤2s`)+R1(`lat≤30`) 三重夹击下：
> - **短间隔+高延迟**（如 15ms+lat30）= 低输入延迟(15ms)+中等省电(window 450ms)；
> - **强制长间隔+低延迟**（如 ~2s+lat0）= 最佳省电(window ~2s)+高输入延迟(~2s)。
> - **二者不可在单一静态参数下兼得** -> 键盘需**动态切换**：打字时短间隔、空闲时长间隔。这正是四档状态机存在的理由。

### 10.1 四档模式定义

间隔单位 1.25ms，超时单位 10ms。所有请求均已过 Apple R24 规则校验。

| 模式 | 请求 min-max(lat,timeout) | iOS 实选 | sleep window | 输入延迟 | TX 功率 | 屏/背光 | 扫描节拍 | 目标电流(校准目标) |
|---|---|---|---|---|---|---|---|---|
| **M1 竞速** | 9-21 (11.25-26.25ms, lat0, 2s) | 11.25ms 或 15ms | 11.25ms | 11.25ms | 0dBm | ON | 挂连接事件 | ~300-500µA |
| **M2 办公** | 12-27 (15-33.75ms, lat29, 6s) | 15ms | 450ms | 15ms | 0/-3dBm | ON(可dim) | 挂连接事件 | ~50-100µA* |
| **M3 空闲** | 36-48 (45-60ms, lat30, 6s) | 45ms | 1395ms | 45ms | -6/-8dBm | OFF+面板深睡 | ~1-2s | ~5-30µA* |
| **M4 极限** | 1584-1596 (~1980ms, lat0, 6s) | ~1.98s | ~1980ms | ~2s | -12dBm | OFF+面板深睡 | ~5s | <20µA* |

`*` = 依赖 D3(DCDC)+D4(定时器) 把 EXTEND-on 地板压低后的校准目标，非保证。

**M1 竞速模式说明**：基于 Apple R24 规则 9（HID 设备可接受 11.25ms 间隔）。但 Apple 同时警告部分设备会将 15ms 请求上调到 30ms，11.25ms 请求也可能被上调。M1 实际间隔以 T2 实测为准，若 iOS 强制 15ms，则退化为 15ms。

**模式间 window 单调递增**：11.25ms(M1) -> 450ms(M2, 40×) -> 1395ms(M3, 3×) -> 1980ms(M4, 1.4×)。
> 注意 M3->M4 window 增益小(1.4×)但延迟代价大(45ms->2s)，**M4 的价值在于"接近 2s 连接态上限"**；若 EXTEND-on 地板压不到 ~15µA，M4 连接态达不到 <20µA，此时 M4 应回退为 **M4′ BLE-off 断连深睡(34µA)**（见 10.3）。

### 10.2 状态机与切换逻辑

```
                 任意按键 ─────────────────────► M1 竞速(11.25/15ms,lat0)
                    ▲                               │
                    │                             3s 无按键
            (有键即升,目标=M1)                       ▼
                    │                            M2 办公(15ms,lat29)
                    ▲                               │
                    │                          UI休眠时长(10/30/60s)
                    │                               ▼
                    │                            M3 空闲(45ms,lat30,屏关)
                    ▲                               │
                    │                          60-120s 无按键
                    │                               ▼
                    │            ┌─ D2 B组通过(iOS接受长间隔) ─► M4 极限(~2s,lat0)
                    │            │
                    │            └─ D2 B组失败(iOS强制15ms) ─► M4′ BLE-off断连深睡(34µA,受⑥门控)
```

**计时基准**：复用 `g_last_act_rtc`（`scan_key.c`，RTC 32k，深睡仍走），不依赖 `g_bm_tick_ms`（深睡停走）。

**上升沿（活动）**：
- 任意键 -> 设"目标=M1"；**仅当当前协商模式≠M1 时**才发一次 `GAPRole_PeripheralConnParamUpdateReq`。连续打字不会重复请求（已在 M1）。
- M3/M4 被唤醒后首键延迟 = 该模式间隔（M3: 45ms，M4′断连需重连），可接受（空闲态非打字）。

**下降沿（空闲）**：
- M1->M2：3s 无键。
- M2->M3：UI 休眠设置时长（10/30/60s）。
- M3->M4/M4′：60-120s 无键。
- 每档**至少驻留 N 秒**才允许继续降（M2->M3 ≥ UI 休眠时长；M3->M4 ≥ 30s），避免抖动。

**请求节流（防 iOS 封禁）**：
- 两次 param update 间隔 ≥ **30s**（Apple"勿频繁请求"准则）；30s 内档位变化先记 pending，到点合并发一次。
- 每次切换调用 `LL_SetTxPowerLevel()` 调 TX、`NV3007_EnterDeepSleep/SetBrightness` 调屏、调整 `START_REPORT_EVT` 重排周期。
- **以 T2 抓到的 `GAP_LINK_PARAM_UPDATE_EVENT` 实际值为"当前协商模式"**，而非请求值。

### 10.3 校准钩子（Phase 1-3 结果如何改写本节）

| 验证结果 | 对 §10 的修正 |
|---|---|
| D2 A 组：iOS 选 15ms+接受 lat | M2 参数确认；window 450ms 落实 |
| D2 B 组通过（iOS 接受强制长间隔） | M3/M4 用长间隔路线；M4 连接态 ~2s window 成立 |
| D2 B 组失败（iOS 强制 15ms） | M3/M4 无法拉长间隔 -> **M4 回退为 M4′ BLE-off 断连(34µA)**；M3 留 lat30(45ms,window 1395ms) 作连接态下限 |
| D3+D4 后 EXTEND-on 地板 ~15µA | M4 连接态可达 <20µA -> 用 M4(连接)；BLE-off 仅作"度假模式"可选 |
| D3+D4 后 EXTEND-on 地板仍 ~160µA | M4 连接态 ~160µA 无意义 -> M3 后直接跳 M4′ BLE-off(34µA) 两段式 |
| 决策⑥：深睡可关 EXTEND | M4′ 地板 34µA（两段式 idle 优） |
| 决策⑥：深睡须留 EXTEND(USB) | M4′ 地板 160µA -> 反而倾向始终连接 M4 |

**阈值与精确参数**（3s/15s/120s、各档 interval/lat 具体值）标为**初值**，待 Phase 3 拿到真实地板与 iOS 协商能力后定标。

### 10.4 与决策⑤⑥的衔接

- **M1-M4 均为"始终连接"形态**（维持 BLE 连接、EXTEND on）。其最低电流 = EXTEND-on 地板 + 稀疏保活，由 D3+D4 决定，**与 C1 高延迟无关**（落实 §6⑤ 修正）。
- **M4′（BLE-off 断连）是比 M4 更深的一档**，仅当：iOS 拒长间隔 **或** EXTEND-on 地板压不下去 **且** 决策⑥允许关 EXTEND 时启用。代价是重连延迟/漏键，故仅长时空闲触发。
- 即：状态机的"极限档"在 M4(连接~2s) 与 M4′(断连34µA) 之间**二选一**，由 Phase 1-3 实测裁决，现在不预设。

---

## 附录 A：关键代码位置速查

| 项 | 文件:行 |
|---|---|
| 连接参数请求 | `APP/BLE_MODE.c:49-58`(宏), `:364-367`(调用) |
| 参数更新事件空处理 | `APP/BLE_MODE.c:471`(hidEmu_ProcessTMOSMsg default) |
| DCDC 宏(死标志) | `HAL/include/config.h:81-82` |
| 睡眠回调 | `HAL/SLEEP.c` CH58X_LowPower, `HAL/MCU.c:124` 注册 |
| LowPower_Sleep DCDC 处理 | `StdPeriphDriver/CH58x_pwr.c:312` |
| TMR3 空转 ISR | `APP/USB_MODE.c:1341` |
| TMR3/TMR0 init | `APP/hidkbd_main.c`(TMR3), `HAL/bm_ui.c:1406`(TMR0) |
| TMR0 tick | `HAL/bm_ui.c:43-46` |
| 屏深睡/背光 | `HAL/NV3007.c:725`(EnterDeepSleep), `:980`(SetBrightness) |
| 校准任务 | `HAL/MCU.c` HAL_REG_INIT_EVENT |
| BLE 主循环 | `APP/hidkbd_main.c` BLE mode while(1) |
| 构建宏 | `.cproject:72`(`-DHAL_SLEEP=1`，无 DEBUG/DCDC) |
| 参数更新/PHY API | `LIB/CH58xBLE_LIB.h:4393`(PeripheralConnParamUpdateReq), `:4235`(UpdatePHY), `:2865`(LL_SetTxPowerLevel) |
| 参数更新事件结构 | `LIB/CH58xBLE_LIB.h` gapLinkUpdateEvent_t |

## 附录 B：Apple BLE 连接参数规则（R24 §52.6 完整版）

> 来源：Apple *Accessory Design Guidelines for Apple Devices* Release R24 (2024-10-21), §52.6 Connection Parameters
> 注：规则原文使用 "Peripheral Latency"，与 BLE 规范 "Slave Latency" 同义，下文统一用 **Slave Latency**。

### B.1 通用规则（所有 BLE 外设）

连接参数请求可能被拒绝，如不符合以下**全部**规则：

| 编号 | 规则 |
|------|------|
| **R1** | Slave Latency **≤ 30** |
| **R2** | Supervision Timeout **from 2 seconds to 6 seconds** |
| **R3** | Interval Min **≥ 15 ms** |
| **R4** | Interval Min **is a multiple of 15 ms** |
| **R5** | Interval Min **≤ 2 seconds** |
| **R6** | One of the following: <br>• Interval Max **at least 15 ms greater than** Interval Min <br>• **Interval Max and Interval Min both set to 15 ms** |
| **R7** | Interval Max × (Slave Latency + 1) **≤ 2 seconds** |
| **R8** | Supervision Timeout **greater than** Interval Max × (Slave Latency + 1) × 3 |

### B.2 HID 设备特殊规则

| 编号 | 规则 |
|------|------|
| **R9** | If BLE HID is a connected service, a connection interval **down to 11.25 ms** may be accepted |
| **R10** | The device **will not read or use** the parameters in the **Peripheral Preferred Connection Parameters (PPCP)** characteristic |

### B.3 对当前项目的影响

- **R9**：M1 竞技模式可请求 11.25ms 间隔，但部分 iOS 设备可能将其上调到 15ms 或 30ms，需以 T2 实测为准。
- **R10**：`scanRspData` 中硬编码的连接间隔范围对 iOS **完全无效**。A8 修复优先级降至 P5 以下。
- **R6**：`min=max` 仅在 **15ms** 时允许，其他任何值必须满足 `max ≥ min + 15ms`。这是 C-base（1s/1s）和 C2 原始版（800-800）被拒绝的根本原因。
