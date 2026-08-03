# CH582_VIAL_PAD — 财务专用三模数字小键盘

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

基于 WCH CH582F 的财务专用数字小键盘固件，支持三模（USB 有线 / 蓝牙 BLE / 2.4G 无线），使用 VIAL 进行键值配置。工程由 MounRiver Studio 生成与管理。

- **参考工程**：[基于CH582M的三模兼容VIAL改键小键盘](https://oshwhub.com/bluetooth-keyboard-squad/the-first-stop-of-the-three-mode-keyboard)
- **目标芯片**：CH582F（CH582/CH583 系列，SFR 与 startup 共用 CH583 资源）
- **开发环境**：MounRiver Studio（RISC-V GCC 工具链，`riscv-none-embed-`）
- **当前验证通过版本**：`v0.3`（2026-08-03）— USB 枚举 + Vial 桌面通信 + 键盘扫描 + HID 输出 + ST7789 屏幕 UI（时钟/状态/上位机）均正常
- **v0.4 目标**：屏幕 UI 迁移至 **LVGL 8.3.x**（三模全量重构，见 §8 LVGL 项目计划书）

<p align="center">
  <img src="Reference\FinPad22.png" alt="CH582 VIAL PAD 预览" width="600"/>
</p>

---

## 一、硬件引脚分配（WeAct CH582F CoreBoard）

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
| PB23 | RST | 复位（与 ST7789 共享复位信号） |

> 以上 8 个引脚不可重新分配。

### 1.3 自由分配（14 pin → 矩阵 10 + ST7789 4）

> 引脚分配以 PCB 布线便利性为导向，Row 按 PA4→PA5→PA15→PA14→PA13→PA12 连续排列，Col 按 PB12→PB13→PB14→PB15 连续排列，ST7789 SCK/MOSI 放相邻 PA9/PA8。

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
│ ST7789 SPI    │ PA9      │ SCK (软件模拟时钟)    │ 输出     │
│   (bit-bang)  │ PA8      │ MOSI (软件模拟数据)   │ 输出     │
│               │ PB7      │ DC  (数据/命令选择)   │ 输出     │
│               │ PB4      │ BL  (背光 PWM10 调光) │ 输出     │
├───────────────┼──────────┼──────────────────────┼──────────┤
│ ST7789 控制   │ PB23     │ RST (与MCU共享复位)   │          │
│               │ GND      │ CS  (唯一SPI设备常低) │          │
└───────────────┴──────────┴──────────────────────┴──────────┘
```

### 1.4 ST7789 接线说明

| 屏幕信号 | 连接 | 说明 |
|----------|------|------|
| SCK | PA9 | GPIO bit-bang 模拟 SPI 时钟 |
| SDA/MOSI | PA8 | GPIO bit-bang 模拟 SPI 数据（与 SCK 相邻，方便走线） |
| DC | PB7 | 数据/命令选择（GPIO 推挽输出） |
| BL | PB4 | 背光控制，映射 TMR2 PWM10 通道实现调光 |
| CS | GND | 唯一 SPI 设备，直接常低 |
| RST | PB23 | 与 CH582 共享上电复位（共用 10K 上拉 + 100nF 对地） |

ST7789 模式配置脚：**IM[2:0] = 010** → 4 线 SPI（有独立 DC，无需 3 线 9-bit 模式）。

### 1.5 未使用 / 预留

| 引脚 | 状态 | 说明 |
|------|------|------|
| PB8 | 未引出 | WeAct 板 QFN28 未 bond，不存在 |
| PB9 | 未引出 | 同上 |
| WS2812 灯带 | 已砍 | 无可用引脚，为功耗考虑移除灯效 |

### 1.6 设计注意事项

1. **烧录方式**：PB14/PB15 被矩阵列占用，**无法使用 SWD 调试/烧录**。程序通过 **USB ISP** 烧录：按住 PB22(BOOT) 重新上电 → WCHISPTool 下载。
2. **PB14/PB15 作列输入**：配置为内部上拉输入，默认高电平，不会误触发 SWD 模式。但 PCB 上严禁外接强下拉电阻或对地电容（矩阵按键到 row 的通路本身不构成下拉，安全）。
3. **SPI 性能**：PA9/PA8/PB7 不在同一硬件 SPI 控制器上，使用 **GPIO bit-bang** 驱动 ST7789。2.25" 76×284 屏刷新足够（约 10~20fps），无需硬件 SPI。
4. **Row 全在 GPIOA**：PA4~PA5~PA15~PA14~PA13~PA12 连通，一次 `R32_PA_OUT` 端口操作即可设所有 row 电平，扫描效率最高。
5. **Col 全在 GPIOB**：PB12~PB13~PB14~PB15 连续排列，一次 `R32_PB_PIN` 读端口即可取所有 col 状态，布线最短。

---

## 二、移植完成情况（2026-07-28）

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

## 三、构建与烧录

1. MounRiver Studio 打开本工程。
2. **刷新工程（F5）→ Project > Clean → Build**（`obj/` 已清空，会全量重编）。
3. 产物：`obj/CH582_VIAL_PAD.elf` / `.hex` / `.map`（USB ISP 烧录还需 `.bin`，生成方法见第七节 7.5）。
4. 硬件 Debug：使用 `.launch` 配置（OpenOCD + WCH-RISCV 调试器），SVD 为 `CH58Xxx.svd`。

> 若链接报 `undefined reference to TMR2_PWMInit / TMR3_TimerInit` 等 SDK 函数，原因是 StdPeriphDriver 排除列表误排了对应 `.c` 文件，确认该 entry 无 `excluding` 即可。
> 若报其它 `undefined reference`（库之间循环依赖），可将三个库包进 `-Wl,--start-group ... -end-group`。

---

## 四、三模切换逻辑（当前状态：USB 已验证，BLE/2.4G 待测）

**当前 `main()`（`APP/hidkbd_main.c`）固定进入 USB 模式**：首次上电默认 USB（需用 Vial 桌面版通讯改键）。BLE / 2.4G 模式初始化分支尚未在 `main()` 中接线，**尚未成功验证**（见 §7.12 待恢复功能）。

模式切换代码在 `USB_MODE.c` / `BLE_MODE.c` / `RF_MODE.c` 均已实现：长按切换键约 2s 将模式字节写入 flash 后复位。BLE/2.4G 的初始化引导（`CH58X_BLEInit()`/`HAL_Init()`/GAP + HID 服务注册 / `RF_Init()`）位于 `BLE_MODE.c`/`RF_MODE.c` 内，待 `main()` 分支接线后启用。

| 切换键（物理键） | 写入模式字节 | 对应模式 |
|---|---|---|
| `key_data_buf[2][0]`（7） | `0x0B` | USB 有线 |
| `key_data_buf[2][1]`（8） | `0xBE` | 蓝牙 BLE |
| `key_data_buf[2][2]`（9） | `0x24` | 2.4G 无线 |

切换键默认键值即 **KP_7 / KP_8 / KP_9**（财务布局，见 §5.1），**无需先经 VIAL 配置**。长按计数 `change_mode_USB/BLE/24`（`HAL/scan_key.c`）达阈值后写模式字节并 `SYS_ResetExecute()`。

> **待办**：恢复 `main()` 读取模式字节（EEPROM `0x3F00`）并按值分支 USB / BLE / 2.4G（`USB_INIT()` / BLE 引导 / `RF_Init()`）；当前 main 为纯 USB。

---

## 五、后期改数字小键盘的计划（待办）

当前代码是参考工程的**全键盘**实现，需按财务小键盘的实际硬件裁剪。以下为后续工作清单：

### 5.1 矩阵适配 ✅ 已完成
- 文件：`HAL/include/scan_key.h`、`HAL/scan_key.c`
- 当前配置：**6 行 × 4 列**，Row: PA4/PA5/PA15/PA14/PA13/PA12，Col: PB12/PB13/PB14/PB15
- `key_data_buf[6][4]` **uint16_t** 4 层键值表，支持 QMK 16-bit 修饰符键码（`QK_LSFT|KC_9` = 0x0226 等）
- 默认键值为财务小键盘布局：R0=`(` / `)` / `=` / Tab，R1~R5=完整数字键盘（NumLock, /, *, Del, 7~9, -, 4~6, +, 1~3, Enter, 0, .）
- 扫描时 `get_key_fanz()` 自动将 QMK 16-bit 键码拆解为 HID modifier byte + usage byte
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

#### 5.8.1 屏幕驱动 ✅ 已完成（CS 接地 + 横向 + 可缩放字体 + 防花屏）

- **型号**：2.25 寸 SPI 屏，ST7789 驱动，76×284 分辨率（**横向使用 284×76**）
- **文件**：`HAL/st7789.c`、`HAL/include/st7789.h`
- **驱动方式**：GPIO bit-bang SPI（PA9=SCK, PA8=MOSI, PB7=DC, PB4=BL）
- **CS 接地**：CS 直接接地，**无需 GPIO 脉冲，PA11 释放**（可复用回晶振/其它功能）
- **SPI mode 3**：CPOL=1（SCK 空闲高）+ CPHA=1（上升沿采样）
- **字节同步**：复位前拉低 SCK/MOSI 防毛刺 + DC=0 下连发 8×NOP 对齐字节边界（CS 接地的关键）
- **防花屏**：DISPON 前先写全黑 GRAM，上电直接黑屏无随机闪烁
- **方向**：MADCTL=0xF0（横向 MV=1 + MX=1 垂直翻转，竖屏对应 0x80）
- **字体**：v0.3 Adafruit 5×7 取模（bit6 顶）+ `ST7789_SetFontZoom()` 可缩放（1×/2×/3×/4×）
- **字体方向**：bit7 顶检查 + **无反转行序**（v0.3 原始配置），MADCTL 方向已校准
- **时钟布局**：HH:MM 4× + 秒 2×（底部对齐），日期校时后自动刷新
- **已实现**：init、全屏填充、矩形填充、画点、可缩放字符/字符串、水平/垂直线、背光控制
- **待办**：TMR2 PWM 调光

**屏幕模拟器**（`tools/tft_sim.html`）：PC 端实时预览首页 UI（284×76，浏览器打开）。改 `HAL/ui.c` 布局后同步调整本文件即可预览，无需编译烧录。注意反斜杠字符 key 需双反斜杠转义（`"\\"`）。

#### 5.8.2 UI 框架（进行中）

- **默认首页**：实时时钟（HH:MM:SS）+ HID 键盘状态（USB/BLE/2.4G 模式 + 按键输入）
- **功能1 计算器**：UI 框架（显示区 + 按键提示区），运算逻辑后续加
- **文件**：`HAL/ui.c`、`HAL/include/ui.h`
- **状态机**：首页 ↔ 计算器切换（组合键触发）
- **实时时钟**：CH582 RTC（`RTC_InitTime`/`RTC_GetTime`），主循环每秒刷新
- **待办**：计算器运算状态机、按键输入映射、模式切换逻辑
- **v0.4（进行中）**：整体迁移至 **LVGL 8.3.x**，自绘 `ui.c` 渲染层被 LVGL 屏幕对象替代（保留树内、编译开关回退），详见 §8 LVGL 项目计划书

#### 5.8.3 模式切换

- 计算器 ↔ 小键盘两种模式互切
- 模式切换时不改变 USB/BLE/2.4G 三模状态
- 计算器模式下的按键不触发 HID 键值上报

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

## 六、参考资源

- oshwhub 原项目：[基于CH582M的三模兼容VIAL改键小键盘](https://oshwhub.com/bluetooth-keyboard-squad/the-first-stop-of-the-three-mode-keyboard)
- WCH 官网：http://www.wch.cn （CH582 数据手册、MounRiver Studio、BLE 库说明）

---

## 七、调试记录（2026-07-29 ~ 2026-07-30）：USB 枚举 / VIAL 启动 / 标准 Vial 协议实现 / 三模切换

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

为 2.25" 76×284 ST7789P3 屏编写驱动（`HAL/st7789.c`），GPIO bit-bang SPI（PA9=SCK, PA8=MOSI, PB7=DC, PB4=BL）。

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

## 八、LVGL 项目计划书（2026-08-03）：三模全量 UI 重构（v0.4 目标）

> 屏幕 UI 原为自绘方案（`HAL/ui.c` + `HAL/st7789.c`，v0.3 已验证）。为支持更复杂的控件、布局与交互，决定引入 **LVGL 8.3.x** 重构 UI。本计划书与现状（§5.8 自绘 UI、§5.9 自定义上位机、§7.13 屏幕驱动）呼应，整体重新规划架构；**实施不影响当前已验证代码**，可一键回退。

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
│  驱动层  HAL/st7789.c（现状保留 + 新增 Flush 批量写）     │
│  HAL/lvgl_port.c —— 显示/输入/节拍移植层                 │
└────────────────────────────────────────────────────────┘
```

### 8.5 与现状的边界（不影响当前代码）

| 类别 | 文件 | 处理 |
|------|------|------|
| **保留不动** | `HAL/st7789.c` 底层（init/CASET/RASET/写像素）、`HAL/scan_key.c` 矩阵扫描、`APP/USB_MODE.c` 三模与 0xE1-E5、EEPROM 布局（keymap 0x3000 / mode 0x3F00 / 文字 0x3F10） | 原样 |
| **替换** | `HAL/ui.c` 渲染层 → LVGL 屏幕对象 | 源码保留树内，`LVGL_EN` 编译开关回退 |
| **新增** | `LVGL/` 源码、`HAL/lvgl_port.c`、`HAL/lv_port_indev.c`、`HAL/ui_lvgl.c`、`tools/font/` 像素字体（8/16/32px） | 新增 |

### 8.6 显示驱动移植（HAL/lvgl_port.c）

- **局部刷新缓冲**：全帧放不下 → 单缓冲 **284×10×2 = 5,680B**（≈1/7.6 屏）；RAM 紧张可降 284×8×2=4,544B。`lv_disp_draw_buf_init(&buf, buf, NULL, 2840)`。
- **flush_cb**：`disp_flush()` → 新增 `ST7789_Flush(x,y,w,h,buf)`（一次 `ST7789_SetWindow` + DC 高 + 紧循环整块发送，比逐像素窗口快）→ `lv_disp_flush_ready()`。
- **RGB565 字节序**：`LV_COLOR_16_SWAP=1` 匹配 ST7789（MSB first），烧录校准一次。
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

### 8.10 内存预算（32K 硬约束）

| 项 | USB 模式 | BLE/2.4G 模式 |
|----|---------|---------------|
| 基础静态（现状 .data/.bss） | 4.8KB | +6KB（MEM_BUF）≈ 10.8KB |
| LVGL 显示缓冲 | 5.7KB | 5.7KB（可降 4.5KB） |
| LVGL 内存池 `LV_MEM_SIZE` | 6-8KB | 6KB |
| LVGL 静态 + 栈增量 | ~1.5KB | ~1.5KB |
| **合计** | **≈ 18-20KB** ✅ | **≈ 24KB** ⚠️ 紧张但可行 |

> BLE 模式若溢出：降显示缓冲至 284×8（4.5KB）、`LV_MEM_SIZE` 至 4KB、关闭未用 widget 与断言宏。以 `obj/*.map` 的 `_end` 实测为准，每里程碑核验。
> **FLASH**：当前 15KB + LVGL core（裁剪后 ~120-200KB）+ 字体 ~10KB + BLE 代码（启用时）→ 448K 富余。

### 8.11 里程碑 M0-M9

| 里程碑 | 内容 | 验证 |
|--------|------|------|
| **M0** | 下载 lvgl 8.3.11 入 `LVGL/`，写 `lv_conf.h`，`.cproject` 加 sourceEntry（excluding 裁剪）+ `-I` 与宏；仅编译通过，不改功能 | MounRiver Build 通过 |
| **M1** | `lvgl_port.c` 显示端口 + `ST7789_Flush` 批量写；纯色/色块/全黑清屏 | 屏显色块，测刷新帧率 |
| **M2** | `lv_font_conv` 生成像素字体三档 + label 显示 ASCII | 与 v0.3 视觉一致 |
| **M3** | 首页全部改 LVGL 对象（模式/电池/时钟/日期/自定义文字），对照 `tools/tft_sim.html` | 视觉一致，RTC 校时生效 |
| **M4** | `lv_port_indev.c` 矩阵→keypad 输入，焦点移动 | 按键可驱动 UI |
| **M5** | 计算器：btnmatrix + 运算状态机（原 `UI_CalcProcessKeys` TODO） | 计算正确 |
| **M6** | 上位机 0xE1-E5 → 更新 LVGL label；EEPROM 逻辑不变 | `tools/ch582_host.py` 全命令验证 |
| **M7** | 三模集成 + 睡眠协调：共用屏幕，切模式只改状态 label，按需刷新与唤醒 | 三模互切 + 睡眠功耗实测 |
| **M8** | 内存/性能优化：裁剪 lv_conf、调缓冲、核验 .map、刷新周期调优 | `_end` < 32K，交互可接受 |
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
