# CH582_VIAL_PAD — 财务专用三模数字小键盘

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

基于 WCH CH582F 的财务专用数字小键盘固件，支持三模（USB 有线 / 蓝牙 BLE / 2.4G 无线），使用 VIAL 进行键值配置。工程由 MounRiver Studio 生成与管理。

- **参考工程**：[基于CH582M的三模兼容VIAL改键小键盘](https://oshwhub.com/bluetooth-keyboard-squad/the-first-stop-of-the-three-mode-keyboard)
- **目标芯片**：CH582F（CH582/CH583 系列，SFR 与 startup 共用 CH583 资源）
- **开发环境**：MounRiver Studio（RISC-V GCC 工具链，`riscv-none-embed-`）
- **当前版本**：`B0.8.8`（2026-08-12，v0.5 之后）— NV3007 142×428 彩屏 + **裸机 UI**（无 LVGL，六主题三页面，凤凰点阵体全字库 + 自定义文本 + 三模 UI 同步）+ USB/BLE 三模切换（复位式）+ Vial 改键 + **刷新/初始化/局部刷新优化**（SPI 快路径 + 初始化参数回退 B0.8.7 验证值，详见 §8.14 B0.8.8 / §8.16 / §9）
- **屏幕 UI**：**裸机 `bm_ui`**（`HAL/bm_ui.c` + `HAL/bm_font.c`），设计规范 `C:\ClaudeProject\tft_NV3007\brand-spec.md`；六主题（像素/极简/黑客 × 双色）、三页面、共享 8×8 数字 + 5×7 拉丁字体

<p align="center">
  <img src="Reference\FinPad22.png" alt="CH582 VIAL PAD 预览" width="600"/>
</p>

## 📌 当前状态（2026-08-11）

| 项 | 状态 |
|----|------|
| 版本 | **B0.8.8**（SPI 快路径 + 局部刷新 + 初始化参数回退 B0.8.7 验证值） |
| 屏幕 | NV3007 142×428（2.79" T279VJ-C10-01），横向 428×142，SPI bit-bang，驱动 `HAL/NV3007.c/h` |
| 三模 | USB ✅ / BLE ✅（复位切换，非热切换）；2.4G ⚠️ 占位（`0x24` 当前复位回 USB） |
| UI | 裸机 `bm_ui`：六主题（默认极简·浅）三页面，USB/BLE 同享；无 LVGL → 释放 `.lvgl_shared` 19.3KB / BLE 尾部 7.9KB |
| 已知问题 | 上电后“顶部到中部淡色带”/需手动复位：根因 RST 共用 MCU 复位网络（VDD 未稳 RST 先高，GOA 闩锁错误）；推荐面板 RST 单独 RC（10K+10µF），固件保持 B0.8.7 验证参数（等待 400ms + init 重试）兜底 |
| 下一步 | 裸机 UI 真机验收（翻页/计算器/六主题）→ 2.4G RF 接入 |

## 📖 目录

- [一、硬件引脚分配](#sec1)
- [二、移植完成情况](#sec2)
- [三、构建与烧录](#sec3)
- [四、三模切换逻辑](#sec4)
- [五、功能与计划（矩阵 / 屏幕 UI / 上位机）](#sec5)
- [六、参考资源](#sec6)
- [七、调试记录（历史日志，已折叠）](#sec7)
- [八、LVGL 项目计划书与实施记录](#sec8)
- [九、版本记录（可回退点）](#sec9)
- [十、蓝牙 BLE 模式开发计划书](#sec10)

---

<h2 id="sec1">一、硬件引脚分配（WeAct CH582F CoreBoard）</h2>

### 1.1 可用引脚总览

WeAct CH582F CoreBoard 引出 **20 个 GPIO**（不含电源/地）：

```text
PA4  PA5  PA8  PA9  PA10  PA11  PA12  PA13  PA14  PA15   ← 10 个
PB4  PB7  PB10 PB11 PB12  PB13  PB14  PB15  PB22  PB23   ← 10 个
```

### 1.2 固定占用（不可动）

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA10 | 32K_XI | 外部 32K 晶振（WeAct 板已焊，BLE 休眠用） |
| PA11 | 32K_XO | 外部 32K 晶振 |
| PB10 | USB_D- | USB 差分信号（固定功能） |
| PB11 | USB_D+ | USB 差分信号（固定功能） |
| PB22 | BOOT | ISP 选择（低电平复位进入 WCH ISP 烧录） |
| PB23 | RST | 复位（与 NV3007 共享复位信号） |

> 以上 8 个引脚不可重新分配。

### 1.3 自由分配（14 pin → 矩阵 10 + NV3007 4）

> 引脚分配以 PCB 布线便利性为导向，Row 按 PA4→PA5→PA15→PA14→PA13→PA12 连续排列，Col 按 PB12→PB13→PB14→PB15 连续排列，NV3007 SCK/MOSI 放相邻 PA9/PA8。

```text
                ┌──────────┬──────────────────────┬──────────┐
                │ 引脚     │ 功能                 │ 信号方向 │
┌───────────────┼──────────┼──────────────────────┼──────────┤
│ 矩阵 Row (6)  │ PA4      │ row_0                │ 输出     │
│  全在 GPIOA   │ PA5      │ row_1                │ 输出     │
│               │ PA15     │ row_2                │ 输出     │
│               │ PA14     │ row_3                │ 输出     │
│               │ PA13     │ row_4                │ 输出     │
│               │ PA12     │ row_5                │ 输出     │
├───────────────┼──────────┼──────────────────────┼──────────┤
│ 矩阵 Col (4)  │ PB12     │ col_0                │ 输入上拉 │
│  全在 GPIOB   │ PB13     │ col_1                │ 输入上拉 │
│               │ PB14     │ col_2                │ 输入上拉 │
│               │ PB15     │ col_3                │ 输入上拉 │
├───────────────┼──────────┼──────────────────────┼──────────┤
│ NV3007 SPI    │ PA9      │ SCK (软件模拟时钟)    │ 输出     │
│   (bit-bang)  │ PA8      │ MOSI (软件模拟数据)   │ 输出     │
│               │ PB7      │ DC  (数据/命令选择)   │ 输出     │
│               │ PB4      │ BL  (背光 PWM10 调光) │ 输出     │
├───────────────┼──────────┼──────────────────────┼──────────┤
│ NV3007 控制   │ PB23     │ RST (与MCU共享复位)   │          │
│               │ GND      │ CS  (唯一SPI设备常低) │          │
└───────────────┴──────────┴──────────────────────┴──────────┘
```

### 1.4 NV3007 接线说明

| 屏幕信号 | 连接 | 说明 |
|----------|------|------|
| SCK | PA9 | GPIO bit-bang 模拟 SPI 时钟 |
| SDA/MOSI | PA8 | GPIO bit-bang 模拟 SPI 数据（与 SCK 相邻，方便走线） |
| DC | PB7 | 数据/命令选择（GPIO 推挽输出） |
| BL | PB4 | 背光控制，映射 TMR2 PWM10 通道实现调光 |
| CS | GND | 唯一 SPI 设备，直接常低 |
| RST | PB23 | 与 CH582 共享上电复位（共用 10K 上拉 + 100nF 对地） |

NV3007 模式配置脚（沿用原 ST7789 方案）：**IM[2:0] = 010** → 4 线 SPI（有独立 DC，无需 3 线 9-bit 模式）。

### 1.5 未使用 / 预留

| 引脚 | 状态 | 说明 |
|------|------|------|
| PB8 | 未引出 | WeAct 板 QFN28 未 bond，不存在 |
| PB9 | 未引出 | 同上 |
| WS2812 灯带 | 已砍 | 无可用引脚，为功耗考虑移除灯效 |

### 1.6 设计注意事项

1. **烧录方式**：PB14/PB15 被矩阵列占用，**无法使用 SWD 调试/烧录**。程序通过 **USB ISP** 烧录：按住 PB22(BOOT) 重新上电 → WCHISPTool 下载。
2. **PB14/PB15 作列输入**：配置为内部上拉输入，默认高电平，不会误触发 SWD 模式。但 PCB 上严禁外接强下拉电阻或对地电容（矩阵按键到 row 的通路本身不构成下拉，安全）。
3. **SPI 性能**：PA9/PA8/PB7 不在同一硬件 SPI 控制器上，使用 **GPIO bit-bang** 驱动屏幕（原 ST7789 方案）。2.25" 76×284 屏刷新足够（约 10~20fps），无需硬件 SPI。
4. **Row 全在 GPIOA**：PA4~PA5~PA15~PA14~PA13~PA12 连通，一次 `R32_PA_OUT` 端口操作即可设所有 row 电平，扫描效率最高。
5. **Col 全在 GPIOB**：PB12~PB13~PB14~PB15 连续排列，一次 `R32_PB_PIN` 读端口即可取所有 col 状态，布线最短。

---

<h2 id="sec2">二、移植完成情况（2026-07-28）</h2>

已将参考工程 `CH582_VIAL_KBD` 的三模键盘代码完整移植到本工程，保持标准 MounRiver Studio 工程结构（真实文件夹，非 Eclipse 链接资源）。

### 2.1 目录结构
<p align="center">
  <img src="Reference\CH582_VIAL_PAD工程架构图.png" alt="CH582_VIAL_PAD工程架构图" />
</p>

```
CH582_VIAL_PAD/
├── .project                          # Eclipse 工程描述
├── .cproject                         # 构建配置：工具链 / Include 路径 / 宏 / 库 / 源路径
├── .template                         # MounRiver 模板标记
├── .mrs/
│   └── CH582_VIAL_PAD.mrs-workspace  # MounRiver 工作区
├── CH582_VIAL_PAD.wvproj             # MounRiver 工程文件
├── CH582_VIAL_PAD.launch             # OpenOCD 调试启动配置
├── README.md
│
├── Ld/
│   └── Link.ld                       # 链接脚本（FLASH 448K / RAM 32K）
├── Startup/
│   └── startup_CH583.S               # 启动代码、中断向量表、复位处理
├── RVMSIS/
│   ├── core_riscv.c
│   └── core_riscv.h                  # RISC-V 内核接口（中断 / CSR / 原子操作）
│
├── StdPeriphDriver/                  # WCH 片上外设驱动（SDK，已与 KBD 对齐）
│   ├── libISP583.a                   # ISP 在线烧录库（链接 -lISP583）
│   ├── CH58x_clk.c                   # 时钟
│   ├── CH58x_gpio.c                  # GPIO
│   ├── CH58x_pwr.c                   # 电源管理
│   ├── CH58x_sys.c                   # 系统
│   ├── CH58x_flash.c                 # Flash 读写
│   ├── CH58x_uart0.c ~ uart3.c       # UART0~3
│   ├── CH58x_spi0.c / spi1.c         # SPI
│   ├── CH58x_i2c.c                   # I2C
│   ├── CH58x_pwm.c                   # PWM
│   ├── CH58x_adc.c                   # ADC
│   ├── CH58x_timer0.c ~ timer3.c     # 定时器（timer2→WS2812，timer3→USB）
│   ├── CH58x_usbdev.c                # USB 设备（HID）
│   ├── CH58x_usbhostBase.c / Class.c # USB 主机
│   ├── CH58x_usb2dev.c / usb2host*.c # USB2.0
│   └── inc/                          # 头文件：CH583SFR.h、CH58x_common.h、CH58x_clk.h ...
│
├── LIB/                              # BLE 协议栈库
│   ├── LIBCH58xBLE.a                 # BLE 库（链接 -lCH58xBLE）
│   ├── CH58xBLE_ROM.h                # ROM 版接口声明
│   ├── CH58xBLE_LIB.h                # LIB 版接口声明
│   ├── CH58xBLE_ROM.hex              # BLE ROM 固件
│   └── CH58xBLE_ROMx.hex
│
├── HAL/                              # 硬件抽象层
│   ├── MCU.c                         # CH58X_BLEInit / HAL_Init / 校准
│   ├── scan_key.c                    # 矩阵扫描、按键读取、模式切换键检测
│   ├── ws2812b.c                     # WS2812 RGB 灯效（TMR2 PWM + DMA）
│   ├── RTC.c                         # RTC
│   ├── SLEEP.c                       # 低功耗睡眠
│   ├── KEY.c / LED.c                 # 按键 / LED（当前未参与编译）
│   └── include/
│       ├── config.h                  # BLE 参数（MAC/DCDC/SLEEP/SNV/连接数...）
│       ├── HAL.h                     # HAL 总头文件
│       ├── scan_key.h                # 矩阵行列引脚定义、按键缓冲
│       └── ws2812.h / RTC.h / SLEEP.h / KEY.h / LED.h
│
├── Profile/                          # BLE GATT 服务
│   ├── hidkbdservice.c               # HID 键盘服务
│   ├── hiddev.c                      # HID 设备通用逻辑
│   ├── battservice.c                 # 电池服务
│   ├── devinfoservice.c              # 设备信息服务
│   ├── scanparamservice.c            # 扫描参数服务
│   └── include/                      # 上述各服务头文件
│
├── APP/                              # 应用层
│   ├── hidkbd_main.c                 # main()：vial_init() 读模式 → USB/BLE/RF
│   ├── USB_MODE.c                    # USB HID 模式（用 TMR3）
│   ├── BLE_MODE.c                    # 蓝牙 BLE 模式
│   ├── RF_MODE.c                     # 2.4G 无线模式
│   ├── libVIAL.a                     # VIAL 配置库（链接 -lVIAL）
│   └── include/
│       ├── hidkbd.h                  # HID 键盘任务接口
│       └── USB_MODE.h / RF_MODE.h / VIAL.h
│
└── obj/                              # 构建输出（自动生成，勿手动改）
    └── makefile / subdir.mk / *.o / *.d / *.elf / *.hex / *.map / *.lst
```

> 各目录文件数：StdPeriphDriver 41、HAL 15、Profile 10、APP 9、LIB 5、RVMSIS 2、Startup 1、Ld 1。

### 2.2 `.cproject` 构建配置改动（5 处）

与 KBD 已验证可编译的配置保持一致：

- **Include 路径 (-I)**：`Startup`、`APP/include`、`Profile/include`、`StdPeriphDriver/inc`、`HAL/include`、`Ld`、`LIB`、`RVMSIS`
- **宏定义 (-D)**：`HAL_SLEEP=1`（启用低功耗睡眠）。~~`DEBUG=1`~~ **已移除**（见 §7.11 — 这是导致 Vial 无法通信的真根因）
- **库搜索路径 (-L)**：`../`、`../LIB`、`../APP`、`../StdPeriphDriver`
- **链接库 (-l)**：`ISP583` → `VIAL` → `CH58xBLE`
- **sourceEntries**：显式列出 `APP`、`HAL`(排除 `KEY.c`/`LED.c`/`ws2812b.c`)、`LIB`、`Ld`、`Profile`、`RVMSIS`、`Startup`、`StdPeriphDriver`(全量编译，不排除任何 `CH58x_*.c`)

> 注：KBD 参考工程的 `.cproject` 在 StdPeriphDriver 排除列表里误排了 `CH58x_timer2.c`/`CH58x_timer3.c`，而 `ws2812b.c` 的 `TMR2_PWMInit`、`USB_MODE.c` 的 `TMR3_TimerInit` 正是由这两个文件提供，会导致链接报 `undefined reference`。本工程已去掉该排除项，全量编译所有 CH58x 驱动文件（未用函数由 `--gc-sections` 自动裁剪）。

### 2.3 关键决策

1. **覆盖 PAD 模板 SDK**：两套 SDK 有 12 个文件不同（`CH583SFR.h`、`CH58x_common.h`、`startup_CH583.S`、`core_riscv.h` 等）。因 `LIBCH58xBLE.a`/`libVIAL.a` 是按 KBD 的 SDK 预编译的，使用 KBD 的 SDK 才能保证 ABI 一致、链接通过。已校验所有 `.a` 库与差异文件 MD5 与 KBD 完全一致。
2. **删除模板 `src/Main.c`**：原 UART 回显例程与 `APP/hidkbd_main.c` 中的 `main()` 冲突，必须移除。
3. **未改动** `.project` / `.launch` / `.wvproj`：`.launch` 仍指向 `obj\CH582_VIAL_PAD.elf`，调试配置有效。

---

<h2 id="sec3">三、构建与烧录</h2>

1. MounRiver Studio 打开本工程。
2. **刷新工程（F5）→ Project > Clean → Build**（`obj/` 已清空，会全量重编）。
3. 产物：`obj/CH582_VIAL_PAD.elf` / `.hex` / `.map`（USB ISP 烧录还需 `.bin`，生成方法见第七节 7.5）。
4. 硬件 Debug：使用 `.launch` 配置（OpenOCD + WCH-RISCV 调试器），SVD 为 `CH58Xxx.svd`。

> 若链接报 `undefined reference to TMR2_PWMInit / TMR3_TimerInit` 等 SDK 函数，原因是 StdPeriphDriver 排除列表误排了对应 `.c` 文件，确认该 entry 无 `excluding` 即可。
> 若报其它 `undefined reference`（库之间循环依赖），可将三个库包进 `-Wl,--start-group ... -end-group`。

---

<h2 id="sec4">四、三模切换逻辑（USB/BLE 可用，2.4G 待实现；复位切换）</h2>

**当前状态（B0.6+）**：`main()` 读取 EEPROM `0x3F00` 模式字节并按值分支——`0x0B` USB（默认，LVGL 三页 UI）、`0xBE` BLE（BLE 协议栈 + LVGL 单主页）、`0x24` 2.4G（占位，当前复位回 USB）。切换为**复位式**：长按切换键约 2s → 写模式字节到 data flash → `SYS_ResetExecute()` 复位进入目标模式（非热切换；冷启动自复位见 §8.14 B0.7.4）。开机按住 `7` 可强制回 USB（B0.4 逃生键）。

| 切换键（物理键） | 写入模式字节 | 对应模式 |
|---|---|---|
| `key_data_buf[2][0]`（7） | `0x0B` | USB 有线 |
| `key_data_buf[2][1]`（8） | `0xBE` | 蓝牙 BLE |
| `key_data_buf[2][2]`（9） | `0x24` | 2.4G 无线 |

切换键默认键值即 **KP_7 / KP_8 / KP_9**（财务布局，见 §5.1），**无需先经 VIAL 配置**。长按计数 `change_mode_USB/BLE/24`（`HAL/scan_key.c`）达阈值后写模式字节并 `SYS_ResetExecute()`。

> **待办**：实现 2.4G RF 模式——`main()` 的 `0x24` 分支目前是 `SYS_ResetExecute()` 占位；`RF_MODE.c` 的 `RF_Init/RF_Tx/RF_Shut` 已就绪，复用同一套 TMOS + 共享 RAM 覆盖区（见 §10.3）。

---

<h2 id="sec5">五、功能与计划（矩阵 / 屏幕 UI / 上位机）</h2>

当前代码是参考工程的**全键盘**实现，需按财务小键盘的实际硬件裁剪。以下为后续工作清单：

### 5.1 矩阵适配 ✅ 已完成
- 文件：`HAL/include/scan_key.h`、`HAL/scan_key.c`
- 当前配置：**6 行 × 4 列**，Row: PA4/PA5/PA15/PA14/PA13/PA12，Col: PB12/PB13/PB14/PB15
- `key_data_buf[6][4]` **uint16_t** 4 层键值表，支持 QMK 16-bit 修饰符键码（`QK_LSFT|KC_9` = 0x0226 等）
- 默认键值为财务小键盘布局：R0=`(` / `)` / `=` / Tab，R1~R5=完整数字键盘（NumLock, /, *, Del, 7~9, -, 4~6, +, 1~3, Enter, 0, .）
- **扫描方式**（`dd6c2bb`，2026-08-06）：**驱动列、读行**（`get_key()`），列 (GPIOB) 为输出 PP、行 (GPIOA) 为输入上拉。扫描时逐列拉 LOW 后读取所有行电平，自动将 QMK 16-bit 键码拆解为 HID modifier byte + usage byte。
  - **之前**（v0.3）：驱动行、读列（`get_key_fanz()`），行输出、列输入上拉。
  - **变更原因**：PCB 二极管方向与 `get_key_fanz` 的电流方向相反（阻断），改为 `get_key` 后电流方向匹配→按键正常检测。
  - **二极管方向（已确认）**：**阳极接 row、阴极接 col** → 导通方向 row→col，恰好匹配 `get_key`（驱动列、读行）。
  - **ghosting 修复**（`82192f5`，2026-08-06）：列切换间加 **2µs 行恢复延时**（`mDelayuS(2)`）。行线为 GPIOA 内部 40kΩ 上拉，列恢复 HIGH 后行需经 RC 充电（τ≈40kΩ×10pF≈400ns，5τ≈2µs）→ 无延时则上一列按键残留电平污染下一列 → 按一键出两键（如 `(`→`()`）。
  - ⚠️ 若 PCB 二极管重新按正确方向焊接，需切回 `get_key_fanz` 并恢复原 GPIO 方向（行输出、列输入）。
- 注意：PB12/PB13 是 USB2（U2D-/U2D+）固定引脚，当前仅用 USB1 故无冲突。若未来启用 USB2，需改引脚。

### 5.2 HID 描述符与键值表（高优先）
- 文件：`APP/USB_MODE.c`（USB HID）、`APP/BLE_MODE.c`（BLE HID）
- 待办：报告描述符改为数字小键盘（Keyboard + Numpad）；键值表映射改为小键盘按键（0~9、+、-、*、/、Enter、.、NumLock 等）；财务场景可能需要的组合键/宏。

### 5.3 模式切换组合键
- 文件：`HAL/scan_key.c`（`find_mode_changekey`、`change_mode_*`）
- 待办：按小键盘可用按键重新定义 USB/BLE/2.4G 切换组合键。

### 5.4 VIAL 键值配置
- `libVIAL.a` 为预编译库，VIAL 协议与键位存储在 flash。
- 待办：用 VIAL 配置工具生成小键盘布局并写入；`vial_init()` 已跳过（见 §7.4），键值表由 `EEPROM_READ` + `FLASH_DATA_VIAL_WITE_mode` 管理。

### 5.5 RGB 灯效 ✅ 已移除
- 文件：`HAL/ws2812b.c`、`HAL/include/ws2812.h`（保留在树中供未来移植到更大封装）
- QFN28 封装无空闲引脚接 WS2812 灯带，已从 `main()`、`BLE_MODE.c`、`RF_MODE.c` 移除所有 `Ws2812_Init()`/`process_RGB_to_pwm()`/`PWM_DATA_DMA_send()` 调用
- `ws2812.h` 不再被 `hidkbd_main.c` include
- **当前不参与编译**：`.cproject` HAL 源条目已排除 `ws2812b.c`（如未来启用，从 excluding 移除并恢复调用）

### 5.6 硬件引脚核对
- `hidkbd_main.c` 中 PA5 复位按键、调试串口 TXD1（PA9）等引脚需与小键盘原理图核对。
- `HAL_SLEEP=1` 下上电会将 PA/PB 全部配为上拉输入，确认睡眠唤醒引脚（扫描列/模式键）配置正确。

### 5.7 低功耗与电池（若需）
- `DCDC_ENABLE`、`HAL_SLEEP`、`BLE_SNV` 等参数在 `HAL/include/config.h`。
- 待办：按电池供电需求调整睡眠参数、RTC 唤醒时间、电池电量上报（`Profile/battservice.c`）。

### 5.8 屏幕 UI（2026-08-02 进行中）

#### 5.8.1 屏幕驱动 ✅ 已完成（NV3007 142×428 彩屏：横向 428×142 + 列转置 + 防花屏）

- **型号**：2.79 寸 SPI 彩屏 **T279VJ-C10-01**，**NV3007 驱动，142×428 分辨率**（GRAM 168×428，可见窗口 X=12..153、Y=0..427）；软件层**横向使用 428×142**（面板旋转 270°）
- **文件**：`HAL/NV3007.c`、`HAL/include/NV3007.h`（文件名随屏幕改为 NV3007；API 统一为 `NV3007_*`（v0.5 起原 `ST7789_*` 全部改名）；内部为 NV3007 初始化序列与 428×142 映射）
- **驱动方式**：GPIO bit-bang SPI（PA9=SCK, PA8=MOSI, PB7=DC, PB4=BL），SPI **mode 0**（CPOL=0/CPHA=0，SCK 空闲低、上升沿采样；NV3007 与 ST7789 的 4 线 SPI 时序一致，旧 bit-bang 直接复用）
- **背光极性**：**ACTIVE-HIGH**（PB4 高 = 点亮，`NV3007_BL_ACTIVE_HIGH=1`，与 LVGL NV3007 Arduino 示例一致；旧 ST7789 板是低电平点亮）。若上电黑屏，量 PB4 应为高；模块相反则把该宏改 0
- **CS 接地**：CS 直接接地，无需 GPIO 脉冲，PA11 释放
- **初始化序列**：默认采用**卖家 T279VJ-C10-01（2.79"）示例代码**（`0xFF 0xA5` 厂商模式 + 279 gamma + `0xF1` 数据 `0x0E 0x17` + `0x3A=0x05` + SLPOUT 220ms + DISPON，已与 Arduino/STM32/C51 三份卖家代码逐字节核对）；`NV3007_INIT_VARIANT=0` 可切回 Arduino_GFX 1.68" 序列备用
- **SPI 波形**：`SPI_WriteByte` 每字节结束把 **SCK 拉低**（空闲低，与卖家「SCL 空闲时低电平，第一个上升沿采样」一致；旧 ST7789 驱动字节间 SCK 空闲高，NV3007 不适用）
- **字节同步**：复位前拉低 SCK/MOSI 防毛刺 + DC=0 下连发 8×NOP 对齐字节边界（CS 接地的关键）
- **防花屏**：DISPON 前先写全黑 GRAM，上电直接黑屏无随机闪烁
- **方向/旋转**：MADCTL=0x00（竖屏物理序），`NV3007_Flush` 内做**列窗口转置**——每条 LVGL 逻辑行 = 一条物理列窗口（列 = 153 - y，行 = x..x+w-1），无需 MV/MX 即可 270° 横屏；若图像上下颠倒，改 `HAL/include/NV3007.h` 的 `NV3007_ROT_REV_Y`
- **字体**：v0.3 Adafruit 5×7 取模（bit6 顶）+ `NV3007_SetFontZoom()` 可缩放（1×/2×/3×/4×）
- **已实现**：init、全屏填充、矩形填充、画点、可缩放字符/字符串、水平/垂直线、背光控制
- **LVGL 局部刷新**：`lvgl_port.c` USB 模式 **428×3 行单缓冲 = 2568B**（16KB 池），BLE 模式 428×2 行 = 1712B（6KB 池）；全帧 428×142×2 = 118KB 放不进 32K RAM
- **待办**：TMR2 PWM 调光、DMA/硬件 SPI 刷新（bit-bang 全屏约 120ms）

**屏幕模拟器**（`tools/tft_sim.html`）：仍是旧 284×76 自绘 UI 模拟器；NV3007 三页布局以 `Reference/numpad-ui-preview.html` 与真机为准。

#### 5.8.2 UI 框架 ✅ 已实现（LVGL 三页双主题，2026-08-06）

- **实现**：`HAL/numpad_ui.c/h`（移植自 `LVGL-opendesign/lv_sim`，LVGL 8.3 三页 UI）
- **主页**：实时时钟（HH:MM，硬件 RTC）+ 日期 + WiFi/蓝牙图标 + USB/BT/RF 模式按钮
- **计算器**：过程行 + 24px 结果行，`double` 表达式求值（`ui_calc_input`），实体小键盘驱动
- **设置**：亮度（进度条）/ 休眠（10/30/60/永不）/ 主题（浅/深瞬时切换）/ 重置连接
- **导航**：底部 3 圆点；**Tab + Backspace 组合键循环翻页**；计算器页按键直通 `ui_calc_input`，其他页保持 HID 输出
- **主题**：共享样式 + `lv_obj_report_style_change()` 瞬时全量切换
- **实时时钟**：CH582 RTC（`lvgl_rtc_init` 内部 32K + 非法时间初始化；`ui_hook_get_rtc` 强符号读取）
- **待办**：中文字体（`ui_font_cn_14/12`）、设置页按键交互、主题/亮度 EEPROM 持久化、`ui_hook_mode_output` 接三模

> **B0.6（2026-08-08）**：LVGL 已重新启用（`LVGL_EN=1`）——USB 模式三页 UI（16KB 池），**BLE 模式也运行 LVGL 主页**（共享区尾部 6KB 池，时钟 + BT 模式高亮）。B0.3/B0.4 的切换修复全部保留。

> **B0.7（2026-08-09）**：屏幕更换为 **NV3007 142×428 彩屏**（横向 428×142）——`HAL/NV3007.c/h` 内部换成 NV3007 初始化 + 列转置 flush，三页布局 284×76 → 428×142 重排，LVGL 行缓冲按新分辨率重算（USB 428×3 / BLE 428×2），详见 §8.14 M10。

#### 5.8.3 页面切换与按键路由（已实现）

- **三页**：主页（HID）↔ 计算器（输入）↔ 设置，`Tab + Backspace` 循环切换
- **计算器页**：小键盘按键（数字/`+ - * /`/`.`/`Enter`/`Backspace`/`ESC`=清空）→ `ui_calc_input`，**不触发 HID**
- **设置页**：**完全禁止 HID 输出**；数字键 1-4 直控四行（1=亮度+20 / 2=休眠循环 / **3=主题浅深切换** / 4=重置连接），其他键忽略
- **主页**：按键正常 HID 上报
- 页面切换不改变 USB/BLE/2.4G 三模状态（三模接线见 §4 待办）

#### 5.8.4 界面预览（静态嵌入 Reference/numpad-ui-preview.html）

<details>
<summary>📺 屏幕 UI 预览：3 页 × 浅色/深色（点击展开，静态渲染）</summary>

<style>
:root { --bg:      oklch(0.972 0.004 250);   
    --surface: oklch(1 0 0);
    --fg:      oklch(0.25 0.012 260);
    --muted:   oklch(0.52 0.015 255);
    --border:  oklch(0.905 0.007 250);
    --accent:  oklch(0.52 0.155 258);    
    --bezel:   oklch(0.17 0.012 280);    

    
    --sc-bg-light:#ffffff;  --sc-bg-dark:#121212;
    --sc-card-light:#ffffff;--sc-card-dark:#1e1e1e;
    --sc-border-light:#e8e8e8; --sc-border-dark:#333333;
    --sc-fg-light:#1a1a1a;  --sc-fg-dark:#eeeeee;
    --sc-muted-light:#999999; --sc-muted-dark:#888888;
    --sc-pressed-light:#f5f5f5; --sc-pressed-dark:#2a2a2a;
    --sc-active-light:#333333; --sc-active-dark:#ffffff;
    --sc-active-fg-light:#ffffff; --sc-active-fg-dark:#121212;
    --sc-soft-light:#fafafa; --sc-soft-dark:#1a1a1a;
    --sc-op-light:#f8f8f8;  --sc-op-dark:#1e1e1e;

    
    --accent-soft: color-mix(in oklch, var(--accent) 14%, transparent);
    --fg-soft:     color-mix(in oklch, var(--fg) 6%, transparent);

    
    --font-display: 'Iowan Old Style', 'Charter', Georgia, 'Times New Roman', serif;
    --font-body:    -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
    --font-mono:    ui-monospace, 'JetBrains Mono', 'SF Mono', Menlo, monospace;

    
    --fs-h1: clamp(40px, 5.4vw, 66px);
    --fs-h2: clamp(30px, 3.6vw, 44px);
    --fs-h3: 22px;
    --fs-lead: 19px;
    --fs-body: 16px;
    --fs-meta: 13px;

    --gap-xs: 8px;  --gap-sm: 12px; --gap-md: 20px;
    --gap-lg: 32px; --gap-xl: 56px; --gap-2xl: 96px;
    --container: 1180px;
    --gutter: 32px;
    --radius: 10px; --radius-lg: 16px; }
.readme-preview *, .readme-preview *::before, .readme-preview *::after { box-sizing: border-box; }
.readme-preview { margin: 0;
    background: var(--bg);
    color: var(--fg);
    font-family: var(--font-body);
    font-size: var(--fs-body);
    line-height: 1.55;
    text-rendering: optimizeLegibility;
    -webkit-font-smoothing: antialiased; }
.readme-preview img, .readme-preview svg { display: block; max-width: 100%; }
.readme-preview a { color: inherit; text-decoration: none; }
.readme-preview button { font: inherit; cursor: pointer; }
.readme-preview p { text-wrap: pretty; }
.readme-preview h1, .readme-preview h2, .readme-preview h3, .readme-preview h4 { text-wrap: balance; }
.readme-preview .container { max-width: var(--container); margin-inline: auto; padding-inline: var(--gutter); }
.readme-preview .section { padding-block: clamp(48px, 8vw, var(--gap-2xl)); }
.readme-preview .section + .section { border-top: 1px solid var(--border); }
.readme-preview .stack { display: flex; flex-direction: column; }
.readme-preview .stack > * + * { margin-top: var(--gap-md); }
.readme-preview .row { display: flex; align-items: center; gap: var(--gap-md); }
.readme-preview .row-between { display: flex; align-items: center; justify-content: space-between; gap: var(--gap-md); }
.readme-preview .grid-2 { display: grid; grid-template-columns: repeat(2, 1fr); gap: var(--gap-lg); }
.readme-preview .grid-3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: var(--gap-lg); }
.readme-preview .grid-4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: var(--gap-md); }
@media (max-width: 920px) { .readme-preview .grid-2, .readme-preview .grid-3, .readme-preview .grid-4 { grid-template-columns: 1fr; } }
.readme-preview .h1, .readme-preview h1 { font-family: var(--font-display); font-size: var(--fs-h1); line-height: 1.06; letter-spacing: -0.02em; margin: 0; }
.readme-preview .h2, .readme-preview h2 { font-family: var(--font-display); font-size: var(--fs-h2); line-height: 1.12; letter-spacing: -0.015em; margin: 0; }
.readme-preview .h3, .readme-preview h3 { font-size: var(--fs-h3); font-weight: 600; line-height: 1.3; letter-spacing: -0.005em; margin: 0; }
.readme-preview .lead { font-size: var(--fs-lead); line-height: 1.55; color: var(--muted); max-width: 60ch; margin: 0; }
.readme-preview .eyebrow { font-family: var(--font-mono); font-size: 12px; letter-spacing: 0.08em;
    text-transform: uppercase; color: var(--accent); margin: 0 0 var(--gap-md); }
.readme-preview .meta { font-family: var(--font-mono); font-size: var(--fs-meta); color: var(--muted); }
.readme-preview .num { font-family: var(--font-mono); font-variant-numeric: tabular-nums; }
.readme-preview .topnav { position: sticky; top: 0; z-index: 10;
    background: color-mix(in oklch, var(--bg) 92%, transparent);
    backdrop-filter: blur(12px);
    border-bottom: 1px solid var(--border); }
.readme-preview .topnav-inner { display: flex; align-items: center; justify-content: space-between; padding-block: 14px; }
.readme-preview .topnav .logo { font-family: var(--font-display); font-size: 19px; font-weight: 600; letter-spacing: -0.01em; }
.readme-preview .topnav nav { display: flex; gap: var(--gap-lg); }
.readme-preview .topnav nav a { font-size: 14px; color: var(--muted); }
.readme-preview .topnav nav a:hover { color: var(--fg); }
.readme-preview .pagefoot { padding-block: var(--gap-xl); color: var(--muted); font-size: 13px; border-top: 1px solid var(--border); }
.readme-preview .pagefoot .row-between { flex-wrap: wrap; gap: var(--gap-md); }
.readme-preview .btn { display: inline-flex; align-items: center; gap: 8px;
    padding: 11px 20px; border-radius: var(--radius);
    border: 1px solid transparent; font-size: 15px; font-weight: 500;
    letter-spacing: -0.005em;
    transition: transform 0.05s ease, background 0.15s ease, border-color 0.15s ease; }
.readme-preview .btn:active { transform: translateY(1px); }
.readme-preview .btn-primary { background: var(--accent); color: var(--surface); border-color: var(--accent); }
.readme-preview .btn-primary:hover { background: color-mix(in oklch, var(--accent) 88%, black); }
.readme-preview .btn-secondary { background: transparent; color: var(--fg); border-color: var(--border); }
.readme-preview .btn-secondary:hover { border-color: var(--fg); }
.readme-preview .card { background: var(--surface); border: 1px solid var(--border); border-radius: var(--radius-lg); padding: 28px; }
.readme-preview .pill { display: inline-flex; align-items: center; gap: 6px; padding: 4px 10px;
    background: var(--accent-soft); color: var(--accent);
    border-radius: 999px; font-family: var(--font-mono);
    font-size: 11px; letter-spacing: 0.04em; text-transform: uppercase; }
.readme-preview .tag { display: inline-flex; align-items: center; padding: 4px 10px;
    background: transparent; color: var(--muted);
    border: 1px solid var(--border); border-radius: 999px; font-size: 12px; }
.readme-preview .chip-row { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; margin-top: 30px; }
.readme-preview .ds-table { width: 100%; border-collapse: collapse; font-size: 14px; }
.readme-preview .ds-table th, .readme-preview .ds-table td { padding: 11px 14px; text-align: left; border-bottom: 1px solid var(--border); }
.readme-preview .ds-table th { color: var(--muted); font-weight: 500; font-family: var(--font-mono); font-size: 12px; letter-spacing: 0.04em; text-transform: uppercase; }
.readme-preview .ds-table tbody tr:hover { background: var(--fg-soft); }
.readme-preview .table-wrap { overflow-x: auto; }
.readme-preview .swatch { display: inline-block; width: 13px; height: 13px; border-radius: 3px; margin-right: 8px; vertical-align: -2px; border: 1px solid color-mix(in oklch, var(--fg) 18%, transparent); }
.readme-preview .hex-label { font-family: var(--font-mono); font-size: 12px; }
.readme-preview .hero { padding-block: clamp(64px, 10vw, 132px); }
.readme-preview .hero-center { text-align: center; max-width: 34ch; margin-inline: auto; }
.readme-preview .hero h1 { margin-bottom: var(--gap-md); }
.readme-preview .hero .lead { margin-bottom: var(--gap-lg); }
.readme-preview .hero-cta { display: inline-flex; gap: var(--gap-sm); flex-wrap: wrap; justify-content: center; }
.readme-preview .sec-head { max-width: 680px; margin-bottom: 40px; }
.readme-preview .sec-head .lead { margin-top: 14px; }
.readme-preview .hero { padding-block: clamp(48px, 7vw, 104px); }
.readme-preview .hero-split { display: grid; grid-template-columns: minmax(0, 5fr) minmax(0, 6fr); gap: clamp(32px, 5vw, 72px); align-items: center; }
.readme-preview .hero-copy h1 { margin-bottom: var(--gap-md); }
.readme-preview .hero-copy .lead { margin-bottom: var(--gap-lg); }
.readme-preview .hero-copy .hero-cta { margin-bottom: var(--gap-md); }
.readme-preview .spec-line { font-family: var(--font-mono); font-size: 12px; color: var(--muted); letter-spacing: 0.02em; margin: 0; }
@media (max-width: 920px) { .readme-preview .hero-split { grid-template-columns: 1fr; } }
.readme-preview .workbench { position: relative; border-radius: var(--radius-lg);
    background:
      linear-gradient(color-mix(in oklch, var(--wb-line) 42%, transparent) 1px, transparent 1px),
      linear-gradient(90deg, color-mix(in oklch, var(--wb-line) 42%, transparent) 1px, transparent 1px),
      var(--wb-bg);
    background-size: 26px 26px;
    padding: clamp(16px, 3.5vw, 34px);
    display: flex; flex-direction: column; align-items: center; gap: 16px;
    box-shadow: inset 0 1px 0 color-mix(in oklch, var(--wb-fg) 16%, transparent);
    --wb-bg: oklch(0.19 0.012 265);
    --wb-line: oklch(0.58 0.02 265);
    --wb-fg: oklch(0.84 0.012 265);
    --wb-muted: oklch(0.63 0.018 265); }
.readme-preview .workbench .preview-stage { --ps: 1.5; }
@media (max-width: 1100px) { .readme-preview .workbench .preview-stage { --ps: 1.28; } }
@media (max-width: 920px) { .readme-preview .workbench .preview-stage { --ps: 1.55; } }
@media (max-width: 760px) { .readme-preview .workbench .preview-stage { --ps: 1.1; } }
@media (max-width: 560px) { .readme-preview .workbench .preview-stage { --ps: 0.9; } }
@media (max-width: 400px) { .readme-preview .workbench .preview-stage { --ps: 0.8; } }
@media (max-width: 340px) { .readme-preview .workbench .preview-stage { --ps: 0.7; } }
.readme-preview .workbench .device { box-shadow: 0 26px 48px -20px color-mix(in oklch, oklch(0 0 0) 78%, transparent),
                0 2px 6px color-mix(in oklch, oklch(0 0 0) 45%, transparent),
                inset 0 1px 0 color-mix(in oklch, oklch(1 0 0) 12%, transparent); }
.readme-preview .wb-dim { position: absolute; font-family: var(--font-mono); font-size: 11px; letter-spacing: 0.06em; color: var(--wb-muted); }
.readme-preview .wb-dim-w { top: 14px; left: 16px; }
.readme-preview .wb-dim-h { top: 50%; right: 12px; transform: translateY(-50%); writing-mode: vertical-rl; }
.readme-preview .workbench::after { content: ''; position: absolute; left: 0; right: 0; bottom: 0; height: 3px; pointer-events: none;
    background: linear-gradient(90deg, transparent, color-mix(in oklch, var(--wb-fg) 22%, transparent) 12%, transparent 26%); }
.readme-preview .wb-controls { display: flex; flex-wrap: wrap; justify-content: center; align-items: center; gap: 10px; }
.readme-preview .seg-dark { background: color-mix(in oklch, var(--wb-bg) 72%, transparent); border-color: color-mix(in oklch, var(--wb-fg) 24%, transparent); box-shadow: none; }
.readme-preview .seg-dark .seg-btn { color: var(--wb-muted); }
.readme-preview .seg-dark .seg-btn:hover { color: var(--wb-fg); }
.readme-preview .seg-dark .seg-btn.active { background: var(--wb-fg); color: var(--wb-bg); }
.readme-preview .status-chip-dark { background: color-mix(in oklch, var(--wb-bg) 72%, transparent); border-color: color-mix(in oklch, var(--wb-fg) 24%, transparent); color: var(--wb-muted); }
.readme-preview .status-chip-dark b { color: var(--wb-fg); }
.readme-preview .workbench .keyhint { margin-top: 0; color: var(--wb-muted); }
.readme-preview .intent-grid { display: grid; grid-template-columns: repeat(2, 1fr); border-top: 1px solid var(--border); border-left: 1px solid var(--border); }
.readme-preview .intent-item { padding: 26px 26px 30px; border-right: 1px solid var(--border); border-bottom: 1px solid var(--border); background: var(--surface); transition: background 0.15s ease; }
.readme-preview .intent-item:hover { background: color-mix(in oklch, var(--bg) 55%, var(--surface)); }
.readme-preview .intent-idx { display: block; font-family: var(--font-mono); font-size: 12px; color: var(--muted); letter-spacing: 0.08em; margin-bottom: 12px; }
.readme-preview .intent-item h3 { font-size: 17px; font-weight: 600; letter-spacing: -0.01em; margin-bottom: 8px; }
.readme-preview .intent-item p { margin: 0; font-size: 14px; line-height: 1.62; color: var(--muted); }
@media (max-width: 920px) { .readme-preview .intent-grid { grid-template-columns: 1fr; } }
.readme-preview .g-caption { margin: 10px 2px 0; font-size: 12px; color: var(--muted); line-height: 1.5; }
.readme-preview .seg { display: inline-flex; align-items: center; gap: 2px; padding: 3px; background: var(--surface); border: 1px solid var(--border); border-radius: 999px; box-shadow: 0 1px 2px color-mix(in oklch, var(--fg) 5%, transparent); }
.readme-preview .seg-btn { border: 0; background: transparent; color: var(--muted); font-size: 13px; font-weight: 500; padding: 7px 16px; border-radius: 999px; }
.readme-preview .seg-btn:hover { color: var(--fg); }
.readme-preview .seg-btn.active { background: var(--fg); color: var(--bg); }
.readme-preview .status-chip { display: inline-flex; align-items: center; gap: 4px; padding: 8px 14px; background: var(--surface); border: 1px solid var(--border); border-radius: 999px; font-family: var(--font-mono); font-size: 12px; color: var(--muted); }
.readme-preview .status-chip b { color: var(--fg); font-weight: 600; }
.readme-preview .preview-stage { --ps: 3.6; display: flex; justify-content: center; }
.readme-preview .preview-box { position: relative; width: 1066px; width: calc(296px * var(--ps)); height: 317px; height: calc(88px * var(--ps)); }
.readme-preview .preview-scale { position: absolute; top: 0; left: 0; transform: scale(var(--ps)); transform-origin: top left; }
.readme-preview .device { width: 296px; height: 88px; padding: 6px; background: var(--bezel); border-radius: 14px; box-shadow: 0 30px 60px -24px color-mix(in oklch, oklch(0 0 0) 55%, transparent), 0 2px 6px color-mix(in oklch, oklch(0 0 0) 24%, transparent), inset 0 1px 0 color-mix(in oklch, oklch(1 0 0) 10%, transparent); }
.readme-preview .device-screen { position: relative; width: 284px; height: 76px; border-radius: 6px; overflow: hidden; touch-action: none; }
.readme-preview .keyhint { text-align: center; margin-top: 6px; max-width: 680px; margin-inline: auto;
    font-family: var(--font-mono); font-size: 12px; color: var(--muted); }
@media (max-width: 1320px) { .readme-preview .preview-stage { --ps: 3; } }
@media (max-width: 1100px) { .readme-preview .preview-stage { --ps: 2.4; } }
@media (max-width: 900px) { .readme-preview .preview-stage { --ps: 1.8; } }
@media (max-width: 620px) { .readme-preview .preview-stage { --ps: 1.3; } }
.readme-preview .screen { width: 284px; height: 76px; position: relative; overflow: hidden;
    border-radius: 6px;
    background: var(--sc-bg); color: var(--sc-fg);
    font-family: var(--font-body); font-size: 11px; line-height: 1.15;
    user-select: none; -webkit-user-select: none;
    box-shadow: inset 0 0 0 1px color-mix(in oklch, var(--sc-fg) 12%, transparent);
    --sc-bg: var(--sc-bg-light); --sc-card: var(--sc-card-light);
    --sc-border: var(--sc-border-light); --sc-fg: var(--sc-fg-light);
    --sc-muted: var(--sc-muted-light); --sc-pressed: var(--sc-pressed-light);
    --sc-active: var(--sc-active-light); --sc-active-fg: var(--sc-active-fg-light);
    --sc-soft: var(--sc-soft-light); --sc-op: var(--sc-op-light); }
.readme-preview .screen.dark { --sc-bg: var(--sc-bg-dark); --sc-card: var(--sc-card-dark);
    --sc-border: var(--sc-border-dark); --sc-fg: var(--sc-fg-dark);
    --sc-muted: var(--sc-muted-dark); --sc-pressed: var(--sc-pressed-dark);
    --sc-active: var(--sc-active-dark); --sc-active-fg: var(--sc-active-fg-dark);
    --sc-soft: var(--sc-soft-dark); --sc-op: var(--sc-op-dark); }
.readme-preview .page { position: absolute; inset: 0; }
.readme-preview .page.hidden { display: none; }
.readme-preview .screen button:focus-visible { outline: 1.5px solid color-mix(in oklch, var(--sc-fg) 55%, transparent); outline-offset: -1px; }
.readme-preview .home-left { position: absolute; left: 0; top: 0; bottom: 0; width: 170px; }
.readme-preview .home-right { position: absolute; right: 0; top: 0; bottom: 0; width: 114px; padding: 4px; display: flex; flex-direction: column; gap: 4px; }
.readme-preview .home-status { position: absolute; top: 6px; right: 5px; display: flex; gap: 3px; align-items: center; color: var(--sc-muted); }
.readme-preview .home-status svg { width: 11px; height: 11px; }
.readme-preview .home-time { position: absolute; left: 8px; top: 7px; font-size: 24px; font-weight: 700; letter-spacing: -0.02em; font-variant-numeric: tabular-nums; color: var(--sc-fg); }
.readme-preview .home-date { position: absolute; left: 8px; bottom: 8px; font-size: 10px; color: var(--sc-muted); letter-spacing: 0.02em; }
.readme-preview .mode-btn { height: 20px; border-radius: 6px; border: 1px solid var(--sc-border);
    background: var(--sc-card); color: var(--sc-fg);
    font-size: 11px; font-weight: 600; letter-spacing: 0.04em;
    display: flex; align-items: center; justify-content: center;
    transition: background 0.12s ease; }
.readme-preview .mode-btn.active { background: var(--sc-active); color: var(--sc-active-fg); border-color: var(--sc-active); }
.readme-preview .mode-btn:active { background: var(--sc-pressed); }
.readme-preview .calc-display { position: absolute; left: 4px; right: 4px; top: 3px; bottom: 8px;
    background: var(--sc-soft); border: 1px solid var(--sc-border); border-radius: 4px;
    display: flex; flex-direction: column; overflow: hidden; }
.readme-preview .calc-proc { flex: none; height: 24px; padding: 0 8px;
    display: flex; align-items: center; justify-content: flex-end;
    font-size: 11px; color: var(--sc-muted); letter-spacing: 0.02em;
    font-variant-numeric: tabular-nums;
    border-bottom: 1px solid var(--sc-border);
    overflow: hidden; white-space: nowrap; }
.readme-preview .calc-result { flex: 1; min-height: 0; padding: 2px 8px 6px;
    display: flex; align-items: flex-end; justify-content: flex-end;
    font-size: 24px; font-weight: 700; line-height: 1; letter-spacing: -0.02em;
    font-variant-numeric: tabular-nums; color: var(--sc-fg);
    overflow: hidden; white-space: nowrap; }
.readme-preview .settings-list { position: absolute; left: 4px; right: 4px; top: 2px; bottom: 9px; display: flex; flex-direction: column; }
.readme-preview .setting-row { flex: 1; display: flex; align-items: center; gap: 4px; padding: 0 5px;
    width: 100%; text-align: left; background: transparent; border: 0;
    border-bottom: 1px solid var(--sc-border); color: var(--sc-fg); border-radius: 0; }
.readme-preview .setting-row:last-child { border-bottom: 0; }
.readme-preview .setting-row:active { background: var(--sc-pressed); }
.readme-preview .setting-icon { width: 10px; height: 10px; color: var(--sc-muted); flex: none; }
.readme-preview .setting-icon svg { width: 10px; height: 10px; }
.readme-preview .setting-label { font-size: 10.5px; font-weight: 600; white-space: nowrap; }
.readme-preview .setting-value { margin-left: auto; font-size: 10px; color: var(--sc-muted); white-space: nowrap; }
.readme-preview .setting-bright .bright-bar { margin-left: auto; }
.readme-preview .setting-bright .setting-value { margin-left: 0; }
.readme-preview .setting-chev { color: var(--sc-muted); font-size: 11px; line-height: 1; }
.readme-preview .bright-bar { position: relative; width: 36px; height: 4px; border-radius: 2px; background: var(--sc-border); overflow: hidden; }
.readme-preview .bright-fill { position: absolute; top: 0; left: 0; bottom: 0; width: 80%; background: var(--sc-fg); }
.readme-preview .nav-dots { position: absolute; left: 0; right: 0; bottom: 3px; display: flex; justify-content: center; gap: 5px; z-index: 6; }
.readme-preview .dot { width: 4px; height: 4px; padding: 0; border: 0; border-radius: 50%; background: var(--sc-border); }
.readme-preview .dot.active { background: var(--sc-fg); }
.readme-preview .dot:focus-visible { outline: 1.5px solid color-mix(in oklch, var(--sc-fg) 60%, transparent); outline-offset: 1px; }
.readme-preview .gallery-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 22px; }
@media (max-width: 920px) { .readme-preview .gallery-grid { grid-template-columns: 1fr; } }
.readme-preview .gallery-item { background: var(--surface); border: 1px solid var(--border); border-radius: var(--radius-lg); padding: 16px 16px 14px; }
.readme-preview .g-title { display: flex; justify-content: space-between; align-items: center; font-size: 13px; font-weight: 600; margin-bottom: 14px; }
.readme-preview .g-tag { font-family: var(--font-mono); font-size: 11px; color: var(--muted); font-weight: 400; }
.readme-preview .g-box { display: flex; align-items: center; justify-content: center; height: 156px; border-radius: 10px; background: color-mix(in oklch, var(--bg) 55%, var(--border)); }
.readme-preview .g-wrap { transform: scale(1.7); flex: none; }
@media (max-width: 640px) { .readme-preview .g-wrap { transform: scale(1.3); } }
@media (max-width: 460px) { .readme-preview .g-wrap { transform: scale(1); } }
.readme-preview .real-head { margin-top: 56px; display: flex; align-items: baseline; gap: 14px; flex-wrap: wrap; margin-bottom: 18px; }
.readme-preview .real-row { display: flex; gap: 20px; justify-content: center; flex-wrap: wrap; }
.readme-preview .real-row + .real-row { margin-top: 16px; }
.readme-preview .real-cell { text-align: center; }
.readme-preview .real-cell .cap { font-family: var(--font-mono); font-size: 11px; color: var(--muted); margin-top: 8px; }
.readme-preview .real-cell .g-wrap { transform: none; }
.readme-preview .spec-note { margin-top: 32px; max-width: 680px; }
.readme-preview .spec-note h3 { margin-bottom: 8px; }
.readme-preview .spec-note p { margin: 0; color: var(--muted); font-size: 14px; }
.readme-preview a:focus-visible, .readme-preview button:focus-visible, .readme-preview [tabindex]:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
</style>

<div class="readme-preview">
  <div class="gallery-grid">
<div class="gallery-item"><div class="g-title">主页<span class="g-tag">浅色</span></div><div class="g-box"><div class="g-wrap"><div class="screen">
    <!-- Page 0 · 主页（时钟 + 键盘模式） -->
    <div class="page page-home hidden" data-od-id="page-home">
      <div class="home-left">
        <div class="home-status" aria-hidden="true">
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="1.5" y="5.5" width="11" height="5" rx="1.4"/><path d="M14 7.5v1" stroke-linecap="round"/><rect x="3" y="7" width="5.5" height="2" rx="0.6" fill="currentColor" stroke="none"/></svg>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M3 6.2c2.7-2.3 7.3-2.3 10 0"/><path d="M5 8.9c1.7-1.4 4.3-1.4 6 0"/><circle cx="8" cy="11.8" r="0.9" fill="currentColor" stroke="none"/></svg>
        </div>
        <div class="home-time">14:30</div>
        <div class="home-date">2026.08.04 周二</div>
      </div>
      <div class="home-right">
        <button type="button" class="mode-btn active" data-mode="USB" data-od-id="mode-usb">USB</button>
        <button type="button" class="mode-btn" data-mode="蓝牙" data-od-id="mode-bluetooth">蓝牙</button>
        <button type="button" class="mode-btn" data-mode="RF" data-od-id="mode-rf">RF</button>
      </div>
    </div>

    <!-- Page 1 · 计算器（纯运算过程显示，无屏幕按键） -->
    <div class="page page-calc hidden" data-od-id="page-calc">
      <div class="calc-display" aria-label="计算过程与结果" data-od-id="calc-readout">
        <div class="calc-proc" data-calc-proc></div>
        <div class="calc-result" data-calc-result>0</div>
      </div>
    </div>

    <!-- Page 2 · 设置 -->
    <div class="page page-settings hidden" data-od-id="page-settings">
      <div class="settings-list">
        <button type="button" class="setting-row setting-bright" data-od-id="setting-bright">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="2.8"/><path d="M8 1.6v1.6M8 12.8v1.6M1.6 8h1.6M12.8 8h1.6M3.6 3.6l1.1 1.1M11.3 11.3l1.1 1.1M12.4 3.6l-1.1 1.1M4.7 11.3l-1.1 1.1"/></svg></span>
          <span class="setting-label">亮度</span>
          <span class="bright-bar"><span class="bright-fill" data-bright-fill style="width:80%"></span></span>
          <span class="setting-value" data-brightness-value>80%</span>
        </button>
        <button type="button" class="setting-row setting-sleep" data-od-id="setting-sleep">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 4.6V8l2.3 1.5"/></svg></span>
          <span class="setting-label">休眠</span>
          <span class="setting-value" data-sleep-value>30秒</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-theme" data-od-id="setting-theme">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M12.6 9.6A5.6 5.6 0 1 1 6.4 3.4a4.6 4.6 0 0 0 6.2 6.2z"/></svg></span>
          <span class="setting-label">主题</span>
          <span class="setting-value theme-value">浅色</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-reset" data-od-id="setting-reset">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M13 8a5 5 0 1 1-1.6-3.6"/><path d="M13 2.6v2.4h-2.4"/></svg></span>
          <span class="setting-label">重置连接</span>
          <span class="setting-value" data-reset-value>执行</span>
          <span class="setting-chev">›</span>
        </button>
      </div>
    </div>

    <!-- 底部导航点 -->
    <div class="nav-dots" data-od-id="nav-dots">
      <button type="button" class="dot active" data-page="0" aria-label="主页"></button>
      <button type="button" class="dot" data-page="1" aria-label="计算器"></button>
      <button type="button" class="dot" data-page="2" aria-label="设置"></button>
    </div>
  </div></div></div><p class="g-caption">时钟 + 连接模式 · 默认待机页</p></div>
<div class="gallery-item"><div class="g-title">计算器<span class="g-tag">浅色</span></div><div class="g-box"><div class="g-wrap"><div class="screen">
    <!-- Page 0 · 主页（时钟 + 键盘模式） -->
    <div class="page page-home" data-od-id="page-home">
      <div class="home-left">
        <div class="home-status" aria-hidden="true">
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="1.5" y="5.5" width="11" height="5" rx="1.4"/><path d="M14 7.5v1" stroke-linecap="round"/><rect x="3" y="7" width="5.5" height="2" rx="0.6" fill="currentColor" stroke="none"/></svg>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M3 6.2c2.7-2.3 7.3-2.3 10 0"/><path d="M5 8.9c1.7-1.4 4.3-1.4 6 0"/><circle cx="8" cy="11.8" r="0.9" fill="currentColor" stroke="none"/></svg>
        </div>
        <div class="home-time">14:30</div>
        <div class="home-date">2026.08.04 周二</div>
      </div>
      <div class="home-right">
        <button type="button" class="mode-btn active" data-mode="USB" data-od-id="mode-usb">USB</button>
        <button type="button" class="mode-btn" data-mode="蓝牙" data-od-id="mode-bluetooth">蓝牙</button>
        <button type="button" class="mode-btn" data-mode="RF" data-od-id="mode-rf">RF</button>
      </div>
    </div>

    <!-- Page 1 · 计算器（纯运算过程显示，无屏幕按键） -->
    <div class="page page-calc" data-od-id="page-calc">
      <div class="calc-display" aria-label="计算过程与结果" data-od-id="calc-readout">
        <div class="calc-proc" data-calc-proc>128 + 64 × 5</div>
        <div class="calc-result" data-calc-result>448</div>
      </div>
    </div>

    <!-- Page 2 · 设置 -->
    <div class="page page-settings hidden" data-od-id="page-settings">
      <div class="settings-list">
        <button type="button" class="setting-row setting-bright" data-od-id="setting-bright">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="2.8"/><path d="M8 1.6v1.6M8 12.8v1.6M1.6 8h1.6M12.8 8h1.6M3.6 3.6l1.1 1.1M11.3 11.3l1.1 1.1M12.4 3.6l-1.1 1.1M4.7 11.3l-1.1 1.1"/></svg></span>
          <span class="setting-label">亮度</span>
          <span class="bright-bar"><span class="bright-fill" data-bright-fill style="width:80%"></span></span>
          <span class="setting-value" data-brightness-value>80%</span>
        </button>
        <button type="button" class="setting-row setting-sleep" data-od-id="setting-sleep">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 4.6V8l2.3 1.5"/></svg></span>
          <span class="setting-label">休眠</span>
          <span class="setting-value" data-sleep-value>30秒</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-theme" data-od-id="setting-theme">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M12.6 9.6A5.6 5.6 0 1 1 6.4 3.4a4.6 4.6 0 0 0 6.2 6.2z"/></svg></span>
          <span class="setting-label">主题</span>
          <span class="setting-value theme-value">浅色</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-reset" data-od-id="setting-reset">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M13 8a5 5 0 1 1-1.6-3.6"/><path d="M13 2.6v2.4h-2.4"/></svg></span>
          <span class="setting-label">重置连接</span>
          <span class="setting-value" data-reset-value>执行</span>
          <span class="setting-chev">›</span>
        </button>
      </div>
    </div>

    <!-- 底部导航点 -->
    <div class="nav-dots" data-od-id="nav-dots">
      <button type="button" class="dot" data-page="0" aria-label="主页"></button>
      <button type="button" class="dot active" data-page="1" aria-label="计算器"></button>
      <button type="button" class="dot" data-page="2" aria-label="设置"></button>
    </div>
  </div></div></div><p class="g-caption">运算过程 + 实时结果 · 显示型</p></div>
<div class="gallery-item"><div class="g-title">设置<span class="g-tag">浅色</span></div><div class="g-box"><div class="g-wrap"><div class="screen">
    <!-- Page 0 · 主页（时钟 + 键盘模式） -->
    <div class="page page-home" data-od-id="page-home">
      <div class="home-left">
        <div class="home-status" aria-hidden="true">
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="1.5" y="5.5" width="11" height="5" rx="1.4"/><path d="M14 7.5v1" stroke-linecap="round"/><rect x="3" y="7" width="5.5" height="2" rx="0.6" fill="currentColor" stroke="none"/></svg>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M3 6.2c2.7-2.3 7.3-2.3 10 0"/><path d="M5 8.9c1.7-1.4 4.3-1.4 6 0"/><circle cx="8" cy="11.8" r="0.9" fill="currentColor" stroke="none"/></svg>
        </div>
        <div class="home-time">14:30</div>
        <div class="home-date">2026.08.04 周二</div>
      </div>
      <div class="home-right">
        <button type="button" class="mode-btn active" data-mode="USB" data-od-id="mode-usb">USB</button>
        <button type="button" class="mode-btn" data-mode="蓝牙" data-od-id="mode-bluetooth">蓝牙</button>
        <button type="button" class="mode-btn" data-mode="RF" data-od-id="mode-rf">RF</button>
      </div>
    </div>

    <!-- Page 1 · 计算器（纯运算过程显示，无屏幕按键） -->
    <div class="page page-calc hidden" data-od-id="page-calc">
      <div class="calc-display" aria-label="计算过程与结果" data-od-id="calc-readout">
        <div class="calc-proc" data-calc-proc></div>
        <div class="calc-result" data-calc-result>0</div>
      </div>
    </div>

    <!-- Page 2 · 设置 -->
    <div class="page page-settings" data-od-id="page-settings">
      <div class="settings-list">
        <button type="button" class="setting-row setting-bright" data-od-id="setting-bright">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="2.8"/><path d="M8 1.6v1.6M8 12.8v1.6M1.6 8h1.6M12.8 8h1.6M3.6 3.6l1.1 1.1M11.3 11.3l1.1 1.1M12.4 3.6l-1.1 1.1M4.7 11.3l-1.1 1.1"/></svg></span>
          <span class="setting-label">亮度</span>
          <span class="bright-bar"><span class="bright-fill" data-bright-fill style="width:80%"></span></span>
          <span class="setting-value" data-brightness-value>80%</span>
        </button>
        <button type="button" class="setting-row setting-sleep" data-od-id="setting-sleep">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 4.6V8l2.3 1.5"/></svg></span>
          <span class="setting-label">休眠</span>
          <span class="setting-value" data-sleep-value>30秒</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-theme" data-od-id="setting-theme">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M12.6 9.6A5.6 5.6 0 1 1 6.4 3.4a4.6 4.6 0 0 0 6.2 6.2z"/></svg></span>
          <span class="setting-label">主题</span>
          <span class="setting-value theme-value">浅色</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-reset" data-od-id="setting-reset">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M13 8a5 5 0 1 1-1.6-3.6"/><path d="M13 2.6v2.4h-2.4"/></svg></span>
          <span class="setting-label">重置连接</span>
          <span class="setting-value" data-reset-value>执行</span>
          <span class="setting-chev">›</span>
        </button>
      </div>
    </div>

    <!-- 底部导航点 -->
    <div class="nav-dots" data-od-id="nav-dots">
      <button type="button" class="dot" data-page="0" aria-label="主页"></button>
      <button type="button" class="dot" data-page="1" aria-label="计算器"></button>
      <button type="button" class="dot active" data-page="2" aria-label="设置"></button>
    </div>
  </div></div></div><p class="g-caption">亮度 / 休眠 / 主题 / 重置连接</p></div>
<div class="gallery-item"><div class="g-title">主页<span class="g-tag">深色</span></div><div class="g-box"><div class="g-wrap"><div class="screen dark">
    <!-- Page 0 · 主页（时钟 + 键盘模式） -->
    <div class="page page-home hidden" data-od-id="page-home">
      <div class="home-left">
        <div class="home-status" aria-hidden="true">
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="1.5" y="5.5" width="11" height="5" rx="1.4"/><path d="M14 7.5v1" stroke-linecap="round"/><rect x="3" y="7" width="5.5" height="2" rx="0.6" fill="currentColor" stroke="none"/></svg>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M3 6.2c2.7-2.3 7.3-2.3 10 0"/><path d="M5 8.9c1.7-1.4 4.3-1.4 6 0"/><circle cx="8" cy="11.8" r="0.9" fill="currentColor" stroke="none"/></svg>
        </div>
        <div class="home-time">14:30</div>
        <div class="home-date">2026.08.04 周二</div>
      </div>
      <div class="home-right">
        <button type="button" class="mode-btn active" data-mode="USB" data-od-id="mode-usb">USB</button>
        <button type="button" class="mode-btn" data-mode="蓝牙" data-od-id="mode-bluetooth">蓝牙</button>
        <button type="button" class="mode-btn" data-mode="RF" data-od-id="mode-rf">RF</button>
      </div>
    </div>

    <!-- Page 1 · 计算器（纯运算过程显示，无屏幕按键） -->
    <div class="page page-calc hidden" data-od-id="page-calc">
      <div class="calc-display" aria-label="计算过程与结果" data-od-id="calc-readout">
        <div class="calc-proc" data-calc-proc></div>
        <div class="calc-result" data-calc-result>0</div>
      </div>
    </div>

    <!-- Page 2 · 设置 -->
    <div class="page page-settings hidden" data-od-id="page-settings">
      <div class="settings-list">
        <button type="button" class="setting-row setting-bright" data-od-id="setting-bright">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="2.8"/><path d="M8 1.6v1.6M8 12.8v1.6M1.6 8h1.6M12.8 8h1.6M3.6 3.6l1.1 1.1M11.3 11.3l1.1 1.1M12.4 3.6l-1.1 1.1M4.7 11.3l-1.1 1.1"/></svg></span>
          <span class="setting-label">亮度</span>
          <span class="bright-bar"><span class="bright-fill" data-bright-fill style="width:80%"></span></span>
          <span class="setting-value" data-brightness-value>80%</span>
        </button>
        <button type="button" class="setting-row setting-sleep" data-od-id="setting-sleep">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 4.6V8l2.3 1.5"/></svg></span>
          <span class="setting-label">休眠</span>
          <span class="setting-value" data-sleep-value>30秒</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-theme" data-od-id="setting-theme">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M12.6 9.6A5.6 5.6 0 1 1 6.4 3.4a4.6 4.6 0 0 0 6.2 6.2z"/></svg></span>
          <span class="setting-label">主题</span>
          <span class="setting-value theme-value">浅色</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-reset" data-od-id="setting-reset">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M13 8a5 5 0 1 1-1.6-3.6"/><path d="M13 2.6v2.4h-2.4"/></svg></span>
          <span class="setting-label">重置连接</span>
          <span class="setting-value" data-reset-value>执行</span>
          <span class="setting-chev">›</span>
        </button>
      </div>
    </div>

    <!-- 底部导航点 -->
    <div class="nav-dots" data-od-id="nav-dots">
      <button type="button" class="dot active" data-page="0" aria-label="主页"></button>
      <button type="button" class="dot" data-page="1" aria-label="计算器"></button>
      <button type="button" class="dot" data-page="2" aria-label="设置"></button>
    </div>
  </div></div></div><p class="g-caption">反向 token · 黑底白字待机</p></div>
<div class="gallery-item"><div class="g-title">计算器<span class="g-tag">深色</span></div><div class="g-box"><div class="g-wrap"><div class="screen dark">
    <!-- Page 0 · 主页（时钟 + 键盘模式） -->
    <div class="page page-home" data-od-id="page-home">
      <div class="home-left">
        <div class="home-status" aria-hidden="true">
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="1.5" y="5.5" width="11" height="5" rx="1.4"/><path d="M14 7.5v1" stroke-linecap="round"/><rect x="3" y="7" width="5.5" height="2" rx="0.6" fill="currentColor" stroke="none"/></svg>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M3 6.2c2.7-2.3 7.3-2.3 10 0"/><path d="M5 8.9c1.7-1.4 4.3-1.4 6 0"/><circle cx="8" cy="11.8" r="0.9" fill="currentColor" stroke="none"/></svg>
        </div>
        <div class="home-time">14:30</div>
        <div class="home-date">2026.08.04 周二</div>
      </div>
      <div class="home-right">
        <button type="button" class="mode-btn active" data-mode="USB" data-od-id="mode-usb">USB</button>
        <button type="button" class="mode-btn" data-mode="蓝牙" data-od-id="mode-bluetooth">蓝牙</button>
        <button type="button" class="mode-btn" data-mode="RF" data-od-id="mode-rf">RF</button>
      </div>
    </div>

    <!-- Page 1 · 计算器（纯运算过程显示，无屏幕按键） -->
    <div class="page page-calc" data-od-id="page-calc">
      <div class="calc-display" aria-label="计算过程与结果" data-od-id="calc-readout">
        <div class="calc-proc" data-calc-proc>128 + 64 × 5</div>
        <div class="calc-result" data-calc-result>448</div>
      </div>
    </div>

    <!-- Page 2 · 设置 -->
    <div class="page page-settings hidden" data-od-id="page-settings">
      <div class="settings-list">
        <button type="button" class="setting-row setting-bright" data-od-id="setting-bright">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="2.8"/><path d="M8 1.6v1.6M8 12.8v1.6M1.6 8h1.6M12.8 8h1.6M3.6 3.6l1.1 1.1M11.3 11.3l1.1 1.1M12.4 3.6l-1.1 1.1M4.7 11.3l-1.1 1.1"/></svg></span>
          <span class="setting-label">亮度</span>
          <span class="bright-bar"><span class="bright-fill" data-bright-fill style="width:80%"></span></span>
          <span class="setting-value" data-brightness-value>80%</span>
        </button>
        <button type="button" class="setting-row setting-sleep" data-od-id="setting-sleep">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 4.6V8l2.3 1.5"/></svg></span>
          <span class="setting-label">休眠</span>
          <span class="setting-value" data-sleep-value>30秒</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-theme" data-od-id="setting-theme">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M12.6 9.6A5.6 5.6 0 1 1 6.4 3.4a4.6 4.6 0 0 0 6.2 6.2z"/></svg></span>
          <span class="setting-label">主题</span>
          <span class="setting-value theme-value">浅色</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-reset" data-od-id="setting-reset">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M13 8a5 5 0 1 1-1.6-3.6"/><path d="M13 2.6v2.4h-2.4"/></svg></span>
          <span class="setting-label">重置连接</span>
          <span class="setting-value" data-reset-value>执行</span>
          <span class="setting-chev">›</span>
        </button>
      </div>
    </div>

    <!-- 底部导航点 -->
    <div class="nav-dots" data-od-id="nav-dots">
      <button type="button" class="dot" data-page="0" aria-label="主页"></button>
      <button type="button" class="dot active" data-page="1" aria-label="计算器"></button>
      <button type="button" class="dot" data-page="2" aria-label="设置"></button>
    </div>
  </div></div></div><p class="g-caption">过程行反白 · 结果高亮</p></div>
<div class="gallery-item"><div class="g-title">设置<span class="g-tag">深色</span></div><div class="g-box"><div class="g-wrap"><div class="screen dark">
    <!-- Page 0 · 主页（时钟 + 键盘模式） -->
    <div class="page page-home" data-od-id="page-home">
      <div class="home-left">
        <div class="home-status" aria-hidden="true">
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="1.5" y="5.5" width="11" height="5" rx="1.4"/><path d="M14 7.5v1" stroke-linecap="round"/><rect x="3" y="7" width="5.5" height="2" rx="0.6" fill="currentColor" stroke="none"/></svg>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M3 6.2c2.7-2.3 7.3-2.3 10 0"/><path d="M5 8.9c1.7-1.4 4.3-1.4 6 0"/><circle cx="8" cy="11.8" r="0.9" fill="currentColor" stroke="none"/></svg>
        </div>
        <div class="home-time">14:30</div>
        <div class="home-date">2026.08.04 周二</div>
      </div>
      <div class="home-right">
        <button type="button" class="mode-btn active" data-mode="USB" data-od-id="mode-usb">USB</button>
        <button type="button" class="mode-btn" data-mode="蓝牙" data-od-id="mode-bluetooth">蓝牙</button>
        <button type="button" class="mode-btn" data-mode="RF" data-od-id="mode-rf">RF</button>
      </div>
    </div>

    <!-- Page 1 · 计算器（纯运算过程显示，无屏幕按键） -->
    <div class="page page-calc hidden" data-od-id="page-calc">
      <div class="calc-display" aria-label="计算过程与结果" data-od-id="calc-readout">
        <div class="calc-proc" data-calc-proc></div>
        <div class="calc-result" data-calc-result>0</div>
      </div>
    </div>

    <!-- Page 2 · 设置 -->
    <div class="page page-settings" data-od-id="page-settings">
      <div class="settings-list">
        <button type="button" class="setting-row setting-bright" data-od-id="setting-bright">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="2.8"/><path d="M8 1.6v1.6M8 12.8v1.6M1.6 8h1.6M12.8 8h1.6M3.6 3.6l1.1 1.1M11.3 11.3l1.1 1.1M12.4 3.6l-1.1 1.1M4.7 11.3l-1.1 1.1"/></svg></span>
          <span class="setting-label">亮度</span>
          <span class="bright-bar"><span class="bright-fill" data-bright-fill style="width:80%"></span></span>
          <span class="setting-value" data-brightness-value>80%</span>
        </button>
        <button type="button" class="setting-row setting-sleep" data-od-id="setting-sleep">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 4.6V8l2.3 1.5"/></svg></span>
          <span class="setting-label">休眠</span>
          <span class="setting-value" data-sleep-value>30秒</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-theme" data-od-id="setting-theme">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M12.6 9.6A5.6 5.6 0 1 1 6.4 3.4a4.6 4.6 0 0 0 6.2 6.2z"/></svg></span>
          <span class="setting-label">主题</span>
          <span class="setting-value theme-value">浅色</span>
          <span class="setting-chev">›</span>
        </button>
        <button type="button" class="setting-row setting-reset" data-od-id="setting-reset">
          <span class="setting-icon" aria-hidden="true"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M13 8a5 5 0 1 1-1.6-3.6"/><path d="M13 2.6v2.4h-2.4"/></svg></span>
          <span class="setting-label">重置连接</span>
          <span class="setting-value" data-reset-value>执行</span>
          <span class="setting-chev">›</span>
        </button>
      </div>
    </div>

    <!-- 底部导航点 -->
    <div class="nav-dots" data-od-id="nav-dots">
      <button type="button" class="dot" data-page="0" aria-label="主页"></button>
      <button type="button" class="dot" data-page="1" aria-label="计算器"></button>
      <button type="button" class="dot active" data-page="2" aria-label="设置"></button>
    </div>
  </div></div></div><p class="g-caption">深色列表 · 反白激活项</p></div>
  </div>
</div>

</details>

完整可交互预览（点击翻页 / 切换主题 / 模拟计算器输入）请打开 [`Reference/numpad-ui-preview.html`](Reference/numpad-ui-preview.html)。

---

### 5.9 自定义上位机（2026-08-02 已实现基础版）

标准 VIAL 无法实现的屏幕功能（自定义显示字符、RTC 校时等），通过**自定义上位机 + raw HID** 实现。

#### 5.9.1 通信机制

- **复用 USB raw HID**（EP3，usage page `0xFF60`，VIAL 同一通道，VID 0x9273 / PID 0x9157）
- 自定义命令协议：`0xFE` 前缀 + 子命令 `0xE1~0xE5`（避开 VIAL 的 0x00~0x0D）
- 固件端处理位于 `APP/USB_MODE.c` `DevEP3_OUT_Deal`

#### 5.9.2 功能清单

| 功能 | 命令 | 说明 | 状态 |
|------|------|------|------|
| **RTC 校时** | `0xE1` | 上位机发送日期时间 → `RTC_InitTime()` 写入 | ✅ 已实现 |
| **屏幕自定义字符** | `0xE2`/`0xE3` | 发送文字 → 存 EEPROM(0x3F10) → 首页显示 | ✅ 已实现 |
| **屏幕亮度调节** | `0xE4` | 背光开关（GPIO，PWM 后续） | ✅ 已实现 |
| **诊断信息回读** | `0xE5` | 回传 RTC 时间 | ✅ 已实现 |
| **固件配置读写** | — | 模式切换、参数读写 | 规划 |

#### 5.9.3 协议（已实现）

```
校时命令:    FE E1 [年2B][月][日][时][分][秒]     → 固件写 RTC，回 [E1][01]
设屏文字:    FE E2 [len][ASCII 文字...]          → 固件存 EEPROM 并显示，回 [E2][len]
读屏文字:    FE E3                              → 固件回 [E3][len][文字]
背光:        FE E4 [level 0-255]                → 回 [E4]
诊断:        FE E5                              → 回 [E5][年2B][月][日][时][分][秒]
```

#### 5.9.4 上位机工具（`tools/ch582_host.py`）

Python 工具，基于 `hidapi`，枚举 raw HID 并发送命令：

```bash
pip install hidapi
python tools/ch582_host.py time "2026-08-02 12:34:56"   # RTC 校时
python tools/ch582_host.py text "FinPad22"                # 设屏幕文字（≤15 字符）
python tools/ch582_host.py get-text                       # 读屏幕文字
python tools/ch582_host.py diag                           # 读 RTC 时间
python tools/ch582_host.py brightness 128                 # 背光
```

#### 5.9.5 待实现

- **固件端**：TMR2 PWM 背光调光、固件配置读写
- **上位机**：Web/图形界面版

---

<h2 id="sec6">六、参考资源</h2>

- oshwhub 原项目：[基于CH582M的三模兼容VIAL改键小键盘](https://oshwhub.com/bluetooth-keyboard-squad/the-first-stop-of-the-three-mode-keyboard)
- WCH 官网：http://www.wch.cn （CH582 数据手册、MounRiver Studio、BLE 库说明）

---

<details>
<summary><b>七、调试记录（2026-07-29 ~ 08-02，USB/VIAL/屏幕调试历史）— 点击展开</b></summary>

<h2 id="sec7">七、调试记录（2026-07-29 ~ 2026-07-30）：USB 枚举 / VIAL 启动 / 标准 Vial 协议实现 / 三模切换</h2>

### 7.1 问题现象

固件烧录到 **WeAct WCH-BLE-Core 核心板**（CH582F）后，连接电脑无任何反应，无法枚举为 USB 设备。经完整排查，共发现以下根因（按定位顺序）：

| 序号 | 根因 | 章节 | 状态 |
|------|------|------|------|
| 1 | USB 控制器用错（USB2 → USB1） | §7.2 | ✅ 已修复 |
| 2 | `DEBUG=1` → USB ISR 内 printf 阻塞 | §7.11 | ✅ 已修复（真根因） |
| ~~2~~ | ~~EEPROM 时序冲突~~ | §7.3 | ❌ 已证伪（在 DEBUG=1 下误判） |
| ~~3~~ | ~~PB12/PB13 USB2 引脚冲突~~ | §7.10 | ❌ 已证伪（在 DEBUG=1 下误判） |

> §7.3 和 §7.10 保留供回溯参考，但所有结论均已被 §7.11 推翻。

### 7.2 根因一：USB 控制器用错（USB2 → USB1）

**板子硬件**（WeAct WCH-BLE-Core，原理图 `HDK/WeAct-CH57xCH58xCoreBoard_V10_SchDoc.pdf`）：

- USB 座子 DP1 = **PB11**、DN1 = **PB10**，对应 CH582 的 **USB1**（UD+/UD-）。
- CH582 的 **USB2**（U2D+/U2D-）在 PB13/PB12，是另一组独立引脚，板子上未接。

**原固件**：`APP/USB_MODE.c` 全程使用 **USB2** 控制器（`R8_USB2_*` / `USB2_DeviceInit` / `USB2_IRQHandler` / `pU2EP*`），D+/D- 拉在 PB12/PB13，与板子 USB 座子（PB10/PB11）物理不通，故永远无法枚举。

**修复**：把 `APP/USB_MODE.c` 从 USB2 整体移植到 USB1。CH582 两套 USB 控制器引脚固定、不可软件重映射，只能改代码。共 43 处符号替换：

| 类别 | USB2（原） | USB1（改后） |
|---|---|---|
| 寄存器 | `R8_USB2_INT_FG/ST/EN`、`R8_USB2_DEV_AD`、`R8_USB2_RX_LEN`、`R8_USB2_MIS_ST` | `R8_USB_INT_FG/ST/EN`、`R8_USB_DEV_AD`、`R8_USB_RX_LEN`、`R8_USB_MIS_ST` |
| 端点寄存器 | `R8_U2EPx_CTRL`、`R8_U2EPx_T_LEN`、`R8_U2DEV_CTRL` | `R8_UEPx_CTRL`、`R8_UEPx_T_LEN`、`R8_UDEV_CTRL` |
| 缓冲区指针 | `pU2EP*_RAM_Addr`、`pU2EP*_DataBuf`、`pU2SetupReqPak` | `pEP*_RAM_Addr`、`pEP*_DataBuf`、`pSetupReqPak` |
| 上拉 | `RB_PIN_USB2_DP_PU` | `RB_PIN_USB_DP_PU` |
| SDK 函数 | `USB2_DeviceInit`、`U2DevEPx_IN_Deal` | `USB_DeviceInit`、`DevEPx_IN_Deal` |
| 应用函数 | `USB2_DevTransProcess`、`U2DevEPx_OUT_Deal` | `USB_DevTransProcess`、`DevEPx_OUT_Deal` |
| 中断 | `USB2_IRQHandler`、`USB2_IRQn` | `USB_IRQHandler`、`USB_IRQn`（startup 中 USB1 向量名即 `USB_IRQHandler`） |

> USB1 的 SDK（`CH58x_usbdev.c`）只提供 `USB_DeviceInit` 与 `DevEPx_IN_Deal`；`USB_DevTransProcess`、`DevEPx_OUT_Deal` 由应用层实现，移植后与头文件声明一致，无链接冲突。
> 移植后 USB1 的 D+/D- 落在 PB10/PB11，与板子 USB 座子一致。Bus Hound 抓包确认设备/配置/字符串描述符均正确返回，`SET CONFIG` 完成，枚举正常。

### 7.3 ~~根因二：Flash / EEPROM 操作与 USB 控制器的时序冲突~~ ❌ 已证伪

> **⚠️ 此节全部调试（7.3.1~7.3.4）均在 `DEBUG=1` 编译条件下完成，结论已被推翻。**
> **真正根因：`.cproject` 中 `DEBUG=1` → `PRINT=printf` → USB ISR 内 printf 阻塞中断 → `RB_UC_INT_BUSY` 自动 NAK 死锁。详见 §7.11。**
>
> 以下保留原始调试过程供回溯参考，但所有 EEPROM 时序结论均不成立。

- **现象**：USB1 修复后，若恢复原始 `vial_init()` 流程，又无法枚举。
- **原因**：USB ISP 烧录会整片擦除 flash，vial 数据区为空（0xFF）。`vial_init()`（在预编译库 `APP/libVIAL.a` 中）对空 flash 做校验，**校验失败时内部卡死/复位、不返回**——已验证：即便加 `if(非法模式) key_mode=0x0B` 兜底仍枚举不了，说明它根本没走到返回。
- **关键推理**：原工程切 BLE/2.4G 时（`USB_MODE.c` 的 `TMR3_IRQHandler`）就是用 `FLASH_DATA_VIAL_WITE_mode({0xBE/0x24})` 写模式后复位，下次开机 `vial_init()` 能读到该模式并进入对应模式。说明 **`FLASH_DATA_VIAL_WITE_mode` 写入的模式是 `vial_init()` 能识别的合法模式**。

#### 7.3.1 真根因：EEPROM 操作必须在 `USB_DeviceInit()` 之后（2026-07-31 最终修正）

经过 10+ 次二分法递减测试，排除了"读写顺序""缓冲区对齐""32位/8位类型""volatile""SW_RESET""调用次数"等所有假设，最终定位到真正根因：

> **在 `USB_DeviceInit()` 之前，只能做恰好 1 次 `EEPROM_READ(addr, &uint8_t_var, 1)` 操作。任何额外的 EEPROM 访问（即使是同一个地址、同一个变量、只多读 1 字节）都会导致 USB 枚举失败。**

完整的测试矩阵（全部为 ISP 烧录后空 flash 冷启动）：

| Commit | EEPROM 操作 | USB_DeviceInit 时序 | 结果 |
|--------|------------|-------------------|------|
| `f6e63b7` | 0 次 | — | ✅ |
| `6bc5682` | 1 次 `READ(0x3F00, &uint8_t, 1)` | 在读之后 | ✅ |
| `005ac45` | 1 次 `READ(0x3F00, &uint32_t, 1)` | 在读之后 | ❌ |
| `1b040d0` | 2 次 `READ(0x3F00, &uint8_t, 1)` (同地址同变量) | 在读之后 | ❌ |
| `20e35e6` | 2 次 `READ` 到 2 个 `uint8_t` | 在读之后 | ❌ |
| `1a840de` | 1 次 `READ(0x3F00, uint8_t[2], 2)` | 在读之后 | ❌ |
| `0b7b20c` | 2 次 + volatile + SW_RESET | 在读之后 | ❌ |
| **`58bda19`** | 1 次 mode 读 → **`USB_DeviceInit()`** → 1 次 magic 读 | **读写分离** | ✅ |

**结论**：在 `USB_DeviceInit()` 之前只能做绝对最小化的 EEPROM 操作（1 次 1 字节 uint8_t 读 + 1 次写）。

> **之前 7.3.1 的分析（"FLASH_DATA_VIAL_WITE_mode 改变 flash 控制器状态"）已被证伪。** 问题不是写函数改变了状态，而是多个 EEPROM 读操作在 USB 控制器初始化之前执行会破坏 USB PHY/时钟的某些初始化状态。具体硬件层面原因未知（`libISP583.a` 为闭源库），但软件层面的规避方案已确定。

#### 7.3.2 两阶段修复（第一版，`commit 58bda19`，2026-07-31）

Phase 1（`USB_DeviceInit()` 之前）— 仅读 mode byte + 写默认模式；
Phase 2（`USB_DeviceInit()` 之后、IRQ 使能之前）— 所有键码加载。

✅ 枚举通过，但 **后续发现 Vial 无法通信**（见 §7.3.3）。

#### 7.3.3 ~~修正：EEPROM 读取必须在 USB 中断使能之后~~ ❌ 同样已证伪（commit `c8aa8c9`）

`58bda19` 虽然 USB 枚举成功，但 Bus Hound 抓包发现：
- USB 枚举（描述符 / SET CONFIG）正常
- **SET REPORT**（Windows 设置键盘 LED 状态）数据阶段延迟 ~5 秒
- 此后**所有控制传输超时**（GET DESCRIPTOR → `c0010000` CANCELED）
- **Vial raw HID 通信完全未启动**（EP3 OUT 无任何 0xFE 命令）

根因：EEPROM 读取放在 `USB_DeviceInit()` 之后、`PFIC_EnableIRQ(USB_IRQn)` **之前**，导致 `RB_UC_INT_BUSY` 机制的 USB 中断标志在 ISR 启用前粘滞（stuck pending），设备进入自动 NAK 死锁状态。

**最终方案** — 三阶段（`APP/hidkbd_main.c`）：

| 阶段 | 时机 | 操作 |
|------|------|------|
| **Phase 1** | `USB_DeviceInit()` 之前 | 1 次 `EEPROM_READ(0x3F00, &uint8_t, 1)` + `FLASH_DATA_VIAL_WITE_mode`（仅空 flash） |
| **Phase 2** | `USB_DeviceInit()` 之后 → **立即 IRQ 使能** | `PFIC_EnableIRQ(USB_IRQn)` → `TMR3_TimerInit` → `PFIC_EnableIRQ(TMR3_IRQn)` |
| **Phase 3** | **USB 子系统完全就绪后** | `load_keymap_from_flash()`：magic byte 检查 + 4 层键码加载 |

```c
// Phase 1: Before USB_DeviceInit — minimal EEPROM ops
EEPROM_READ(0x3F00, &mode, 1);     // read mode byte
if (invalid) FLASH_DATA_VIAL_WITE_mode(&mode);  // write default 0x0B

Scan_init();                        // GPIO-only
USB_DeviceInit();

// Phase 2: USB IRQ enabled IMMEDIATELY (no EEPROM ops in between!)
PFIC_EnableIRQ(USB_IRQn);
TMR3_TimerInit(90000);
TMR3_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
PFIC_EnableIRQ(TMR3_IRQn);

// Phase 3: Keymap load AFTER USB subsystem is fully operational
load_keymap_from_flash();
Main_Circulation_USB();
```

**关键洞察**：Phase 2 证明"EEPROM 读取放在 IRQ 使能之前安全"的假设是**错误的**。正确做法是让 USB 中断在 EEPROM 读取之前就绪——宿主已完成枚举、总线空闲，且任何后续 USB 事件都会由 ISR 及时清除中断标志。

> **`load_keymap_from_flash()` 中的 flash 操作可能短暂关闭全局中断，此时若有 USB 事件会被延迟到 flash 操作结束后由 ISR 处理。USB Full-Speed 对 100μs 级的中断延迟容忍度很高（NAK 重试机制），不影响通信。**

#### 7.3.4 ~~`Scan_init()` 约束~~ ❌ 结论无效（同属 DEBUG=1 误判）

`Scan_init()` 保持 GPIO-only（只配置 row/col 方向），无任何 flash 操作。

✅ **2026-07-31 最终验证通过**：移除 `DEBUG=1` 后（§7.11），Vial 桌面版完全可用——连接 → 识别布局 → 键值编辑均正常。详见 §7.12。

### 7.4 已知行为 / 副作用

- **跳过 `vial_init()` + 手动设 `vial_key_done=1`**：`vial_init()` 在空 flash 下校验 `0x3E00`/`0x7F018` 失败后死循环，与 mode 字节无关。故彻底不调它，改为手动设 `vial_key_done=1`（使 `FLASH_DATA_VIAL_WITE_mode`/`FLASH_DATA_KEY` 等所有 vial 库函数可用）。✅ **已测试验证：空 flash 首次上电可枚举。**（注：当前 `main()` 固定 USB、不读模式字节，见 §4。）
- **EEPROM 访问时序约束**：§7.3 的相关结论已被证伪。移除 `DEBUG=1` 后（§7.11），EEPROM 访问约束大幅放宽。当前最小化测试版本（§7.12）未做 EEPROM 读取，待功能恢复后再确定新的约束。
- **核心板无按键矩阵**：WeAct 核心板是裸 MCU，没有矩阵，故枚举后按键无输出属正常；接上小键盘矩阵（见第五节 5.1）后才会有键值。
- **键值表持久化**：键值表配置后，开机不再写 `0x0B`（仅空 flash 才写），故不会擦键值表。仍建议用 VIAL 上位机写一次键位后断电重启确认键位在。

### 7.5 `.bin` 生成（USB ISP 烧录用）

工程默认 `Create flash image` 输出格式为 **ihex**（`.hex`），见 `.cproject` 的 `createflash.choice`。用 WCHISPTool 做 USB ISP 烧录需要 `.bin`，二选一：

- **GUI**：Project → Properties → C/C++ Build → Settings → Build Steps → `Create flash image` 的 `Output file format (-O)` 改为 `binary`，重新 Build 即产出 `obj/CH582_VIAL_PAD.bin`。
- **命令行**：编译出 `.elf` 后用工具链转换 `riscv-none-embed-objcopy -O binary obj/CH582_VIAL_PAD.elf obj/CH582_VIAL_PAD.bin`。

> 烧录时 `.bin` 起始地址为 `0x0000`（应用区起始）。ISP 烧录会整片擦除 flash（含 vial 数据区），故每次 ISP 烧录后均为"空 flash 冷启动"，由 §7.4 的 `vial_key_done=1` 修复保证仍能枚举。

### 7.6 根因三：vial.rocks 无法识别（固件为自定义 Vial 协议，非标准 Vial）

- **现象**：USB 枚举正常、vial.rocks 也能 WebUSB 配对，但连接后提示 "No devices detected"，无法显示布局/改键。
- **根因**：`APP/USB_MODE.c` 的 `DevEP3_OUT_Deal`（VIAL 端点处理）是**自定义协议**，不是标准 Vial 协议：
  - 只处理 `0x01`（开始批量传输）、`0x05`（设键）、`0x12`（读键值页），其余命令**原样回显**。
  - 标准 Vial 连接后首发 `get_keyboard_id(0x00)` / `get_size(0x01)` / `get_def(0x02)` 均未实现（`0x00` 被回显、`0x01` 被当作"开始传输"、`0x02` 未作定义读取），vial.rocks 拿不到键盘 UID/定义，判定非合法 Vial 键盘。
  - HID 描述符（usage page `0xFF60`、32B in/out）是标准 Vial 的，故 WebUSB 能配对；但协议层握手失败。
- **非移植丢失**：对比 KBD 参考工程，其 `U2DevEP3_OUT_Deal` 与本工程完全相同，全工程无任何标准 Vial 标识（`keyboard_definition` / `VIAL_KEYBOARD_UID` / `vial_get_keyboard` 等均无）。即 KBD 原工程用的也是自定义协议，vial.rocks 对它同样不适用。
- **自定义协议摘要**（供后续实现参考）：
  - `0x12`（offset 0 / 0x1c）：读键值表两页，返回 `key_data_buf`
  - `0x05` `[05,layer,row,col,mode,keycode]`：设单个键，写 `key_data_buf` 并存 flash（`Debonding_layer_cfg`）
  - `0x01` + 77 包 × 32B：批量读写 vial data flash

- **已修复（2026-07-30）**：✅ 已将 `DevEP3_OUT_Deal` 重写为标准 VIA/Vial 协议，内嵌 LZMA 压缩键盘定义，详见 6.8 节。`libVIAL.a` 只提供 flash 存储，不含 Vial 协议。

### 7.7 三模互切补全：USB 切换键

- **现象**：原 `USB_MODE.c` 的 `TMR3_IRQHandler` 中，`key_data_buf[1][0]`（USB 切换键）只有 `//USB MODE` 注释、**没有写 `0x0B` 的逻辑**，导致 USB 模式下无法切回 USB（BLE/2.4G 切换键正常）。`BLE_MODE.c` / `RF_MODE.c` 本就有 USB 切换（写 `0x0B`）。
- **修复**（`APP/USB_MODE.c` 的 `TMR3_IRQHandler`）：给 USB 切换键补上 `change_mode_USB++`，并加 `if (change_mode_USB == 1500)` 写 `0x0B` 后复位（与 BLE/2.4G 同阈值 ~2.25s）；两处计数器复位同步加 `change_mode_USB = 0`。
- **配合 §7.4**：切换键 `key_data_buf[2][0/1/2]`（物理键 7/8/9）默认键值已配为 **KP_7/8/9**（见 §5.1），无需先经 VIAL 配置即可长按触发。⚠️ 当前 `main()` 固定 USB 模式、开机不读模式字节（见 §4），写模式字节复位后仍回 USB，待 `main()` 分支接线后生效。

### 7.8 标准 Vial 协议实现与调试（2026-07-30）

将自定义协议重写为标准 VIA/Vial 协议，使 Vial 桌面应用可识别。以下是调试过程中遇到的所有问题及修复。

#### 7.8.1 协议架构

标准 Vial 协议通过 raw HID 接口（usage page `0xFF60`，EP2 IN / EP3 OUT，32 byte/包）通信：

- **VIA 命令**（byte 0 = 0x01–0x0D）：协议版本、键值读写、宏、灯光等
- **Vial 命令**（byte 0 = `0xFE`，byte 1 = 子命令 0x00–0x0D）：键盘识别、定义传输、解锁、QMK 设置等

每个命令收到后**必须在同一中断内**将 32 字节响应写入 `pEP2_IN_DataBuf` 并调用 `DevEP2_IN_Deal(32)`。

#### 7.8.2 VIA 协议版本字节序（大端序）

- **问题**：Vial 桌面版连接后提示 `Unsupported protocol version!`
- **根因**：VIA 协议用**大端序**存储 16-bit 版本号。QMK 实现为 `msg[1]=hi, msg[2]=lo`。本工程写成了小端序 `msg[1]=lo, msg[2]=hi`，桌面版读到版本 `0x0900` (2304) 而非 `0x0009` (9)。
- **修复**：`APP/USB_MODE.c` `VIA_GET_PROTOCOL_VERSION` 处理中交换 bytes 1/2 顺序。

#### 7.8.3 Vial 响应格式：有无 0xFE 前缀

参考 Vial 固件源码（`quantum/vial.c`）后发现不同命令的响应格式不同：

| 命令 | 响应格式 |
|---|---|
| `GET_KEYBOARD_ID` (0x00) | `[0xFE, 0x00, pv(4B LE), uid(8B), flags]` — **保留前缀**，数据从 msg[2] 开始 |
| `GET_SIZE` (0x01) | `[0xFE, 0x01, sz(4B LE)]` — **保留前缀**，数据从 msg[2] 开始 |
| `GET_DEFINITION` (0x02) | `[def_data…]` — **无前缀**，直接覆盖 msg[0..31] |
| 其他 Vial 命令 | **保留前缀**，数据从 msg[2] 开始 |

- **修复**：除 `GET_DEFINITION` 直接覆写整个缓冲区外，其他 Vial 命令保留 0xFE 前缀和命令字节。

#### 7.8.4 GET_DEFINITION 页面索引格式

- **问题**：定义数据解压失败 `LZMAError: Compressed data ended before the end-of-stream marker`
- **根因**：Vial 协议用 **16-bit page 索引**（`msg[2..3]`），每页 = 32 字节。本工程用成了 32-bit offset + 30 字节/页。
- **修复**：`uint32_t page = msg[2] | (msg[3] << 8)`，每页精确 32 字节，无前缀。

#### 7.8.5 LZMA 压缩格式兼容性

- **问题**：vial.rocks（浏览器版）始终报 `emscripten_sleep` 错误
- **根因**：Python `lzma.compress()` 默认使用 XZ 容器（`FORMAT_XZ`）和 4MB 字典，Emscripten 编译的 Pyodide（vial.rocks 后端）在处理大字典时触发 WASM 异步限制。
- **方案**：改用 QMK Vial 完全相同的格式 — FORMAT_ALONE（legacy .lzma）+ LZMA1 filter + preset=4。vial.rocks 网页版仍会失败（Pyodide 限制），但 **Vial 桌面版正常工作**（原生 Python，无 WASM 限制）。

#### 7.8.6 键盘定义 32 字节对齐

- **问题**：桌面版加载定义时 `LZMAError: Compressed data ended before end-of-stream marker`
- **根因**：LZMA 压缩数据长度不是 32 的倍数（252 bytes = 7.875 页），最后一页包含 memset 的尾部 0 字节。桌面版逐页取 32 字节后拼接，尾部 0 破坏 LZMA 流。
- **修复**：`_gen_vial_def.py` 在 JSON 末尾加 284 个空格使其压缩后恰好 = **256 bytes（8 整页）**，最后一页无尾部 0。

#### 7.8.7 QMK Settings 查询死循环

- **问题**：Vial 桌面版 `reload_settings()` 阶段超时 `RuntimeError: failed to communicate with the device`
- **根因**（Bus Hound 抓包确认）：桌面版发送 `CMD_VIAL_QMK_SETTINGS_QUERY` (0x09) 遍历 QMK 设置列表，固件回显了请求。桌面端从回显解析出 `qsid=0x09FE`（非 `0xFFFF` 结束标记），进入死循环直至超时。
- **修复**：`APP/USB_MODE.c` 添加 `0x09` 处理，返回 `[0xFF, 0xFF, …]` → 桌面端读到 `0xFFFF` → 循环立即终止。同时补全 `0x0A~0x0D` 命令的响应处理。

#### 7.8.8 默认键值 KC_NO

- **问题**：连接成功、定义加载成功、settings 同步成功后，keymap 编辑器崩溃 `KeyError: (0, 0, 0)`
- **根因**：`key_data_buf` 默认初始化为 `0x04`（'a' 键），空 flash 读回 `0xFF`。Vial 桌面端 `code_for_widget()` 按 `(layer, row, col)` 查找键值字典时，不认识 `0x04`/`0xFF` 对应的 HID 键值，跳过该位置，导致字典缺失该条目。
- **修复**：`HAL/scan_key.c` 默认键值改为 `0x00`（KC_NO = 无键）；`main()` 的 `load_keymap_from_flash()` 读 flash 时跳过 `0xFFFF`（未写位置保留编译默认值）。
- **后续（§5.1）**：默认键值已填充**财务小键盘布局**（R0=`(`/`)`/`=`/Tab，R1~R5=数字键盘），首次上电进入 USB 模式即可用 Vial 桌面版改键。

#### 7.8.9 实现文件清单

| 文件 | 说明 |
|---|---|
| `APP/include/vial_protocol.h` | VIA/Vial 命令常量、协议版本、键盘 UID（8 字节）、矩阵尺寸 |
| `APP/include/vial_definition.h` | LZMA 压缩的键盘定义（256 字节，8 页，自动生成） |
| `APP/USB_MODE.c` `DevEP3_OUT_Deal()` | 重写为标准 VIA/Vial 协议处理器（~130 行） |
| `HAL/scan_key.c` `Scan_init()` | 加载全部 4 层键值表 + 0xFF→0x00 转换 |
| `HAL/include/scan_key.h` | 补充 `key_data_buf_1/2/3` extern 声明 |
| `_gen_vial_def.py` | Python 脚本：从 `Reference/vial.json` 生成 LZMA 压缩 C 数组 |
| `Reference/vial.json` | 5×4 键盘布局定义（466 字节原始 JSON） |

#### 7.8.10 当前状态

- ✅ Vial 桌面版：连接正常、定义加载正常、QMK settings 同步正常、**键值编辑正常**
- ✅ 键盘布局可正确加载和编辑，所有 4 层 24 键位均可通过 Vial 桌面配置
- ✅ vial.rocks 网页版：实测可用（commit `17298da` 确认，移除 `DEBUG=1` 后重新验证通过）
- ✅ uint16_t keymap 升级完成：键值表支持 QMK 16-bit 修饰符键码（LSFT/LCTL 等组合键）
- ⚠️ 三模切换键（`key_data_buf[2][0/1/2]`）默认键值已配为 **KP_7/8/9**（见 §5.1），长按即触发写模式字节；因当前 `main()` 固定 USB（见 §4），开机暂不按模式分支
- ⚠️ 4 字节头 GET_BUFFER 响应已修正为 BE keycode（见 §7.9.6），2026-08-01 烧录验证通过，布局正确

---

### 7.9 终极调试：KeyError(0,0,0) 根因定位与修复（2026-07-30，commits `9bff6d9` → `a45fc5f`）

这是整个 Vial 协议实现中最耗时的一次调试，共经历 6 次 Bus Hound USB 抓包分析才定位到真正的根因。Vial 桌面端流程如下：

```text
reload_definition()      — FE 00 (UID) → FE 01 (size) → FE 02 (def)
reload_version()         — 01 (VIA 协议版本)
reload_layers()          — 11 (获取层数)        ← ★ 关键！
reload_macros_early()    — 0C/0D (宏)
reload_settings()        — FE 09 (QMK 设置)
reload_dynamic()         — FE 0D (动态条目)
reload_keymap()          — 12 (批量读键值表)    ← ★ 从未被执行！
```

#### 7.9.1 现象

键值编辑器崩溃：`KeyError: (0, 0, 0)`。`self.keyboard.layout` 字典为空，因为键值数据从未被加载。

#### 7.9.2 Bus Hound 分析：缺失命令

USB 抓包发现关键事实：

| 命令 | 出现次数 | 说明 |
|---|---|---|
| `11 00 00 00 00`（size=0 探针） | **1 次** | 桌面发来的唯一 0x11 请求 |
| `12 ...`（GET_KEYMAP_BUFFER） | **0 次** | 从未出现！键值批量读取从未执行 |
| `04 ...`（GET_KEYCODE 单个读） | **0 次** | 也从未出现 |

**结论**：`reload_keymap()` 根本没发送任何 HID 命令。循环迭代次数为 0。

#### 7.9.3 第一轮排查（误判，commit `9bff6d9` → `2f3c2e9`）

最初认为是 `size=0` 探针返回空数据导致桌面跳过。QMK 固件在 `size==0` 时返回总 keymap 大小（`layers × rows × cols × 2 = 192`），而我们返回了 `size=0`（空）。

**修复**（`via_get_buffer_resp`）：`size==0` → `size = 192` → cap 到 28 字节 → 返回实际键码数据。结果：探针返回 28 字节键码数据，桌面 `size` 字段非零。**但问题依旧 — `0x12` 仍未出现。**

#### 7.9.4 第二轮排查：查 Vial 桌面源码

查看 [Vial 桌面 constants.py](https://github.com/vial-kb/vial-gui/blob/main/src/main/python/protocol/constants.py) 发现：

```python
CMD_VIA_GET_LAYER_COUNT = 0x11    # ← 不是 DYNAMIC_KEYMAP_GET_BUFFER!
CMD_VIA_KEYMAP_GET_BUFFER = 0x12
```

**0x11 不是键值读取命令，是"获取层数"命令！** 桌面用 `0x11` 返回的值设置 `self.layers`。

#### 7.9.5 ★ 真正的根因（commit `a45fc5f`）

桌面发送 `11 00 00 00 00...` 想获取层数，期望响应 `[0x11, 0x04]`（4 层）。

我们的固件把 `0x11` 当作 keymap 读取命令，返回了 **32 字节 keymap 数据**：
```
11 00 00 1c 00 26 00 27 00 2e 00 2b 00 53 00 54 ...
↑cmd  ↑offset  ↑size ↑───── 14 个 16-bit HID 键码 ─────→
```

桌面端按层数响应解析：`data[1] = 0x00` → **`self.layers = 0`**。

然后 `reload_keymap()` 计算：
```python
total_size = self.layers * self.rows * self.cols * 2
           = 0 * 6 * 4 * 2
           = 0
```

循环 `for offset in range(0, 0, 28):` → **零次迭代** → 不发送任何 `0x12` 命令 → `self.keyboard.layout` 空 → `KeyError(0,0,0)`。

**修复**：
```c
// 0x11 → CMD_VIA_GET_LAYER_COUNT，返回层数
case 0x11: {
    pEP2_IN_DataBuf[0] = 0x11;
    pEP2_IN_DataBuf[1] = VIAL_LAYER_COUNT;  // 4
    break;
}
// 0x12 → 保持为 KEYMAP_GET_BUFFER
case VIA_KEYMAP_GET_BUFFER: { ... }
```

#### 7.9.6 GET_BUFFER 响应 header 格式修复 — 最终确认（2026-08-01）

**问题复现（`wroing.vil` vs `demo.vil`）：**

| 位置 | wroing.vil（错误） | demo.vil（正确） |
|------|-------------------|-----------------|
| R0C0 | `KC_9` (0x0026) | `LSFT(KC_9)` (0x0226) |
| R0C2 | `LSFT(KC_EQUAL)` (0x022E) | `KC_EQUAL` (0x002E) |

**根因分析：双重错误**

1. **Header 格式**：5 字节头多了一个 `size_hi` 字节，Vial 桌面按 QMK 标准 4 字节头解析，将 `size_hi` 当作 keycode 数据解析，导致偏移 1 字节。
2. **Keycode 字节序**：固件发送 keycode 时按 **little-endian**（`lo, hi`），但 Vial 桌面用 **big-endian**（`hi, lo`）解析。

**第一版修复（`a45fc5f`，5 字节头 + LE keycode，已烧录验证）：**

对比 QMK 源码发现 header 格式不同：

| 字段 | QMK 标准格式 | 本工程第一版修复 |
|---|---|---|
| offset | 2 字节 BE | 2 字节 LE ✗ |
| size | **1 字节 u8（28 bytes max）** | **2 字节 LE（多余 size_hi）** ✗ |
| keycode 起始偏移 | **byte 4** | **byte 5**（偏移了 1 字节！） |
| keycode 字节序 | **big-endian**（`hi, lo`） | **little-endian**（`lo, hi`）✗ |

第一版 5 字节头代码：
```c
pEP2_IN_DataBuf[1] = (uint8_t)(offset & 0xFF);        // offset lo (LE)
pEP2_IN_DataBuf[2] = (uint8_t)((offset >> 8) & 0xFF); // offset hi
pEP2_IN_DataBuf[3] = (uint8_t)(size & 0xFF);           // size lo (LE)
pEP2_IN_DataBuf[4] = (uint8_t)((size >> 8) & 0xFF);    // size hi ← 多余！
// keycodes 从 pEP2_IN_DataBuf[5] 开始，LE (lo, hi)
```

**烧录验证结果（2026-08-01）：**
- ✅ USB 枚举正常
- ✅ Vial 桌面通信正常（连接、识别、定义加载均通过）
- ❌ 布局显示错位：R0C0 显示 `KC_9` 而非 `LSFT(KC_9)`，R0C2 显示 `LSFT(KC_EQUAL)` 而非 `KC_EQUAL`

**最终修正（4 字节头 + BE keycode，已验证通过）：**

通过对比 `wroing.vil` 与 `demo.vil` 的字节级差异，确认 Vial 桌面用 **big-endian** 解析 keycode（`struct.unpack('>H', ...)`），与已存在的 `GET_KEYCODE(0x04)`/`SET_KEYCODE(0x05)` 处理一致。

```c
// QMK 标准 4 字节头 + big-endian 16-bit keycodes
pEP2_IN_DataBuf[0] = cmd;
pEP2_IN_DataBuf[1] = (uint8_t)((offset >> 8) & 0xFF); // offset hi (BE)
pEP2_IN_DataBuf[2] = (uint8_t)(offset & 0xFF);         // offset lo
pEP2_IN_DataBuf[3] = resp_bytes;                        // size u8（bytes, max 28）
// keycodes 从 pEP2_IN_DataBuf[4] 开始，每对 BE (hi, lo)
pEP2_IN_DataBuf[4 + i]     = (uint8_t)((kc >> 8) & 0xFF); /* hi (BE) */
pEP2_IN_DataBuf[4 + i + 1] = (uint8_t)(kc & 0xFF);         /* lo */
```

**5 字节头 → 错位分析（`wroing.vil` 的成因）：**

```
固件发送的 32 字节（5 字节头 + LE keycode）：
 Byte 0:  cmd = 0x12
 Byte 1:  offset_lo = 0x00        \
 Byte 2:  offset_hi = 0x00        /  Vial 按 4 字节头读取
 Byte 3:  size_lo  = 0x1C (28)    →  Vial 读作 size=28 bytes
 Byte 4:  size_hi  = 0x00         →  Vial 读作 R0C0 的第一个字节！← 错位起点
 Byte 5:  kc0_lo  = 0x26          \
 Byte 6:  kc0_hi  = 0x02          /  实际 R0C0 = 0x0226 = LSFT(KC_9)
 Byte 7:  kc1_lo  = 0x27          \
 Byte 8:  kc1_hi  = 0x02          /  实际 R0C1 = 0x0227 = LSFT(KC_0)
 Byte 9:  kc2_lo  = 0x2E          \
 Byte 10: kc2_hi  = 0x00          /  实际 R0C2 = 0x002E = KC_EQUAL
 Byte 11: kc3_lo  = 0x2B          \
 Byte 12: kc3_hi  = 0x00          /  实际 R0C3 = 0x002B = KC_TAB

Vial 按 4 字节头 + BE 解析后的 keycode 配对（全部错位 1 字节）：
 R0C0: bytes [4:5] = {0x00,0x26} → (0x00<<8)|0x26 = 0x0026 = KC_9      (不是组合键 ✗)
 R0C1: bytes [6:7] = {0x02,0x27} → (0x02<<8)|0x27 = 0x0227 = LSFT(KC_0) (正确 ✓)
 R0C2: bytes [8:9] = {0x02,0x2E} → (0x02<<8)|0x2E = 0x022E = LSFT(KC_EQUAL) (被「挤」成组合键 ✗)
 R0C3: bytes[10:11]= {0x00,0x2B} → (0x00<<8)|0x2B = 0x002B = KC_TAB    (正确)
```

此分析精确匹配 `wroing.vil` 的输出 → 证明：
- ✅ Vial 桌面用 **4 字节头**（QMK 标准）
- ✅ Vial 桌面用 **big-endian** 解析 16-bit keycode
- ✅ 最终修正（4 字节头 + BE keycode）的输出应完全匹配 `demo.vil`

#### 7.9.7 协议常量纠正

同步更新 `APP/include/vial_protocol.h`：
- `VIA_DYNAMIC_KEYMAP_GET_BUFFER = 0x11` → **删除**，改为 `VIA_GET_LAYER_COUNT = 0x11`
- `VIA_DYNAMIC_KEYMAP_SET_BUFFER = 0x14` → **删除**（VIA 协议中无此命令）
- `VIA_KEYMAP_GET_BUFFER = 0x12`、`VIA_KEYMAP_SET_BUFFER = 0x13` 保持不变

#### 7.9.8 修复后的完整流程（4 字节头 + BE keycode，已验证通过）

```text
桌面                                    固件
 │                                       │
 ├─ 11 ───────────────────────────────→  │  GET_LAYER_COUNT
 │  ←─────────────────────────────── 11 04  layers=4 ✓
 │                                       │
 ├─ 0C ───────────────────────────────→  │  宏数量
 │  ←─────────────────────────────── 0C 00
 │                                       │
 ├─ FE 0D ────────────────────────────→  │  动态条目
 │  ←─────────────────────────────── 00...│  无动态条目
 │                                       │
 ├─ 12 00 00 1C ──────────────────────→  │  KEYMAP_GET_BUFFER offset=0 size=28
 │  ←─ 12 00 00 1C [28B BE keycodes] ──  │  4 字节头，keycodes 从 byte 4 开始
 │                                       │   每对 BE (hi, lo)
 ├─ 12 00 1C 1C ──────────────────────→  │  offset=28 size=28
 │  ←─ 12 00 1C 1C [28B BE keycodes] ──  │  继续……
 │                                       │
 │         …… 共 7 包，读完 192 字节 ……    │
 │                                       │
 └─ self.layout = R0C0:LSFT(KC_9)...   │  ← 键值编辑器正常！✅
```


#### 7.9.9 调试方法总结

Bus Hound 在此次调试中至关重要。关键使用方式：

1. **确认命令是否发送**：搜索特定 HID 命令字节（如 `OUT.*12` 搜索 0x12）
2. **对比请求和响应**：确认固件返回的数据格式是否正确
3. **统计命令频率**：确认循环是否正确执行（0x12 应有 7 次出现）
4. **查桌面源码**：当不确定命令语义时，查 Vial 桌面端常量定义确认命令用途

#### 7.9.10 最终状态

- ✅ **Vial 桌面版完全可用**：连接 → 识别 → 定义加载 → 键值读写 → 布局编辑
- ✅ 所有 4 层 × 24 键位（6×4 矩阵）可通过 Vial 桌面在线配置
- ✅ key_data_buf 升级为 uint16_t，支持 QMK 修饰符键码（LSFT/LCTL/LALT 等组合键）
- ✅ 键值持久化到 EEPROM（layer 0~3 各 48 字节，新地址 0x3000 + layer×48）
- ✅ **vial.rocks 网页版实测可用**

---



---

### 7.10 ~~真根因定位：PB12/PB13 引脚冲突~~ ❌ 同样已证伪（2026-07-31）

> **⚠️ 此节分析方向是正确的质疑，但最终结论错误。PB12/PB13 作矩阵列输入不会干扰 USB1 通信。真正的根因是 `DEBUG=1`（§7.11）。**
>
> 以下保留分析过程供回溯参考。

#### 7.10.1 排除 EEPROM 时序假设

7.3 节的"EEPROM 时序"假设经多轮验证被推翻：

1. **7.3 三阶段修复（`c8aa8c9`，EEPROM 读放 IRQ 之后）**：Bus Hound 显示 SET REPORT 仍 5s NAK，与修复前完全相同。
2. **零 EEPROM 测试**（硬编码 USB 模式，不读 mode / 不读键码 / 不写 mode）：SET REPORT 仍 5s NAK。-> **问题与 EEPROM 操作无关**。
3. **回退 `a45fc5f` 启动顺序（EEPROM 读放 `USB_DeviceInit()` 之前）**：**连枚举都失败**。-> 在当前硬件上，EEPROM 读放 USB init 之前确实会破坏枚举（7.3.1 在这点上是对的）。

三者矛盾说明：EEPROM 时序不是 SET REPORT 故障的根因，只是与枚举相关的次级约束。

#### 7.10.2 ~~PB12/PB13 假说~~

当时对比了 `a45fc5f`（PB0~PB3 cols，Vial 可用）与 `ff9e052` 之后（PB12~PB15 cols，5s NAK），唯一差异在矩阵列引脚——其中 PB12/PB13 恰好是 USB2 的 D-/D+ 引脚。怀疑 GPIO 配置干扰了 USB1 PHY。

但后续**隔离测试证伪了此假设**：注释掉 `GPIOB_ModeCfg(col_all, ...)`（完全不配置 PB12~PB15），5s NAK 依旧。最终定位到真根因 §7.11。

> **PB12/PB13 作为矩阵列输入的注意事项**：虽然不干扰 USB1 通信，但 PB12/PB13 是 USB2（U2D-/U2D+）固定功能引脚。若未来启用 USB2（如双 USB 设备），则这两个引脚不可同时作 GPIO。当前固件仅用 USB1，PB12/PB13 作上拉输入是安全的。

---

### 7.11 ★ 真根因：`.cproject` `DEBUG=1` → USB ISR 内 printf 阻塞（2026-07-31 最终定位）

#### 7.11.1 根因链

```
.cproject -DDEBUG=1（自初始提交 1971014 就存在，从未被怀疑）
  → CH583SFR.h: #define PRINT(X...)  printf(X)
    → USB_MODE.c USB_DevTransProcess() 的 UIS_TOKEN_OUT 分支:
        case UIS_TOKEN_OUT:           // EP0 OUT — 在 USB ISR 上下文中!
            if(SetupReqCode == 0x09)  // SET_REPORT (Windows 设置键盘 LED)
            {
                PRINT("[%s] Num Lock\t",   ...);  // = printf(...) 阻塞!
                PRINT("[%s] Caps Lock\t",  ...);  // = printf(...) 阻塞!
                PRINT("[%s] Scroll Lock\n", ...);  // = printf(...) 阻塞!
            }
      → 3 次 printf() 在 USB 中断处理函数内串行执行
        → ISR 无法及时返回，UIF_TRANSFER 中断标志粘滞（stuck pending）
          → R8_USB_CTRL 的 RB_UC_INT_BUSY 位 = 1（USB_DeviceInit 默认设置）
            → USB 控制器自动 NAK 所有后续令牌包（IN/OUT/SETUP）
              → SET REPORT 数据阶段 → 5 秒超时
              → 所有后续控制传输 → CANCELED (c0010000)
              → Vial raw HID (EP3 OUT 0xFE 命令) → 从未到达!
```

#### 7.11.2 为什么之前没发现

1. **DEBUG=1 自 repo 创建就存在**（`.cproject` 初始提交 `1971014`），所有固件编译都带着它。没有"正常工作的版本"可对比。
2. **QFN28 封装没有空闲 TXD1 引脚**（PA8/PA9 被 ST7789 占用，UART debug 被注释掉）→ printf 输出实际上无处可去，但函数调用本身（字符处理、UART 状态轮询）的开销仍在。
3. **SET REPORT 是唯一带 OUT 数据阶段的控制传输**。枚举阶段的所有传输（GET_DESCRIPTOR=控制 IN，SET_CONFIG/SET_IDLE=控制 OUT 无数据）不需要进入 `UIS_TOKEN_OUT` 的 `SetupReqCode==0x09` 分支，故均正常。只有 Windows 发 SET_REPORT 设键盘 LED 时触发。
4. **7.3 节的测试矩阵全部在 DEBUG=1 下进行**，测量的是"printf 对 USB ISR 阻塞的不同表现"，而非 EEPROM 时序。

#### 7.11.3 修复

在 `.cproject` 中移除 `DEBUG=1` 编译宏定义：

```xml
<!-- 修复前 -->
<listOptionValue builtIn="false" value="DEBUG=1"/>
<listOptionValue builtIn="false" value="HAL_SLEEP=1"/>

<!-- 修复后 -->
<listOptionValue builtIn="false" value="HAL_SLEEP=1"/>
```

保留 `HAL_SLEEP=1`。移除 `DEBUG=1` 后：
- `PRINT(X...)` 展开为空（no-op）
- USB ISR 不再被 printf 阻塞
- `UIF_TRANSFER` 在 ISR 返回前被硬件自动清除
- `RB_UC_INT_BUSY` 不会触发
- SET REPORT 立即完成（无 5s 延迟）
- Vial raw HID EP3 正常收发

#### 7.11.4 验证

| 条件 | SET REPORT | Vial 通信 |
|------|-----------|----------|
| `DEBUG=1`（修复前） | ❌ 5s NAK → CANCELED | ❌ 从未启动 |
| `DEBUG=1` 移除（修复后） | ✅ 立即完成 | ✅ 正常识别布局、编辑键值 |

> **Bus Hound 对比**：修复后 SET REPORT 数据阶段在 <1ms 内完成（无 NAK 重试），EP3 OUT 出现连续的 Vial 0xFE 命令包（`GET_KEYBOARD_ID` → `GET_SIZE` → `GET_DEFINITION` → `GET_LAYER_COUNT` → `QMK_SETTINGS_QUERY` → `KEYMAP_GET_BUFFER`）。

#### 7.11.5 教训

1. **编译宏是全局的**：`DEBUG=1` 看起来无害，但它改变了 `CH583SFR.h` 中 `PRINT()` 的行为，从空操作变成真正的 `printf()`，影响到每个调用 `PRINT()` 的 ISR。
2. **不要在 ISR 内调用 printf**：即使启用了 UART，在 USB ISR 中调用 `printf()` 也会因为 UART 输出耗时（USB Full-Speed 每帧 1ms，中断来不及响应）导致问题。如果确实需要 ISR 内打 log，应使用环形缓冲区 + 主循环异步输出的方式。
3. **怀疑一切未变的常量**：DEBUG=1 自 repo 创建就存在，被认为"从来没变过所以没问题"，但它恰恰是根因。
4. **二分法调试的前提是单变量**：7.3 节的测试矩阵改变 EEPROM 读写次数/顺序，但始终在 DEBUG=1 下测试——被污染的基准导致所有结论无效。

---

### 7.12 当前代码状态（2026-08-01，扫描验证通过）

#### 当前工作版本

| 项目 | 内容 |
|------|------|
| **快照日期** | 2026-08-01（此后 §7.13 / §5.8 已加入 ST7789 + UI） |
| **当前 `main()`**（更新至 2026-08-03） | `Scan_init()` → `USB_DeviceInit()` → `PFIC_EnableIRQ(USB_IRQn)` → **`TMR3_TimerInit(90000)`** → `load_keymap_from_flash()` → **`ST7789_Init()` → `UI_Init()` → `while(1){ UI_Process() }`** |
| **Vial 协议** | 标准 VIA/Vial 协议完整实现，含 LZMA 压缩键盘定义 |
| **GPIO 引脚** | Row: PA4/PA5/PA15/PA14/PA13/PA12, Col: PB12/PB13/PB14/PB15 |

#### 当前已验证通过

| 项目 | 验证结果 |
|------|---------|
| USB 枚举 | ✅ 正常 |
| Vial 桌面连接 | ✅ 正常 |
| 键盘定义加载 | ✅ 正常 |
| 键值布局（R0C0=LSFT(KC_9)...） | ✅ 正确，与 `Reference/demo.vil` 一致 |
| 键值编辑 | ✅ 正常 |
| Flash 持久化 | ✅ 正常（magic byte 0xA5 + 4 层 × 48B） |
| **键盘扫描 + HID 输出** | ✅ **正常** — 短接 PA4-PB12 输出 `(`，PA5-PB12 输出 `)` |

#### 待恢复功能

当前 `main()` 已恢复 TMR3 扫描，以下功能尚未恢复：

1. **三模切换**：恢复 BLE/2.4G 模式初始化分支（`BLE_MODE.c`/`RF_MODE.c`）
2. **GPIOA 复位按键**：恢复 PA5 外部中断

---

### 7.13 ST7789 屏幕驱动调试（2026-08-02）：CS 接地根因与字节同步方案

#### 7.13.1 背景

为 2.25" 76×284 ST7789P3 屏编写驱动（`HAL/NV3007.c`，当时文件名沿用 st7789），GPIO bit-bang SPI（PA9=SCK, PA8=MOSI, PB7=DC, PB4=BL）。

#### 7.13.2 遇到的问题

- CS 接 PA11 且**每字节脉冲**（低→8位→高）：屏幕正常显示。
- 尝试 CS **接地**（恒低）：屏幕黑屏，误判为"这块屏必须 CS 脉冲"。
- 尝试过 CS 恒低 + SPI mode 0/2/3 均失败。

#### 7.13.3 乌龙：MOSI 误当 CS

排查时发现 **测试接线把 MOSI 当成了 CS**——数据线接地当然黑屏，CS 接地本身没问题。纠正接线后，CS 接地的真实行为需要重新验证。

#### 7.13.4 真正根因：CS 接地需要字节边界同步

CS 恒低时，ST7789 的 SPI 移位寄存器无法确定字节边界（8 个时钟为一个字节）。若上电/复位后总线有毛刺，或字节边界未对齐，D/C=0 的第一个命令会被当成上一个字节的残余位，导致后续命令/数据全部错位。

**关键**（参考 TFT_eSPI #163：CS 不使用时须用 SPI mode 3）：

1. **SPI mode 3**（CPOL=1 空闲高 + CPHA=1 上升沿采样）——CS 固定低时的正确 SPI 模式
2. **复位前拉低 SCK/MOSI**——防止毛刺被当作时钟/数据
3. **DC=0 下连发 8×NOP(0x00)**——强迫移位寄存器对齐 8 位字节边界，替代 CS 脉冲的状态复位作用

```c
/* 1. 复位前拉低 SCK/MOSI 防毛刺 */
SCK_LOW();  MOSI_LOW();  DC_LOW();
DelayMs(1);

/* 2. RST (PB23 共享) 复位后等稳定 */
DelayMs(250);

/* 3. 字节边界同步：DC=0 下发 8 个 NOP(0x00) */
DC_LOW();
for (i = 0; i < 8; i++) SPI_WriteByte(0x00);  /* NOP */

/* 4. 正常初始化序列（SWRESET → SLPOUT → 寄存器 → 0x29 DISPON） */
```

#### 7.13.5 最终方案

| 项 | 值 |
|----|-----|
| CS | **接地**（无需 GPIO 脉冲），PA11 释放 |
| SPI mode | 3（CPOL=1 + CPHA=1） |
| 字节同步 | 复位前拉低 SCK/MOSI + 8×NOP 对齐 |
| init 序列 | 8 针蓝板 ST7789 标准序列（0x29 DISPON + 120ms） |

✅ **已验证：CS 接地 + mode 3 + 字节同步，屏幕正常显示，PA11 可释放复用。**

#### 7.13.6 待办

- 文字方向校准（MADCTL 0x00 时可能旋转，需按实际屏方向调整）
- 5×7 字体行列索引已修正（bit7 顶行 + 8 行完整绘制），数字列顺序已反转
- TMR2 PWM 背光调光

---

</details>

<h2 id="sec8">八、LVGL 项目计划书（2026-08-03）：三模全量 UI 重构（v0.4 目标）</h2>

> 屏幕 UI 原为自绘方案（`HAL/ui.c` + `HAL/NV3007.c`，v0.3 已验证）。为支持更复杂的控件、布局与交互，决定引入 **LVGL 8.3.x** 重构 UI。本计划书与现状（§5.8 自绘 UI、§5.9 自定义上位机、§7.13 屏幕驱动）呼应，整体重新规划架构；**实施不影响当前已验证代码**，可一键回退。

### 8.1 目标与约束

| 约束 | 决定 |
|------|------|
| **版本** | LVGL **8.3.x**（8.3.11，8.x 系列最终版） |
| **范围** | **三模全量**：USB / BLE / 2.4G 共用一套 LVGL 屏幕，切模式只改状态 label |
| **交付** | 计划书入 README（本章节）；当前代码（v0.3）完全不动、可回退；整体重规划架构 |

### 8.2 硬件资源实测（数据来源：`Ld/Link.ld`、`obj/CH582_VIAL_PAD.map`）

| 资源 | 实测值 | 对 LVGL 的影响 |
|------|--------|----------------|
| RAM | 32K 总量；USB 单模静态 **4.8KB**（`_end=0x12d4`），空闲堆 **~26.8KB** | 全帧缓冲 42.2KB 放不下 → **局部刷新** |
| FLASH | 448K；当前固件 **~15KB** | LVGL core + 字体富余 |
| CPU | 60MHz RISC-V | 简单 UI 够用，动画需克制 |
| 显示 | 284×76 RGB565；全帧 284×76×2 = 42.2KB；GPIO bit-bang SPI | 局部刷新 + 整块批量写 |
| 睡眠 | `HAL_SLEEP=1`（BLE/2.4G 模式） | LVGL 节拍须与睡眠协调 |

> **结论**：RAM 32K 是唯一硬约束，但静态基础小、空闲堆充足，LVGL 局部刷新可行。

### 8.3 版本选型：为什么 8.3.x 而非 9.x

- LVGL **8.3.11**：8.x 系列最终版，成熟稳定，内存占用最小，社区验证多。
- **放弃 9.x**：实测 LVGL 9 比 8.3 多占约 **33% RAM**（内部颜色从 16 位 union 改为 32 位，所有 widget 单对象内存增大），32K RAM 下 8.3 是安全选择（参考 [LVGL forum 迁移对比](https://forum.lvgl.io/t/lvgl-9-higher-memory-usage-and-different-usage-reports-in-lvgl-9-compared-to-8/15308)）。

### 8.4 目标架构（整体重新规划）

```
┌────────────────────────────────────────────────────────┐
│  APP 层（三模不变：USB_MODE / BLE_MODE / RF_MODE）      │
│   · TMR3 矩阵扫描 ISR（现状保留）                        │
│   · HID 上报（现状保留）                                 │
│   · 自定义上位机 0xE1-E5（现状保留）                     │
├────────────────────────────────────────────────────────┤
│  UI 层（新）  HAL/ui_lvgl.c —— LVGL 屏幕 / 控件 / 事件   │
│   · 首页：模式label+电池icon+时钟label+日期+自定义文字    │
│   · 计算器：btnmatrix 按钮 + 显示 label + 运算状态机      │
│   · 三模共用一套屏幕，切模式只改状态 label               │
├────────────────────────────────────────────────────────┤
│  LVGL 核心  LVGL/ (vendored v8.3.11, lv_conf.h 裁剪)    │
│   · lv_disp 局部刷新 → flush_cb                          │
│   · lv_indev keypad（矩阵→按键）                         │
│   · lv_tick（TMR0 1ms）                                  │
├────────────────────────────────────────────────────────┤
│  驱动层  HAL/NV3007.c（现状保留 + 新增 Flush 批量写）    │
│  HAL/lvgl_port.c —— 显示/输入/节拍移植层                 │
└────────────────────────────────────────────────────────┘
```

### 8.5 与现状的边界（不影响当前代码）

| 类别 | 文件 | 处理 |
|------|------|------|
| **保留不动** | `HAL/NV3007.c` 底层（init/CASET/RASET/写像素）、`HAL/scan_key.c` 矩阵扫描、`APP/USB_MODE.c` 三模与 0xE1-E5、EEPROM 布局（keymap 0x3000 / mode 0x3F00 / 文字 0x3F10） | 原样 |
| **替换** | `HAL/ui.c` 渲染层 → LVGL 屏幕对象 | 源码保留树内，`LVGL_EN` 编译开关回退 |
| **新增** | `LVGL/` 源码、`HAL/lvgl_port.c`、`HAL/lv_port_indev.c`、`HAL/ui_lvgl.c`、`tools/font/` 像素字体（8/16/32px） | 新增 |

### 8.6 显示驱动移植（HAL/lvgl_port.c）

- **局部刷新缓冲**：全帧放不下 → 单缓冲 **428×3×2 = 2,568B**（≈1/47 屏，B0.7 NV3007）；`lv_disp_draw_buf_init(&buf, buf, NULL, 428 * 3)`。BLE 模式 428×2 = 1712B。
- **flush_cb**：`lvgl_flush_cb` → `ST7789_Flush(x,y,w,h,buf)`（每条 LVGL 逻辑行设一条物理列窗口：列 = 153-y、行 = x..x+w-1，DC 高 + 紧循环整块发送，内含 270° 转置）→ `lv_disp_flush_ready()`。
- **RGB565 字节序**：`LV_COLOR_16_SWAP=1` 匹配 NV3007（MSB first），烧录校准一次。
- **节拍**：`lv_tick_inc(1)` 由 **TMR0**（空闲）1ms ISR 驱动；SysTick 归 BLE 库（`MCU.c`），不可占用。
- **主循环**：仅在「标脏」时调 `lv_tick_inc`/`lv_timer_handler`（时钟每秒只刷秒数，LVGL 自动只重绘脏区）。

### 8.7 输入设计（HAL/lv_port_indev.c）

- `LV_INDEV_TYPE_KEYPAD`，矩阵扫描（TMR3，现状保留）产出 HID usage → 映射 `lv_key` 压入事件队列。
- 计算器按键用 **`lv_btnmatrix`**（单对象，24 键仅数百字节，远省于 24 个 button 对象）。
- 方向/确认：数字键盘 2/4/6/8 → `LV_KEY_DOWN/LEFT/RIGHT/UP`，Enter → `LV_KEY_ENTER`；退出：Backspace → `LV_KEY_ESC`（Tab+Backspace 组合切换 HID↔计算器，沿用 `UI_TOGGLE_K1/K2`）。

### 8.8 字体方案（保持像素风格）

v0.3 5×7 取模（bit6 顶、5 列）是用户认可的像素风格。LVGL 不做运行时缩放，用官方 `lv_font_conv` 预生成三档（输入 BDF，`--bpp 1 --format lvgl`）：

| 档位 | 对应现状 | 像素 | 用途 |
|------|---------|------|------|
| 8px | 1× | 5×8 | 顶部模式 / 底部日期 / 自定义文字 |
| 16px | 2× | 10×16 | 秒 / 计算器显示 |
| 32px | 4× | 20×32 | 时钟 HH:MM |

ASCII 0x20-0x7E 约 95 字，每档 ≈ 2-4KB FLASH。方向保持 bit6 顶 + 无反转行，烧录校准一次。

### 8.9 三模 + 低功耗适配（HAL_SLEEP=1）

| 模式 | 主循环 | LVGL 接入 |
|------|--------|-----------|
| USB | `while(1)` | `lv_timer_handler()` 直接调用（当前 main 循环改造） |
| BLE | TMOS 事件循环 | LVGL 挂 TMOS 周期事件，仅标脏时处理 |
| 2.4G | RF 事件循环 | 同 BLE |

**睡眠协调（关键）**：`lv_timer_handler` 仅在「时钟秒变 / 按键 / 上位机命令 / 模式切换」时调用；RTC 内部 32K 每秒唤醒刷新秒 label 再睡；矩阵列（上拉输入）变化唤醒；**禁用 LVGL 动画**（`LV_USE_ANIMATION=0`）省 CPU 与唤醒；BLE 模式由 BLE 栈管理睡眠，LVGL 无独立 timer。

### 8.10 内存预算（32K 硬约束，2026-08-06 实测）

**实测（numpad_ui 三页 UI 移植后，`obj/CH582_VIAL_PAD.map`）**：

| 资源 | 实测值 | 说明 |
|------|--------|------|
| FLASH | **184KB / 448K（40%）** | LVGL core + montserrat 10/14/24 + 软浮点（`%.8g`）+ lv_img 子系统 |
| RAM 总 | **22.1KB / 32K（68%）** | 含 16KB lv_mem 池 + 3.4KB 显示缓冲 |
| `LV_MEM_SIZE` | **16KB** | 三页 UI ~48 对象 + 渲染临时缓冲（8KB 不足会挂死，12KB 仍不足） |
| 显示缓冲 | **428×3 行 = 2,568B**（B0.7） | 单缓冲局部刷新（全帧 428×142×2 = 118KB 放不下） |
| 栈 | 6KB | LVGL 渲染深度 + USB ISR |
| BLE `MEM_BUF` | 0（gc 裁掉） | USB-only 模式无引用，被链接器裁剪 |

> ⚠️ **经验**：三页 UI 必须 ≥16KB 池，否则 `LV_ASSERT_MALLOC` 在 `ui_init()` 挂死（USB/VIAL 中断仍工作，表现为黑屏）。后续若加功能，需同步核验 `_end`。
> **FLASH**：448K 富余；软浮点（计算器 double + `LV_SPRINTF_USE_FLOAT`）约 +20KB。

### 8.11 里程碑 M0-M9

| 里程碑 | 内容 | 验证 |
|--------|------|------|
| **M0** ✅（`f90f1ef`，2026-08-03） | 下载 lvgl 8.3.11 入 `LVGL/`，写 `lv_conf.h`，`.cproject` 加 sourceEntry + `-I`；仅编译通过，不改功能 | MounRiver Build 通过（FLASH/RAM 不变） |
| **M1** ✅（2026-08-04） | `lvgl_port.c` 显示端口 + `ST7789_Flush` 批量写（含 y 翻转）；色带测试 UI | 屏显红/绿/蓝 + 文字正立（见 §8.14） |
| **M2** | 中文字体 `ui_font_cn_14/12` 生成（Python+PIL 恢复后执行） | 切回中文标签 |
| **M3** | ✅ 首页/计算器/设置三页 + 双主题（**直接移植 `LVGL-opendesign` numpad_ui 完成**，`bb6c179`） | 主页正常显示（见 §8.14） |
| **M4** | ✅ 按键驱动 UI：Tab+Backspace 翻页 + 计算器页输入（`617a52e`）；`lv_port_indev` keypad 焦点后续 | 实体键可操作三页 |
| **M5** | ✅ 计算器运算状态机（numpad_ui 自带 `ui_calc_eval` double 求值） | 计算正确 |
| **M6** | 上位机 0xE1-E5 → 更新 LVGL label；EEPROM 逻辑不变 | `tools/ch582_host.py` 全命令验证 |
| **M7** | 三模集成 + 睡眠协调：共用屏幕，切模式只改状态 label，按需刷新与唤醒 | 三模互切 + 睡眠功耗实测 |
| **M8** | 内存/性能优化：已调至 16KB 池 + 6 行缓冲（`_end`≈22KB ✅）；继续：裁剪 lv_conf、刷新周期调优 | 交互可接受 |
| **M9** | README 更新（本计划书执行结果 + 回退说明），提交 | 文档一致 |

### 8.12 风险与对策

| 风险 | 等级 | 对策 |
|------|------|------|
| RAM 溢出（尤其 BLE 模式） | 高 | M8 专项裁剪；缓冲/`LV_MEM_SIZE` 可调；每里程碑核验 .map；超限退回自绘 UI |
| bit-bang SPI 慢导致卡顿 | 中 | 局部刷新 + `ST7789_Flush` 整块写；时钟只刷秒/时分区；动画全关 |
| flush 期间 ISR 延迟（BLE/按键） | 中 | flush 在主循环（非 ISR）；必要时分片刷新 |
| BLE + `HAL_SLEEP` 与 LVGL 节拍冲突 | 高 | §8.9 按需刷新设计；无独立 LVGL timer；RTC 1s 唤醒驱动 |
| RGB565 字节序 / 字体方向反 | 低 | `LV_COLOR_16_SWAP`、字体档位各校准一次（已有 MADCTL 0xF0 基准） |
| LVGL 9 升级迁移 | 低 | 锁定 8.3.x；如需升级单独立项 |

### 8.13 回退与兼容

- 编译开关 **`LVGL_EN`**（config.h 或 .cproject 宏）：`0`=现状自绘 UI（`ui.c`/`st7789.c` 原样），`1`=LVGL。当前代码（v0.3 已验证版 `abf47ed` 之后）完全不动。
- `HAL/ui.c`、`HAL/include/ui.h` 保留树内（同 ws2812b.c 处理：保留不编译），一键回退。
- EEPROM 布局、VIAL 协议、三模逻辑、上位机协议**全部不变**，LVGL 只替换屏幕渲染层。

### 8.14 实施记录

#### M0（commit `f90f1ef`，2026-08-03）— LVGL 8.3.11 植入 ✅

- **源码**：`LVGL/lvgl.h` + `LVGL/src/`（192 个 .c，整库 vendored）
- **配置**：`LVGL/lv_conf.h` 裁剪（详见下表）
- **构建**：`.cproject` 加 `-I ../LVGL`、`-I ../LVGL/src`、`LVGL` sourceEntry
- **验证**：仅编译通过、无人调用 lv_*，FLASH/RAM 不变（14932B / 5132B）
- **踩坑**：`lv_conf_template.h` 顶部 `#if 0 /*Set it to "1" to enable content*/` 必须改 `#if 1`，否则全部配置失效、LVGL 回退内部默认（montserrat 全开 + 48KB 池 → RAM 溢出）
- **依赖**：`lv_canvas` 需要 `LV_USE_IMG`（lv_img.h 内容被 `#if LV_USE_IMG` 保护），M0 设 `LV_USE_CANVAS 0` 规避；图标改由 lv_obj 样式 + lv_line 绘制

**lv_conf.h 关键配置**：

| 项 | 值 | 说明 |
|----|-----|------|
| `LV_COLOR_DEPTH` / `LV_COLOR_16_SWAP` | 16 / 1 | RGB565，SWAP=1 匹配 ST7789 MSB-first |
| `LV_MEM_SIZE` | 6KB | lv_mem 池（M8 再调） |
| `LV_USE_ANIMATION` | 0 | 时钟/计算器无需动画，省 CPU/RAM |
| `LV_FONT_DEFAULT` | `&lv_font_unscii_8` | M2 换 v0.3 像素字体 |
| `LV_USE_*` | 仅 label/btn/btnmatrix/line/flex/theme_default | 其余全关（含全部 extra 控件） |

#### M1（2026-08-04）— 显示驱动移植 ✅

**新增/修改文件**：

| 文件 | 内容 |
|------|------|
| `HAL/lvgl_port.c` / `HAL/include/lvgl_port.h` | LVGL 显示端口：局部刷新缓冲 + `flush_cb` + TMR0 1ms tick + M1 色带测试 UI |
| `HAL/NV3007.c/h` | 新增 `ST7789_Flush(x,y,w,h,buf)` 批量写 |
| `HAL/include/config.h` | `LVGL_EN` 编译开关（1=LVGL 默认，0=回退自绘 ui.c） |
| `APP/hidkbd_main.c` | `main()` 按 `LVGL_EN` 分支 |
| `Ld/Link.ld` | `__stack_size` 512→2048（LVGL 渲染栈深） |

**关键设计**：
- **局部刷新**：284×10 单缓冲 = 5680B（全帧 284×76×2 = 42KB 放不进 32K RAM）
- **垂直翻转**：MADCTL=0xF0 使 ST7789 `y=0`=物理**底部**（v0.3 自绘约定）；LVGL `y=0`=顶部 → `NV3007_Flush` 内做 y 轴翻转 + 行序倒序。⚠️ **勿改 MADCTL**，否则与 flush 翻转双重反向（曾踩坑：误改 0xF0→0xD0 导致文字上下颠倒）
- **节拍**：TMR0 1ms ISR → `lv_tick_inc(1)`（SysTick 归 BLE 库，不可占用）
- **测试 UI**：红/绿/蓝三色带 + 居中 `LVGL 284x76 OK`，验证 flush 路径 / RGB565 字节序 / 字体渲染

**M1 验证**：屏显红上·绿中·蓝下 + 文字正立 ✅

**M1 资源占用**（实测 `obj/*.map`）：

| 资源 | 值 | 占用 |
|------|-----|------|
| FLASH | ≈107KB（0x1AEA4） | 448K 的 24%，余量大 |
| RAM 静态 | 17.3KB（`_end=0x4514`，含显示缓冲 5.7KB + lv_mem 6KB） | 32K 的 54% |
| RAM 总（含栈 2KB） | ≈19.3KB | 32K 的 60% |
| 空闲堆 | ≈12.7KB | LVGL 对象经 lv_mem 池，无需堆 |


#### M1 稳定化（2026-08-06，94d63b1 / 4fa7f10 / dd6c2bb / 82192f5）— 屏幕/USB/VIAL/按键全链路修复 ✅

M1 基础上修复 4 个缺陷（均已验证）：
1. TMR0_IRQHandler 缺 __INTERRUPT（94d63b1）：M1 引入。缺该属性 → GCC 生成 ret 而非 mret，中断返回跳回被打断函数上层 ra、跳过 mepc~ra 间指令 → 寄存器残留腐坏任意被中断的代码。静态色带掩盖了症状，VIAL 通信（USB ISR 高频 + TMR0 每 1ms 打断渲染/SPI）触发可见异常。修复：加 __INTERRUPT __HIGH_CODE。同时 LVGL_Process 加 idle yield（原 while(1){lv_timer_handler();} 100% CPU 空转）。
2. TMR3 扫描期屏蔽 TMR0（4fa7f10）：get_key_fanz 位脉冲 GPIO 扫描期间禁用 TMR0，避免 1ms 中断入口/出口扰动扫描时序（Bus Hound 确认当时 ZERO HID 输入报告）。
3. GPIO 方向交换 + get_key()（dd6c2bb）：二极管实测反焊（阳极接 row、阴极接 col），get_key_fanz 电流方向被阻断 → 改 get_key（驱动列、读行）电流方向匹配 → 按键可检测。get_key 补上 scan_modifier 累积。
4. 列切换 2µs 行恢复延时（82192f5）：行线 40kΩ 上拉 RC 恢复不足 → 上一列按键残留污染下一列 → ghosting（按 "(" 出 "()"）。修复：get_key() 列恢复 HIGH 后 mDelayuS(2)。

**当前验证**：色带正常 + USB 枚举/VIAL 通信稳定 + 按键正常检测输出。

#### M4 移植（2026-08-06）— LVGL-opendesign numpad_ui 三页双主题 UI ✅

从 `C:\ClaudeProject\LVGL-opendesign\lv_sim` 移植 LVGL 8.3 三页双主题 UI 到本工程，替换 M1 色带测试 UI：

**新增文件**：
- `HAL/numpad_ui.c`（40KB，原样移植 + 2 处适配）
- `HAL/include/numpad_ui.h`（API：ui_init / ui_set_page / ui_set_theme / ui_set_mode / ui_calc_input / 亮度/休眠设置项 + 3 个 weak hooks）

**适配点**：
1. `UI_USE_CN_LABELS=0`（英文标签）：CN 字体（ui_font_cn_14/12）需 Python+PIL 生成器，当前环境不可用；中文模式代码保留，生成字体后改回 1 即可
2. 计算器 `×`/`÷` 改 ASCII `*`/`/`：Montserrat 字符集不含 U+00D7/U+00F7

**lv_conf.h 增量**：LV_USE_IMG=1（ALPHA_8BIT 图标）、LV_SPRINTF_USE_FLOAT=1（计算器 %.8g）、montserrat_10/14/24=1、LV_MEM_SIZE 6→8KB、LV_FONT_DEFAULT=&lv_font_montserrat_14

**lvgl_port.c**：删 lvgl_test_ui → ui_init()；新增强符号 ui_hook_get_rtc（读 CH582 RTC）+ ui_hook_mode_output / ui_hook_reset_connection（预留）；lvgl_rtc_init（内部 32K + 非法时间初始化）。显示驱动 / TMR0 tick / __INTERRUPT / yield / USB-first 顺序全部保留。

**功能**：主页（时钟+日期+WiFi/蓝牙图标+USB/BT/RF 模式按钮）、计算器（过程+结果，实体键驱动）、设置（亮度/休眠/主题/重置连接）、底部 3 圆点导航、深浅主题（共享样式 + lv_obj_report_style_change 瞬时切换）。

**待办**：生成 ui_font_cn_14/12 后切回中文标签；ui_hook_mode_output 接三模切换；实体键→ui_set_page/ui_calc_input 接线（M4 输入里程碑）。

#### M4 按键接线（2026-08-06，617a52e）— 实体键驱动三页 UI ✅

- `numpad_ui.h/c` 新增 `ui_get_page()` getter
- `APP/USB_MODE.c` TMR3 ISR 路由：
  - **Tab + Backspace** 组合键 → 循环翻页（主页 → 计算器 → 设置）
  - **计算器页**：小键盘按键 → `ui_calc_input`（数字 / `+ - * /` / `.` / Enter=`=` / Backspace=退格 / ESC=C 清空），不触发 HID
  - **主页/设置页**：按键正常 HID 上报
- 补丁过程曾引入 `else if(0)` 禁用 HID 的 BUG，已修复为 `else`
- **内存最终值**：`LV_MEM_SIZE=16KB` + 显示缓冲 284×6（3.4KB），`ui_init()` 不再挂死（8/12KB 均不足）

**当前验证**：三页 UI 正常显示 + USB/VIAL/HID 正常 + 实体键翻页/计算器输入正常。

#### M8 性能优化（2026-08-06，f1de2d6）— bit-bang SPI 提速 ~2.5x ✅

**DMA/硬件 SPI 评估结论：不可行（不改硬件）**
- SPI0 引脚固定 PA12-15 / PB12-15，**全部被矩阵占用**（row_2-5 / col_0-3）
- SPI1 控制器存在但 CH583 **无 GPIO 引脚引出**（驱动无引脚配置）
- PWM-DMA（WS2812 方案）为单线协议，无法驱动 SCK+MOSI 双线
- 屏幕 SCK/MOSI（PA9/PA8）是 UART1 引脚，非 SPI 引脚——除非改 PCB 重排矩阵

**实际优化（`HAL/NV3007.c` SPI_WriteByte）**：
- `R32_PA_CLR`（写清除，1-2 周期）替代读改写拉低 SCK/MOSI（SCK/MOSI 同在 GPIOA）
- 去掉 NOP 节流（~8-10Mbit/s，低于 ST7789 15.5MHz 上限）
- 8 位展开消除循环分支
- 效果：全屏翻页 ~100ms+ → **~40-50ms**（翻页/刷新明显流畅）

#### M10（2026-08-09，B0.7）— 换屏：ST7789 76×284 → NV3007 142×428（横向 428×142）✅ 待真机验证

**背景**：屏幕更换为 **NV3007 142×428 彩屏**（1.68"，GRAM 168×428，可见窗口 X=12..153、Y=0..427）。

**改动**（文件改名 `HAL/NV3007.c/h`，API 保留 `ST7789_*`，调用点零改动）：

| 文件 | 内容 |
|------|------|
| `HAL/NV3007.c/h` | 初始化序列换成 Arduino_GFX NV3007 同源序列（`0xFF 0xA5` 厂商模式 + gamma/电源/时序 + `0x3A=0x05` + SLPOUT/DISPON）；MADCTL=0x00 竖屏物理序；`ST7789_Flush` 改为列窗口转置（每条逻辑行 → 物理列窗口，列 = 153-y、行 = x..x+w-1） |
| `HAL/lvgl_port.c` | `hor_res/ver_res` → 428×142；USB 行缓冲 4→3 行（428×3×2 = 2568B）；共享区 pad `0x100`→`0x170`（保证 `.lvgl_shared` ≥ BLE 尾部 `0x4B70`，`.highcode` 仍在 `0x4C00`，bss 不碰栈） |
| `HAL/numpad_ui.c` | 三页布局 284×76 → 428×142（主页左 256+右 172、时钟/日期重排、模式按钮 34px、计算器/设置显示区 420×128、圆点 y=134、亮度条 48×6） |

**验证清单**（真机）：上电黑屏 → 主页正常；文字方向（反了改 `NV3007_ROT_REV_Y`）；翻页/计算器/设置布局；BLE 模式主页不 OOM。

**B0.7.1（2026-08-09）— 背光极性修复**：NV3007 模块背光为**高电平点亮**（LVGL 官方 NV3007 Arduino 示例 `digitalWrite(BL, HIGH)`），原代码沿用旧 ST7789 的低电平点亮导致上电黑屏。新增 `ST7789_BL_ACTIVE_HIGH` 宏（默认 1），`BL_ON()/BL_OFF()` 统一封装，`ST7789_Init` 与 `ST7789_SetBrightness` 走同一极性。

**B0.7.2（2026-08-09）— 花屏定位工具**：背光修好后屏亮了但花屏，初始化表已与 Arduino_GFX/LVGL 源码逐字节核对无误，怀疑点为初始化变体或 SPI/接线。新增两个编译开关：
- `ST7789_DEBUG_PATTERN`（默认 1）：初始化后依次刷 红→绿→蓝→白→黑 纯色各 0.6s 再进 LVGL——纯色正常说明驱动/SPI/窗口正常，问题在 LVGL 层；纯色也花则问题在初始化/接线。
- `ST7789_INIT_VARIANT`（默认 0）：0 = Arduino_GFX 默认（1.65"/1.68" 屏），1 = LVGL `lv_nv3007.c`（2.79" 屏 "279" gamma 序列，已逐字节核对）。纯色自检花屏时切换此宏重试。

**B0.7.3（2026-08-09）— 按卖家 T279VJ-C10-01 示例代码修正**：用户提供卖家驱动包（`2.79TFT-T279VJ-C10-01-SPI-NV3007`），确认本屏为 **2.79" T279VJ-C10-01**（背光高电平开、SCL 空闲低、列偏移 12）。据此：
- `ST7789_INIT_VARIANT` 默认改为 **1**（卖家 279 序列），默认表替换为卖家可执行代码线序（`0xF1` 数据 = `0x0E 0x17`，无 `0xF9` 命令；Arduino/STM32/C51 三份卖家代码一致，已逐字节核对）。
- `SPI_WriteByte` 每字节结束 **SCK 拉低**（真 mode 0 空闲低，与卖家波形一致）。
- 结论：此前花屏的主因是 2.79" 屏用了 1.68" 的 Arduino_GFX 序列。

**B0.7.4（2026-08-09，`fac390b`）— 驱动改名 NV3007 + 冷启动自复位 + LVGL 刷新诊断**：
- **文件改名**：`HAL/st7789.c/h` → `HAL/NV3007.c/h`（`ST7789_*` API 保留、调用点零改动；MounRiver 按文件夹收集源码，无需改工程配置），全部 include 与 README 路径同步。
- **冷启动自复位**：`main()` 开头读复位标志，上电复位（`RST_FLAG_RPOR`）时先执行一次 `SYS_ResetExecute()`——MCU 全局复位带动共用复位网络给 NV3007 一个干净的 RST 脉冲，等效手动复位键；软复位后标志为 `RST_FLAG_SW` 自动跳过，不会死循环（手动按键 MR / 看门狗 WTR 均不触发）。
- **LVGL 刷新诊断**：新增 `LVGL_FULL_REFRESH` / `LVGL_SOLID_TEST` 开关（默认 0），用于验证“上电后顶部到中部淡色带”是否由窄窗口局部写入引起。

**B0.8（2026-08-11）— 取消 LVGL，裸机 UI（`bm_ui`）**：
- **开关**：`config.h` 设 `LVGL_EN=0`、`UI_BM_EN=1`；`numpad_ui.c` / `lvgl_port.c` 移出构建（`.cproject` HAL excluding），LVGL 源码移出 sourceEntry（源码保留在树内）。
- **新增**：`HAL/bm_ui.c/h` + `HAL/bm_font.c/h`——六主题调色板（像素/极简/黑客 × 双色，默认极简·浅）、三页面（主页/计算器/设置）、底部三点导航、TMR0 1ms tick + RTC；公共 API 与 numpad_ui.h 同名，`USB_MODE.c` 按键路由零改动。
- **内存**：`Ld/Link.ld` 删除 `.lvgl_shared` / `.lvgl_shared_ble`，`.highcode` 起点改为 `ALIGN(SIZEOF(.ovl_ble)+SIZEOF(.ble_heap), 0x100)` → USB 模式释放 19.3KB、BLE 模式释放 7.9KB，三模 + UI 不再抢 RAM。
- **设计规范**：`C:\ClaudeProject\tft_NV3007\brand-spec.md`；字体 = 8×8 数字（时钟/结果整数缩放）+ 5×7 拉丁（由 NV3007.c 内置字库转置生成）；中文标签 / 16×16 图标子集待字体转换器接入。
- **淡色带**：软复位实验证实为面板复位不彻底（短脉冲无效、手动复位正常）；`NV3007_RST_GPIO=1`（RST 改接 PA11，100ms 低/120ms 高）为根治方案，驱动代码已就绪。

**B0.8.1（2026-08-12）— 裸机 UI 字符方向修复 + 模拟器截图直写**：
- **方向修复**：真机"字符上下颠倒、仅左下角日期正常"。根因有二：
  - 5×7 字库（`g_ascii_data`）以"底行优先"存储，8×8 数字（`g_digit_data`）以"顶行优先"存储，两套字形方向不一致，`NV3007_TEXT_FLIP=1`（`HAL/include/NV3007.h`）统一后 8×8 又反过来；
  - 5×7 的 `'1'` 字模本身存反（衬线落在底部）。
- **修复内容**：`g_digit_data` 全部反转成底行优先（与 5×7 一致）；`'1'` 字模改为 `{0x20,0x60,0x20,0x20,0x20,0x20,0x70}`；`bm_icon16_direct` 图标翻转逻辑与文字相反（图标顶行优先，需 `15-row` 反序）；`NV3007_TEXT_FLIP` 默认置 1。模拟器逐字形验证：时钟/日期/按钮/计算器结果全部正立。
- **模拟器**：`sim/main.c` headless 截图改为直接从 `sim_fb` 写 24-bit BMP（绕开 SDL 渲染器，无头输出不再受渲染缩放影响）。

**B0.8.2（2026-08-12）— 裸机 UI 中文界面 + 几何对齐参考规范**：
- **中文字库**：`HAL/bm_font.c` 新增 16×16 点阵中文字库（39 字：设置标签/主题名/蓝牙/周几/秒/永不/执行完成 + `·`，SimSun 15px 二值化生成，约 1.2 KB Flash）。新增 `bm_font_glyph_utf8()` UTF-8 解码查字；`bm_text_direct`/`bm_text_width` 支持拉丁 + 中文混排（中文 16×16 不放大，与 14px 拉丁同高）。
- **界面中文化**：设置页标签 `亮度/休眠/主题/重置连接`、值 `80% / 30秒 / 永不 / 执行 / 完成`、主题名 `像素·绿…黑客·琥珀`、主页模式按钮 `USB/蓝牙/RF`、日期周几 `周日…周六`，对齐 `numpad-ui-pager.html` 唯一真源。
- **几何对齐**：模式按钮统一 `x=266 宽 152`（参考右区 172px + padding 10，原极简/黑客 258/160）；计算器显示面板极简/黑客加 6px 圆角（像素保持方形硬边）；设置图标 16×16 改 14×14 中心裁剪（参考 setting-icon 14px）；设置亮度条极简/黑客加 3px 圆角。

**B0.8.3（2026-08-12）— 裸机 UI 精细度对齐 numpad-ui-pager.html**：
- **像素主题 LCD 点阵纹理**：驱动新增 `ST7789_FillDots()`（逐行单窗口流式，真机/模拟器同实现）；像素·绿/琥珀背景按参考 `.screen::after` 叠 3px 网格深色点（传呼机观感），极简/黑客保持纯色。
- **设置页**：像素主题补 `--sc-soft` 面板底 + 2px 边框（原缺面板底）；黑客主题按参考移除 1px 外框（透明列表）。
- **主页状态簇**：蓝牙图标 16×16→14×14 与电池图标对齐（参考 14px）；像素主题电量百分比 16px 主文字色（参考 `batt-val` fg），极简绿/红、黑客亮色不变；模式按钮各主题统一 `y=10`（参考右区 padding 10）。
- **计算器**：过程行分隔线归位（像素 32px / 极简·黑客 36px 行高，参考 `.calc-proc` 高度）；结果行底边距 6px（参考 `padding-bottom 6`）。
- **日期**：极简/黑客底边距 14px（参考 `.home-date bottom:14`），像素保持 12px。

**B0.8.8（2026-08-12）— 刷新/初始化优化 + 初始化参数回退（真机验证）**：
- **SPI 快路径**：`SPI_WriteByte` 改为整端口直写（每 bit 两次 `R32_PA_OUT` 全写，去掉读-改-写），全屏数据流约快 1.5 倍；`NV3007_SLOW_SPI=1` 保留原逐位 NOP 调试路径。
- **局部刷新**：计算器按键只重绘表达式行 + 结果行（分隔线保留）；设置页亮度/休眠/重置只重绘对应行（主题仍整页）；“已重置→执行”反馈只刷第 3 行。整页与局部共用 `bm_draw_settings_row_text()` / `bm_draw_settings_row0_bar()`。
- **初始化参数回退（真机冷启动异常）**：曾试 `NV3007_PWR_SETTLE_MS=200`、`NV3007_INIT_RETRY=0`、`NV3007_DEFER_DISPON=1`（UI 首帧后再 DISPON），真机初始化异常 → **恢复 B0.8.7 验证值**：`PWR_SETTLE_MS=400`、`INIT_RETRY=1`、`DEFER_DISPON=0`（Init 内清黑 + DISPON）。新增 `NV3007_DisplayOn()` API 保留（仅 DEFER=1 时由 UI 调用）。
**B0.8.7（2026-08-12）— NV3007 复位优化（Arduino_GFX 对比 + 无 GPIO 上电复位方案）**：
- **Arduino_GFX 参考结论**：`Arduino_NV3007.cpp` 的 `tftInit()` 在未接 RST（`GFX_NOT_DEFINED`，默认）时**不做任何复位**（源码仅注释 "Software Rest"），直接执行 init 表；`nv3007_init_operations` / `nv3007_279_init_operations` 表内也**没有 SWRESET(0x01)**——Arduino_GFX 完全依赖面板上电自复位。LVGL 官方 NV3007 示例同样要求 GPIO 拉 RST（高 100ms → 低 120ms → 高 120ms）。卖家 C51/STM32/Arduino 参考代码全部用硬 RST。本工程 2.79" T279VJ-C10-01 面板实测 **SWRESET 无法复位 GOA/GIP 闩锁状态**，与“按复位键才正常/淡色带”现象吻合。
- **根因**：面板 RST 与 MCU 复位网络共用（10K 上拉 + 100nF，τ≈1ms），RST 在面板 VDD 尚未稳定时就升为高电平，GOA 闩锁错误；手动复位发生在 VDD 稳定之后，所以一按就好。ST7789 内部 POR 更健壮，因此旧屏不需要硬复位。
- **固件改动（`HAL/NV3007.c/h`）**：
  - 复位与 init 序列抽成 `NV3007_SoftReset()` / `NV3007_SendInitSequence()` 复用；
  - 无 GPIO 路径新增 `NV3007_PWR_SETTLE_MS=400`（上电后先等 VDD/RST 稳定再发 SWRESET+init，原固定 250ms）；
  - 新增 `NV3007_INIT_RETRY=1`：若第一遍 init 发在 RST 仍为低期间被忽略，300ms 后重发完整序列（SWRESET+init）；
  - 新增 `NV3007_RST_USE_PA10`：`NV3007_RST_GPIO=1` 时可选 PA10（32K_XI）代替 PA11——PA10/PA11 是 32K 晶振引脚，RTC 用内部 32K（`CLK_OSC32K=1`）时空闲，作为“无 GPIO”之外的兜底。
- **推荐硬件方案（无需 GPIO）**：给面板 RST 单独 RC——RST ← 10K→3V3 + 10µF→GND（τ≈100ms），上电后 RST 在 VDD 稳定后才升为高电平，等效手动复位；若 RST 仍接在 MCU 复位网络上，可把 100nF 加大到 4.7~10µF 并保持固件等待 ≥400ms。接线确认后无需再改固件（或只调 `NV3007_PWR_SETTLE_MS`）。

**B0.8.6（2026-08-12）— 架构审查修复（三模 UI 同步 + 自定义文本接入 bm_ui + 旧 UI 移除）**：
- **三模 UI 同步**：hidkbd_main.c BLE 分支启动时调用 ui_set_mode(UI_MODE_BT)、USB 分支 ui_set_mode(UI_MODE_USB)——修复蓝牙模式下主页仍高亮 USB 的问题；模拟器 headless 支持 mode 参数，像素级验证 BT 高亮正确。
- **长按切换防丢跳**：USB 模式 ==1667、BLE 模式 ==313 改为 >=——若扫描中断丢一拍（TMR0 屏蔽/BLE 任务抖动），原精确比较会永远无法触发切换。
- **BLE 三页面路由补齐**：BLE 模式扫描路径加入 Tab+Backspace 翻页、计算器键、设置 1-4 键路由（此前 BLE 只发 HID、无法翻页），与 USB 模式一致；ui_key_to_calc_char/ui_key_to_settings_idx 从 USB_MODE.c 私有改为共享（ui.h 声明）。
- **自定义文本接入 bm_ui**：raw HID 0xE2/0xE3 原本写入旧 ui.c 的内存缓冲但 bm_ui 不显示（命令形同虚设）。现由 m_ui.c 实现 UI_SetCustomText/UI_GetCustomText/UI_UpdateCustomText，EEPROM 0x3F10 持久化、主页日期上方显示（模拟器 SIM_CUSTOM_TEXT 环境变量可测）。
- **旧 UI 框架移除**：ui.c（旧 284×76 布局死代码）从构建排除（.cproject sourceEntries + obj/HAL/subdir.mk），Flash 省约 13KB；lvgl_port.c/
- **死代码清理（P5）**：`HAL/KEY.c`、`HAL/LED.c`、`HAL/lvgl_port.c`、`HAL/numpad_ui.c`、`HAL/ws2812b.c` 移入 `Reference/retired/`（保留头文件与事件位定义，无链接依赖）；RAM 布局文档更正为"裸机 UI 与 BLE 堆共存"（§10.3/10.4）。
umpad_ui.c 等仍排除不参与构建。**B0.8.5（2026-08-12）— 凤凰点阵体 16px 全字库替换（对齐 numpad-ui-pager.html 像素主题）**：
- **字库来源**：用户提供 凤凰点阵体vonwaon-bitmap.ttf（VonwaonBitmap-16px.ttf），用 GDI+ SingleBitPerPixelGridFit 渲染全部 ASCII 0x20-0x7E（95 字）+ 39 个中文到 16×16 网格（32B/字，MSB 左，顶行优先），替换原 8×8 数字 + 5×7 拉丁 + SimSun 中文三套自绘字模。
- **字体结构升级**：glyph_t 新增 ix 标志——中文 fix=1 固定 1:1（16px），拉丁/数字按 scale 放大；m_font_glyph()/m_font_glyph_narrow()/m_font_glyph_utf8() 全部改查凤凰字库。
- **渲染函数适配**：m_text_direct()/m_text_direct_narrow() 按 16 宽双字节读取 + fix 判断 + NV3007_TEXT_FLIP 翻转。
- **字号重排**（16px 网格整数倍）：时钟像素 3×（48px）/ 极简黑客 2×（32px）；日期 1×（16px）；主页按钮 1×（16px）；状态簇 1×；计算器过程 1×、结果像素 3×（48px）/ 极简黑客 2×（32px）；设置行 1×。
- **模拟器验证**：六主题三页面全部正常，无重叠/乱码/颠倒；识图（qwen3.7-plus）确认主页时钟、模式按钮、日期、电池簇与参考 HTML 像素主题观感一致。
**B0.8.4（2026-08-12）— 裸机 UI 中文渲染修复 + 极简主题细节优化**：
- **中文 16×16 字模渲染修复**：m_glyph()/m_text_direct() 原按每行 1 字节读取字模，16×16 中文每行实为 2 字节，导致中文字只画左半边、显示为乱码方块。新增 ytes_per_row 并按 col >> 3 选择高位/低位字节，中文（蓝牙/周四/设置标签等）全部正常显示。
- **极简主题按钮优化**：圆角半径 3px→6px（更接近参考 .mode-btn border-radius:8px）；按钮标签字号 10px→15px（label_scale 2→3，5×7 点阵放大）。
- **极简主题时钟放大**：m_clock_scale() 非像素主题分化为极简 6（42px）、黑客 5（35px），更接近参考极简 38px 大字。
- **计算器结果字体**：非像素主题分化为极简 6（48px）、黑客 5（40px），过程行/结果行布局不变。

### 8.15 LVGL 换屏快速移植指南

LVGL 与屏幕的唯一耦合点是 `lv_disp_drv_t.flush_cb`（[HAL/lvgl_port.c](HAL/lvgl_port.c) 的 `lvgl_flush_cb`），渲染逻辑（三页 UI/主题/字体）与屏幕无关。

**换屏五步**：
1. 新屏驱动 `HAL/<new_lcd>.c`：实现 `NEWLCD_Init()`（初始化序列）+ `NEWLCD_Flush(x,y,w,h,buf)`（设窗口 + 批量写 RGB565）
2. `lvgl_port.c` flush_cb 内改调 `NEWLCD_Flush`（一行）
3. `main()`：`NV3007_Init()` → `NEWLCD_Init()`（保持 USB 之后）
4. `lv_conf.h`：`LV_COLOR_DEPTH`/`LV_COLOR_16_SWAP` 对齐新屏；`LV_MEM_SIZE` 不变
5. `lvgl_port.c`：`hor_res`/`ver_res`/`LVGL_BUF_ROWS` 按新屏

**常见坑**：垂直翻转（flush 内 y 翻转）、红蓝互换（`LV_COLOR_16_SWAP` 取反）、位深不符（`LV_COLOR_DEPTH`）、方向（寄存器 + hor/ver 交换）、漏 `lv_disp_flush_ready`（卡死）。

**复用模板**：`HAL/NV3007.c` 的 `NV3007_Flush` 是通用模板（行倒序 + 批量循环），只需替换写字节函数与窗口设置。

---

### 8.16 NV3007 初始化调试过程（B0.7 → B0.8.8）

1. **换屏（B0.7 / M10）**：ST7789 76×284 → NV3007 142×428（横向 428×142，逻辑行=物理列转置 flush）。
2. **花屏根因（B0.7.3）**：SPI 必须 **mode 0、SCK 空闲低**——与卖家 C51/STM32/Arduino 三份代码逐字节核对，`SPI_WriteByte` 字节结束把 SCK 拉低。
3. **淡色带/冷启动异常（B0.7.4）**：面板 RST 与 MCU 复位网络共用（10K 上拉 + 100nF，τ≈1ms），上电时 RST 在 VDD 稳定前先拉高 → **GOA/GIP 闩锁错误**；SWRESET(0x01) 只复位控制器、清不掉闩锁状态。手动复位发生在 VDD 稳定之后，所以“按一下就好”。
4. **Arduino_GFX 对比（B0.8.7）**：无 RST 引脚（`GFX_NOT_DEFINED`）时 `tftInit()` 什么都不做、init 表也没有 SWRESET；LVGL 官方 NV3007 示例硬性要求 GPIO 拉 RST（高 100ms → 低 120ms → 高 120ms）。结论：本 2.79" 面板必须一次“VDD 稳定后的干净复位脉冲”。
5. **方案**：`NV3007_RST_GPIO=1`（PA11/PA10 飞线）为根治；无 GPIO 时面板 RST 单独 RC（10K→3V3 + 10µF→GND）等效手动复位；固件兜底为 `NV3007_PWR_SETTLE_MS=400` + `NV3007_INIT_RETRY=1`（B0.8.7 验证值）。
6. **当前稳定配置**：`NV3007_RST_GPIO=0`、`NV3007_PWR_SETTLE_MS=400`、`NV3007_INIT_RETRY=1`、`NV3007_DEFER_DISPON=0`、`NV3007_SLOW_SPI=0`（快路径）、`NV3007_INIT_VARIANT=1`（卖家 279 序列）。

---
### 8.17 ★ 中文字库生成裁剪 bug（2026-08-21，重大发现）

**现象**：模拟器截图里部分汉字**右边、下边被截断**（如"间"右侧竖钩缺失、"周"右侧竖钩缺失），其余汉字正常。

**排查过程（逐项排除）**：
1. 布局越界：用 `bm_text_width*` 实测全部文本 `起始X+宽度 ≤ 磁贴右边界`，无越界 → 排除；
2. 4bit 半字节解包：脚本端 `stride=(Grid+1)>>1` 与固件端硬编码（16→8/12→6/32→16/22→11/40→20）完全一致，12px 列数 12×4bit=48bit=6 字节恰好整除 → 排除；
3. 全屏裸绘"周六"（无磁贴）仍正常 → 排除布局裁剪；
4. **最终定位（gen 脚本位图裁剪）**：`gen_klb_font.ps1` 的 `New-GlyphBitmap` 位图宽度=字号（12px 字用 48px 位图）。实测 Noto 渲染"间"（U+95F4）墨迹 **x=12..47，右侧余量 0px**——右侧笔画被位图右边界裁掉；96px 位图下完整（x=12..51）。降采样后右侧列 coverage 掉到 0~5 → 汉字缺右/下笔画。

**修复（bbox-fill 生成）**：放弃"原生比例 + ox 偏移"取模，改用：
- 大位图（宽高 = 字号×2，杜绝贴边裁剪）；
- 检测墨迹包围盒 → 等比缩放**填满网格**（高填满，宽等比居中）。

已用该方式重新生成全部缺失汉字（含 时/间/克/莱/因/蓝/默/认/低/功/耗/恢 等 42 字，16px+12px）。修复后"间"右侧竖钩 coverage=11 贯穿全行，视觉验证完整。

**附带修复**：
- `bm_text_direct_micro` 增加 UTF-8 判断 → head 中文（"设置""外观"）与 `·`（NV·PAD / M·512）恢复显示；
- 字库补"恢"（U+6062，SETT-01"恢复默认"此前缺字显示为"复默认"）。

**后续（同一提交前追加）**：39 个最初由 `gen_klb_font.ps1` 生成的汉字（"周""连""六"等）经核实同样存在右侧/底部裁剪（"周六""连接"截断）——已用 bbox-fill **统一重新生成全部 80 个汉字**（16px + 12px，仅 `·` 保留原数据），模拟器放大验证"周"竖钩/`周六`/`连接` 完整，布局在新字宽下仍全部在容器内。

**规范对齐（§3/§5，2026-08-21 追加）**：严格按 `vial-pad-klb-ui.md` 对齐计算器：
- CALC-02 运算页表达式 **22px**（此前误用 13px expr）→ 改用 22px mode 字体；
- 补 mode22 运算符字形：`+ * / % = × ÷`（39→46 字符）；
- 运算符号显示映射 `* → ×`、`/ → ÷`（13px 结果页/历史 + 22px 运算页），符合规范示例 `1,280 × 3.5 =` / `256 ÷ 4`；
- 预览补 `= ` 前缀（规范 `= 64`）；
- 修复 mode22 减号字形（原为实心方块，重生成 22×9 细横线）。

> 已用 **Saira Thin**（`%TEMP%\fonts\Saira-Thin.ttf`）统一重生成 mode22 全部 46 字符（A-Z/0-9/空格/`- . + * / % = × ÷`），
> 放大验证 `12.5×3.5-2÷4` 显示完整、风格统一；其中 `.` 因 Saira 小数点基线超出采样区，手工构造 22 网格底部小点。

**主页 head/时钟对齐（2026-08-21 追加）**：
- 左上品牌 `NV·PAD` → **`FinPad`**（纯 ASCII，micro 字体）；
- 时钟 32px → **28px**（对齐 html HOME-02 规格 `9/22/28`），用 Saira Thin 重新生成 0-9/`:`/空格；
- 修复冒号：原 32px 冒号两点间距 22px 过开、偏上下，28px 重生成后两点居中清晰；
- 日期 `08.21 周六` 格式与 html `08.17 周一` 对齐（`MM.DD 空格 周X`）。

**时间/日期细节（2026-08-21 追加）**：
- 冒号加宽（w 4→8，两侧留白），解决冒号与数字贴太近；
- 日期新增 12px 数字字模（`g_date12`，Saira Thin），`08.21` 与 `周六` 字号/基线完全对齐（原 9px 数字偏小）。

**软件 SPI 提速（2026-08-21 追加）**：`SPI_WriteByte` 快路径改为**全展开**（去掉 do-while 循环计数/移位/分支，直接 8 位 MSB 先发，每字节降至 16 次端口写）。软件 bit-bang 的物理极限即每字节 16 次端口写（每位 2 次），展开仅省循环开销，预计整屏刷新提升 15–25%；若需数量级提速，需硬件 SPI（当前 PA9/PA8 非 CH582 SPI0 引脚）或 DMA。

**上电首页刷新两次修复（2026-08-21 追加）**：`main` 先 `ui_bm_init()` 画首帧，随后 `ui_set_mode()` 无条件置 `dirty=1`，导致进入主循环后**整页重绘第二遍**（肉眼即"首页闪两次"）。修复：
- `ui_bm_init(ui_mode_t)`：首帧即用指定模式绘制（USB/BT），不再依赖 init 后的 `ui_set_mode`；
- `ui_set_mode`：仅当模式真正变化时才 `dirty=1`（相同模式不重绘）。

---
<h2 id="sec9">九、版本记录（可回退点）</h2>

| Tag | Commit | 内容 |
|-----|--------|------|
| **`B0.8.8`（待提交）** | 本版 | NV3007 SPI 快路径 + 局部刷新 + 初始化参数回退 B0.8.7 验证值 + **中文字库生成裁剪修复（bbox-fill 重生成缺失字，§8.17）** + head 中文/`·` 修复 + 补"恢"字（详见 §8.14 B0.8.8 / §8.16 / §8.17） |
| **`v0.5`（B0.8.7）** | `dc6d2ca` | NV3007 复位优化：Arduino_GFX 对比（无 RST 引脚时不发任何复位）+ 无 GPIO 上电复位（RC 硬件方案 + 固件等待/init 重试）+ PA10 兜底（详见 §8.14 B0.8.7） |
| `B0.7.4` | `fac390b` | 驱动改名 `HAL/NV3007.c/h` + 冷启动自复位（免手动复位键）+ LVGL 刷新诊断开关（详见 §8.14） |
| `B0.7.3` | `916e463` | 卖家 T279VJ-C10-01 初始化序列 + SCK 空闲低（花屏根因） |
| **`B0.7-nv3007`** | `f817739` | NV3007 142×428 彩屏移植：横向 428×142 + 列转置 flush + 三页布局重排 + LVGL 缓冲重算（详见 §8.14 M10） |
| **`v0.4-numpad-ui-verified`** | `92248cc`+ | LVGL 三页双主题 UI + 实体键翻页/计算器输入 + SPI 提速 ~2.5x + 设置页布局修复（左右对齐、无重叠）+ 设置页禁止 HID、数字键 1-4 操作 |
| `v0.3-st7789-landscape` | `0a60677` | 自绘 UI 横向显示 + 3x 字体（LVGL 前的屏幕基线） |
| `v0.2-usb-scan-verified` | `9d5af03` | USB 枚举 + Vial + 键盘扫描 + HID |
| `v0.1-usb-vial-verified` | `d6cf4de` | USB 枚举 + Vial 协议 + 布局修复 |

**回退**：`git checkout fac390b`（B0.7.4 基线）；`git checkout B0.7-nv3007`（B0.7 换屏基线）；`git checkout v0.4-numpad-ui-verified`（旧屏 LVGL 三页基线）；`git checkout v0.3-st7789-landscape`（回到自绘 UI）

### 9.1 设置页布局修复（2026-08-07，8b2a2ee / 480d301 / d7a9143）— 重叠 + 左右对齐 ✅

**调试方法**：`LVGL-opendesign/.toolchain/headless_main.c` 无窗口渲染器（ui_init → 3 页 × 2 主题 → PPM 截图），PowerShell 像素级分析行分布定位，比 SDL 窗口/烧录迭代快。

**三个根因与修复**：
1. **行重叠**（8b2a2ee）：flex column 默认 cross 轴 CENTER → 行收缩为内容宽，4 行堆叠中央、左右组重叠。修复：`lv_obj_set_width(row, LV_PCT(100))` + 图标 12×12→10×10 + 重置图标/箭头改 ASCII `R`/`>`（FontAwesome 字形 10px 下渲染出异常垂直带）+ list 高度 66→64（圆点空间）+ label LONG_CLIP。
2. **左侧未对齐**（480d301）：row/lg flex `CENTER`/`SPACE_BETWEEN` 使左组偏离行缘。修复：lg 内容 `START`（图标贴左 x10-12）。
3. **右侧未对齐**（d7a9143）：LVGL 8.3 `SPACE_BETWEEN` 未把右组推到右缘（chev 停在 x221）。修复：`rg` 加 `flex-grow(1)` + 内容 `END`（值/chev 贴右 x272-273，等价 HTML `margin-left:auto`）。

**最终布局**（headless 像素验证）：4 行左贴左（icon x10-12）、右贴右（值/chev x272-273）、无重叠。

### 9.2 设置页按键逻辑（2026-08-07，92248cc）— 禁止 HID + 数字键操作 ✅

- 设置页**完全禁止 HID 输出**（此前设置页走 HID 分支）
- 数字键直控四行（`ui_settings_apply(idx)`，与点击事件共用逻辑）：
  - `1` = 亮度 +20%（≥100 回卷 20）
  - `2` = 休眠循环（10s/30s/60s/永不）
  - `3` = 主题切换（浅 ↔ 深，即时全屏）
  - `4` = 重置连接（"已重置"→900ms 恢复）
- 其他键在设置页忽略且不发送 HID
- 主页 HID 输出、计算器页输入逻辑不变
<h2 id="sec10">十、蓝牙 BLE 模式开发计划书（2026-08-07）</h2>

### 10.1 目标与现状

**目标**（B0.2 恢复单固件三模）：`main()` 读取 EEPROM `0x3F00` 模式字节——`0x0B` 进入 USB 模式（LVGL 三页 UI，v0.4 不变）；`0xBE` 进入 BLE HID 模式（广播 → 连接 → 按键照常，屏幕用轻量 `HAL/ui.c` 显示时钟/BT 状态）；`0x24` 为 2.4G 预留。**长按 7/8/9 写模式字节后复位即可三模互切，无需重新烧录**。

**现状（已具备）**：
- `APP/BLE_MODE.c`：`HidEmu_Init()` + BLE HID 服务（hidEmuSendKbdReport / 状态回调）——参考工程遗留，未接线
- `HAL/MCU.c`：`CH58X_BLEInit()`（BLE 栈配置，MEM_BUF 堆）
- 三模切换键：长按 7/8/9 写模式字节并复位（`USB_MODE.c`/`BLE_MODE.c`/`RF_MODE.c` 均已实现）
- 32K 晶振（PA10/PA11 外部晶振已接）、DCDC 支持
- `HAL_SLEEP=1` 已定义

### 10.2 架构（§7.3 USB-first 约束下）

```
main() 固定顺序（不可变，§7.3）：
  SetSysClock → Scan_init → ST7789_Init → USB_DeviceInit → IRQ+TMR3 → load_keymap
  → 读模式字节 0x3F00
      ├─ 0x0B → LVGL_Init → while(1){ LVGL_Process }          （USB，LVGL 三页）
      └─ 0xBE → memset(MEM_BUF) → CH58X_BLEInit → HAL_Init → GAPRole_PeripheralInit
              → HidDev_Init → HidEmu_Init → UI_Init（轻量 HAL/ui.c）
              → while(1){ TMOS_SystemProcess(); UI_Process(); }
```

**关键点**：
- USB 初始化先行（枚举所需），BLE 初始化在其后（两者不冲突）
- **主循环双服务**：BLE 模式为 TMOS（BLE 栈，1.25ms 调度）+ 轻量 UI_Process；USB 模式为 LVGL_Process（30ms 刷新）
- 按键扫描（TMR3 ISR）不变；计算器/翻页逻辑不变
- **HID 上报切换**：BLE 模式用 `hidEmuSendKbdReport`（替代 `U2DevHIDKeyReport`），TMR3 ISR 内按模式分发

### 10.3 RAM 预算矛盾（最大风险 ⚠️）— **B0.2 共享 RAM 重叠方案（2026-08-07）**

**实测**（B0 全量链接后，`obj/*.map` 2026-08-07 00:45）：RAM **42844B ≈ 42.9KB**，分解：

| 块 | 大小 | 说明 |
|----|------|------|
| `.highcode` | 11.4KB | BLE 库 RF/TMOS 常驻代码 ~6.4KB + USB/ISP/驱动 ~4.8KB（实际以 objdump 校准） |
| `.data` | 1.6KB | BLE 库初始化数据为主 |
| `.bss` | 27.9KB | LVGL 池 16KB + 显示缓冲 3.4KB + MEM_BUF 4KB + 其余 ~4.5KB |
| 栈 | 2KB | `Link.ld` 实测值 |

**结论**：单固件同时承载 **LVGL 三页 UI（~21KB）+ BLE 全栈（~14.7KB）+ 固定开销（~8.2KB）≈ 44KB > 32KB**，方案 A 只省 2KB，**证伪**。

**B0.2 共享 RAM 重叠（单固件三模）**：**B0.8.6 更正**：LVGL 已移除，现为**裸机 UI（.data/.bss）与 BLE 堆（.ble_heap 6KB）共存布局**——同一固件链接，USB/BLE 模式共用；链接层复用 RAM 基址区（历史 B0.2 方案，LVGL 时期为时间互斥）：

| 段 | VMA | 内容 | 归属 |
|----|-----|------|------|
| `.ovl_ble` | `0x20000000` | BLE 库 RF/TMOS 常驻代码（`libCH58xBLE.a` 的 `.highcode` 已用 objcopy 改名 `.ovl_highcode`） | 仅 BLE/RF 模式 |
| `.lvgl_shared` | `0x20000000`（与上重叠） | LVGL 池 16KB + 显示缓冲 4 行 2.3KB（`lv_mem.c`/`lvgl_port.c` 声明加 section 属性） | 仅 USB 模式 |
| `.ble_heap` | `0x20000000 + SIZEOF(.ovl_ble)` | `MEM_BUF` 6KB（NOLOAD，BLE 初始化前 memset） | 仅 BLE/RF 模式 |
| `.highcode`（固定） | 共享区后、256B 对齐 | 向量表 + USB/ISP/驱动/扫描 ISR | 所有模式 |

启动流程：`startup_CH583.S` 增加第二段拷贝循环，把 BLE 常驻代码拷入 `0x20000000`；USB 模式下 LVGL_Init 会在同一地址格式化内存池，互不干扰。RAM 合计 ≈ **30.4KB ✅（余 ~1.6KB）**：BLE 侧 6.4+6KB 落在 LVGL 侧 18.2KB 共享区内，固定段（向量/驱动 4.8KB + data 1.6KB + bss 3.9KB + 栈 2KB）另计。

> **三模切换恢复**：单固件长按 7/8/9 → 写模式字节 → 复位 → 对应模式启动，无需重新烧录。BLE/RF 模式屏幕为轻量 UI（时钟+状态），USB 模式为完整 LVGL 三页。

#### 10.3.1 **B0.3 决策（2026-08-08）：LVGL 暂时砍掉，键盘与三模优先**

**实测现象**：B0.2 下 USB→BLE 可切，但 BLE 模式**切不回 USB**。

**根因**：BLE 模式下存在**两条按键扫描路径**同时运行——`USB_MODE.c` 的 `TMR3_IRQHandler`（1.5ms 扫一次，阈值 1667）与 `BLE_MODE.c` 的 `HidEmu_ProcessEvent`（`START_DEVICE_EVT`，8 TMOS tick 扫一次，阈值 313）**共用同一组 `scan_buf`/`last_buf`/`change_mode_*`**，互相清计数，长按计数永远到不了阈值 → 无法写 0x0B 复位。

**决策（用户）**：LVGL 可以以后再做，**三模切换和键盘功能优先**。

**B0.3 实施**：

| 项 | 改动 |
|----|------|
| `HAL/include/config.h` | `LVGL_EN=0`（源码保留树内，随时可恢复） |
| `APP/hidkbd_main.c` | USB/BLE 模式都走 legacy `HAL/ui.c`（时钟+状态+计算器，v0.3 已验证），不再调用 LVGL |
| `Ld/Link.ld`、`Startup/startup_CH583.S`、`LIB/libCH58xBLE.a` | **全部恢复原始版本**，撤销共享 RAM 重叠（回归 B0 前已验证布局） |
| `HAL/numpad_ui.c` | `#if LVGL_EN` 包裹 + 空桩，保证 `USB_MODE.c` 可链接 |
| `APP/USB_MODE.c` | `TMR3_IRQHandler` 仅 `g_boot_mode==0x0B` 时扫描；BLE 模式按键扫描与切回 USB 由 `BLE_MODE.c` 自带逻辑负责（阈值 313，参考固件已验证） |

**RAM**：无 LVGL 后合计约 23KB（BLE 高代码 6.4 + 堆 6 + 固定段 ~10.5 + 栈 2），32KB 内余量充足，不再需要重叠方案。LVGL 移植代码全部保留，后续在共享 RAM 方案（本节约束条件）下可重新启用。

#### 10.3.2 **B0.4 修复（2026-08-08）：BLE 模式扫描方向 + 开机逃生键**

**实测现象**：B0.3 后仍切不回 USB；且**重新烧录也进 BT**。

**根因一（切不回）**：`BLE_MODE.c` 的 `HidEmu_ProcessEvent` 调用 `get_key_fanz()`——这是**旧扫描方向**（驱动行、读列），而当前硬件与 `Scan_init` 是**驱动列、读行**（`get_key()`，二极管阳极→row、阴极→col）。BLE 模式下 `get_key_fanz` 读不到任何按键 → 长按 7 的计数永远为 0 → 写不了 `0x0B`。

**根因二（重烧仍 BT）**：模式字节存在**数据闪存** `0x3F00`，USB ISP 烧录只擦代码区，不清数据区 → 0xBE 一直保留，开机永远进 BLE。

**修复**：

| 项 | 改动 |
|----|------|
| `APP/BLE_MODE.c` | `get_key_fanz(scan_buf)` → `get_key(scan_buf)`（START_DEVICE_EVT 与 START_REPORT_EVT 两处），与当前矩阵方向一致；BLE HID 上报与切回 USB 同时恢复 |
| `APP/hidkbd_main.c` | 开机逃生键：模式字节非 USB 时，若**按住切换键 7 上电**，强制写 `0x0B` 并进入 USB（数据闪存重烧不清，这是唯一不需要上位机的恢复手段） |

> **恢复操作**：按住数字键 7 重新上电 → 进入 USB 模式并清除模式字节。

#### 10.3.3 **B0.5 决策（2026-08-08）：LVGL 重新启用（共享 RAM 重叠 + 已修复的切换链路）**

B0.3/B0.4 证明切换问题与共享 RAM 重叠本身无关（真因是扫描方向 + 双扫描冲突），因此恢复 B0.2 验证过的重叠布局：

- `LIB/libCH58xBLE.a` 恢复 `.ovl_highcode` 改名版、`Ld/Link.ld` 恢复共享区布局、`Startup/startup_CH583.S` 恢复第二拷贝循环；
- `LVGL_EN=1`：**USB 模式** → LVGL 三页 UI（内存池/显示缓冲在共享区）；**BLE/RF 模式** → legacy `HAL/ui.c`（BLE 栈占用共享区）；
- 保留 B0.3/B0.4 修复：TMR3 仅 USB 模式扫描、`BLE_MODE.c` 用 `get_key`、开机按住 7 强制 USB；
- RAM ≈ 31.1KB（B0.2 实测布局），切换链路已是修复版。

> 验证顺序：USB 模式三页 UI 正常 → 长按 8 进 BLE（轻量 UI）→ 长按 7 切回 USB（三页 UI）→ 按住 7 上电强制 USB。

#### 10.3.4 **B0.6（2026-08-08）：BLE 模式也运行 LVGL（单主页 + 6KB 池）**

共享区实测：`.ovl_ble` 0x0–0x14BC + `.ble_heap` 0x14BC–0x2CC0，**0x2CC0–0x49E0 约 7.3KB 空闲**——BLE 模式可以在这段尾部再跑一个精简 LVGL：

| 项 | 说明 |
|----|------|
| `LV_MEM_CUSTOM=1` | LVGL 分配走 `ui_lvgl_alloc/free/realloc`（自研 first-fit 链表，`HAL/lvgl_port.c`） |
| 双池 | USB：16KB（`.lvgl_shared`，RAM 基址）；BLE：6KB（`.lvgl_shared_ble`，共享区尾部）+ 2 行显示缓冲 |
| 页面 | BLE 模式仅创建**精简主页**（时钟 + 日期 + 右上角 BT 角标，无图标/模式按钮），计算器/设置页因 6KB 池放不下暂缺；`ui_set_page`/圆点刷新已做 NULL/页数保护 |
| 主循环 | BLE：`TMOS_SystemProcess()` + `lv_timer_handler()` |
| RAM | 总占用不变（≈31.1KB）——BLE 池/缓冲用的是共享区原有空闲尾段 |

> 若 6KB 池导致主页 OOM（LV_USE_ASSERT_MALLOC 挂死），可缩减主页控件或把 `.ble_heap` 降到 5KB 换更大池。

### 10.4 节拍与中断（无冲突）

| 资源 | 归属 | 说明 |
|------|------|------|
| SysTick | BLE 库（TMOS） | `CH58X_BLEInit` 配置但禁用中断；TMOS 自用 |
| TMR0 | 裸机 UI tick（1ms，`bm_ui`） | `ui_bm_init()` 启用，USB/BLE 共用；驱动时钟刷新/设置反馈 |
| TMR3 | 按键扫描（1.5ms） | 仅 USB 模式扫描（BLE 模式由 `BLE_MODE.c` 任务扫描） |
| USB1 | USB 枚举（HID/VIAL） | USB-first 后 BLE 初始化，不冲突 |
| RTC | 时钟显示 | 现有（内部 32K；BLE 用外部 32K 更佳——验证 LSE） |


### 10.5 模式切换闭环

- 主页 USB/BT/RF 按钮 → `ui_hook_mode_output`（现为空）→ 写模式字节 `0x0B/0xBE/0x24` → `SYS_ResetExecute()`
- 长按 7/8/9 切换键（已实现）保留
- BLE 模式 UI：主页模式按钮高亮 BT；`ui_hook_get_rtc` 不变

### 10.6 里程碑

| 里程碑 | 内容 | 验证 |
|--------|------|------|
| B0.6 | BLE 模式 LVGL 主页（6KB 池 + 2 行缓冲，共享区尾部）+ `LV_MEM_CUSTOM` 双池 | BLE 模式主页正常显示、时钟走动；互切正常 |
| B0.5 | LVGL 重新启用（USB 三页 UI + 共享 RAM 重叠），保留 B0.3/B0.4 切换修复 | 编译通过、RAM < 32K；USB↔BLE 互切 + 三页 UI 正常 |
| B0.4 | BLE 模式扫描方向修复（`get_key_fanz`→`get_key`）+ 开机按住 7 强制 USB（逃生键） | BLE 长按 7 切回 USB；重烧后按住 7 上电回 USB |
| B0.3 | 键盘优先：`LVGL_EN=0`（源码保留）、恢复原始 Link.ld/startup/BLE 库、TMR3 仅 USB 模式扫描、BLE 模式切回由 `BLE_MODE.c` 负责 | 编译通过；USB↔BLE 互切（长按 7/8）恢复 |
| B0.2 | 共享 RAM 重叠（单固件三模）：`libCH58xBLE.a` 改名 `.ovl_highcode` + `Link.ld` 共享区 + startup 第二拷贝循环 + `MEM_BUF`/LVGL 池进共享段 + main() 0xBE 分支轻量 UI + TMR3 按模式路由 | 编译通过，RAM < 32K，USB 模式行为不变 |
| B1 | BLE 广播/连接：HidEmu 接线、配对、按键 HID 输出 | 手机/PC 蓝牙连接并输入字符 |
| B2 | BLE 模式 UI：轻量页时钟 + BT 连接状态显示（`HAL/ui.c`） | 状态正确显示 |
| B3 | 三模闭环：长按 7/8/9 写模式字节并复位，断电保持 | USB↔BLE 互切验证 |
| B4 | 功耗/睡眠：HAL_SLEEP 协调、连接间隔、屏显节流 | 静态/连接功耗实测 |
| B5 | README 更新 + tag（v0.5-ble-verified） | 文档一致 |

### 10.7 风险与对策

| 风险 | 等级 | 对策 |
|------|------|------|
| BLE 堆 4KB 不足（广播/连接失败） | 高 | 退回 6KB + UI 精简（方案 C）或 lv_mem 12KB+精简 |
| 三页 UI 16KB 池 + BLE 共存仍超 RAM | 高 | 显示缓冲 6→4 行（−1.1KB）、关闭未用 widget、MEM_BUF 4KB |
| TMOS + LVGL 主循环时序（LVGL 渲染阻塞 TMOS） | 中 | lv_timer_handler 不加长 yield；刷新周期 30→50ms；必要时渲染降级 |
| USB-first 后 BLE 初始化时序（射频/时钟） | 中 | 参考 WCH 官方 BLE HID 例程顺序 |
| 外部 32K 晶振 vs 内部 LSI 精度 | 中 | 优先外部晶振（PA10/11 已接）；BLE 校时用 `Lib_Calibration_LSI` 备选 |
| VIAL 键值表与 BLE 上报映射 | 低 | BLE HID 复用 `key_data_buf`（uint16_t QMK 拆解已有） |

### 10.8 涉及文件

| 文件 | 改动 |
|------|------|
| `LIB/libCH58xBLE.a` | B0.3 恢复原始版本（`.highcode`）；B0.2 曾 objcopy 改名 `.ovl_highcode`（已撤销） |
| `Ld/Link.ld` | B0.3 恢复原始版本（无共享区）；B0.2 曾做 `.ovl_ble`/`.lvgl_shared`/`.ble_heap` 重叠（已撤销） |
| `Startup/startup_CH583.S` | B0.3 恢复原始版本（无第二拷贝循环） |
| `APP/hidkbd_main.c` | 单固件三模分支；USB/BLE 均走 legacy `HAL/ui.c`，不调用 LVGL |
| `HAL/include/config.h` | `LVGL_EN=0`（键盘优先，源码保留）；`BLE_MEMHEAP_SIZE` 6KB |
| `LVGL/src/misc/lv_mem.c` | LVGL 池保留 `.lvgl_shared` section 属性（LVGL 禁用时被 GC，无影响） |
| `HAL/lvgl_port.c` | 保持 `#if LVGL_EN` 包裹；显示缓冲 4 行（LVGL 恢复时生效） |
| `APP/USB_MODE.c` | TMR3 仅 `g_boot_mode==0x0B` 扫描（修复 BLE 模式双扫描冲突）；USB=页面路由/HID；BLE/RF=交给对应模式层 |
| `APP/BLE_MODE.c` | 接线 `HidEmu_Init`、按键报告入口、连接状态回调 |
| `HAL/ui.c` | 所有模式的当前渲染层（时钟+HID 状态，v0.3 已验证）；顶栏按 `g_boot_mode` 显示 USB/BT/RF MODE |
| `HAL/numpad_ui.c` | `#if LVGL_EN` 包裹 + 空桩（LVGL 禁用时保证链接）；LVGL 恢复后接三模写模式字节 |
| `README.md` | 本计划书 + 里程碑记录 |

## 11. PC 模拟器（SDL2 免烧录验证）

> 2026-08-12：不烧录真机即可在 PC 上运行**同一份** `HAL/bm_ui.c` +
> `HAL/bm_font.c` 绘图代码，用于验证三页面渲染、主题切换与按键路由。

### 11.1 原理

- `sim/sim_nv3007.c` 用 428×142 RGB565 帧缓冲模拟 `NV3007.h` 声明的
  `NV3007_*` API，并复刻真机的物理列窗口转置（列 = 153 − 逻辑 y），
  所以 `bm_ui.c` 的直写渲染在 PC 上显示与真机一致。
- `HAL/bm_ui.c` 通过 `BM_SIM` 宏隔离 CH582 硬件依赖（TMR0 中断、
  CH58x RTC），绘图代码本身零改动；RTC 使用 PC 本地时间。
- SDL2 窗口 3 倍放大显示；键盘路由与固件一致。

### 11.2 构建与运行

1. `sim\download_tools.bat`：下载 w64devkit（MinGW-w64）与 SDL2 mingw
   开发包到 `sim\vendor\`（首次需联网，已 gitignore）。
2. `sim\build.bat`：生成 `sim\nv3007_sim.exe`。
3. 交互运行：`sim\nv3007_sim.exe`
   - `Tab + Backspace`：翻页（主页 → 计算器 → 设置）
   - 计算器页：`0-9 + - * / .`、`Enter`(=)、`Backspace`、`Esc`(C)
   - 设置页：`1` 亮度 / `2` 睡眠 / `3` 主题 / `4` 重置
   - `M`：循环 USB / BT / RF；`T`：任意页循环主题；`B`：循环亮度
4. 无头截图：`nv3007_sim.exe --shot out.bmp [页] [帧] [主题点击] [表达式]`
   （表达式自动追加 `=`，如 `--shot s.bmp 1 8 0 12+34`）

### 11.3 已用模拟器验证

| 页面 | 结果 |
| --- | --- |
| 主页 | 窄点阵时钟（像素 49px / 极简·黑客 35px）、状态簇（电池绿/次要色+蓝牙+电量）、card 背景模式按钮（像素方角 / 极简圆角 / 黑客 `[ ]`+辉光）、日期含周几、主题化导航点 |
| 计算器 | 过程行 16px 右对齐（黑客带 `> ` 与闪烁光标）、结果大字号、除零 `Err`、超长转指数（`9.1e+12`）、黑客结果辉光 |
| 设置页 | 4 行等高：亮度（图标+条+百分比）/ 睡眠 / 主题（半日图标）/ 重置，行间分隔线、黑客 `> ` 提示符、值 + `›` 指示 |

### 11.4 UI 规范实现（2026-08-12，对应 brand-spec.md）

- **六主题**：像素·绿/琥珀、极简·浅/深、黑客·绿/琥珀，设置页「主题」循环切换。
- **主页**：左区 252px（时钟+状态簇+日期）/ 右区 172px（3 模式按钮）；
  状态簇右上右 10（极简电池绿/红、蓝牙工业蓝；其余主题次要色）。
- **计算器**：显示型面板（无屏键），过程行/结果行右对齐；像素 2px、
  极简·黑客 1px 边框与分隔线；黑客霓虹辉光 + `> ` 提示符 + 闪烁光标。
- **导航点**：像素空心方块、极简实心圆点、黑客辉光圆点。
- **像素点阵纹理**：背景 3px 网格深色点（NV3007_FillDots），极简/黑客无纹理；设置页像素=软面板+2px 边框、黑客=无外框（对齐 pager.html）。

### 11.5 字符方向开关（B0.9）

模拟器已逐像素验证全部字形（5×7 窄点阵、8×8 数字、16×16 图标）正立。
若真机烧录后文字/图标上下颠倒：

1. `HAL/include/NV3007.h` 的 `NV3007_TEXT_FLIP` 改为 `1`，只翻转直写
   文字与图标字形（几何/FillRect 不受影响），重新编译烧录。
2. 先用 `HAL/include/bm_ui.h` 的 `BM_UI_DIR_TEST_BOOT=1` 烧录一版：
   上电显示方向自检帧（`TOP`/`BOT` + 大号 `2Pq` + 三个图标），
   按 Tab+Backspace 进入正常 UI，据此判断 `TOP` 在上还是在下。
