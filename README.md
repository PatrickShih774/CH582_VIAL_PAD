# CH582_VIAL_PAD — 财务专用三模数字小键盘

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

基于 WCH CH582F 的财务专用数字小键盘固件，支持三模（USB 有线 / 蓝牙 BLE / 2.4G 无线），使用 VIAL 进行键值配置。工程由 MounRiver Studio 生成与管理。

- **参考工程**：`C:\ClaudeProject\CH582_VIAL_KBD`（oshwhub 三模键盘开源项目）
- **目标芯片**：CH582F（CH582/CH583 系列，SFR 与 startup 共用 CH583 资源）
- **开发环境**：MounRiver Studio（RISC-V GCC 工具链，`riscv-none-embed-`）

---

## 一、移植完成情况（2026-07-28）

已将参考工程 `CH582_VIAL_KBD` 的三模键盘代码完整移植到本工程，保持标准 MounRiver Studio 工程结构（真实文件夹，非 Eclipse 链接资源）。

### 1.1 目录结构

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

### 1.2 `.cproject` 构建配置改动（5 处）

与 KBD 已验证可编译的配置保持一致：

- **Include 路径 (-I)**：`Startup`、`APP/include`、`Profile/include`、`StdPeriphDriver/inc`、`HAL/include`、`Ld`、`LIB`、`RVMSIS`
- **宏定义 (-D)**：`DEBUG=1`、`HAL_SLEEP=1`（启用低功耗睡眠）
- **库搜索路径 (-L)**：`../`、`../LIB`、`../APP`、`../StdPeriphDriver`
- **链接库 (-l)**：`ISP583` → `VIAL` → `CH58xBLE`
- **sourceEntries**：显式列出 `APP`、`HAL`(排除 `KEY.c`/`LED.c`)、`LIB`、`Ld`、`Profile`、`RVMSIS`、`Startup`、`StdPeriphDriver`(全量编译，不排除任何 `CH58x_*.c`)

> 注：KBD 参考工程的 `.cproject` 在 StdPeriphDriver 排除列表里误排了 `CH58x_timer2.c`/`CH58x_timer3.c`，而 `ws2812b.c` 的 `TMR2_PWMInit`、`USB_MODE.c` 的 `TMR3_TimerInit` 正是由这两个文件提供，会导致链接报 `undefined reference`。本工程已去掉该排除项，全量编译所有 CH58x 驱动文件（未用函数由 `--gc-sections` 自动裁剪）。

### 1.3 关键决策

1. **覆盖 PAD 模板 SDK**：两套 SDK 有 12 个文件不同（`CH583SFR.h`、`CH58x_common.h`、`startup_CH583.S`、`core_riscv.h` 等）。因 `LIBCH58xBLE.a`/`libVIAL.a` 是按 KBD 的 SDK 预编译的，使用 KBD 的 SDK 才能保证 ABI 一致、链接通过。已校验所有 `.a` 库与差异文件 MD5 与 KBD 完全一致。
2. **删除模板 `src/Main.c`**：原 UART 回显例程与 `APP/hidkbd_main.c` 中的 `main()` 冲突，必须移除。
3. **未改动** `.project` / `.launch` / `.wvproj`：`.launch` 仍指向 `obj\CH582_VIAL_PAD.elf`，调试配置有效。

---

## 二、构建与烧录

1. MounRiver Studio 打开本工程。
2. **刷新工程（F5）→ Project > Clean → Build**（`obj/` 已清空，会全量重编）。
3. 产物：`obj/CH582_VIAL_PAD.elf` / `.hex` / `.map`。
4. 硬件 Debug：使用 `.launch` 配置（OpenOCD + WCH-RISCV 调试器），SVD 为 `CH58Xxx.svd`。

> 若链接报 `undefined reference to TMR2_PWMInit / TMR3_TimerInit` 等 SDK 函数，原因是 StdPeriphDriver 排除列表误排了对应 `.c` 文件，确认该 entry 无 `excluding` 即可。
> 若报其它 `undefined reference`（库之间循环依赖），可将三个库包进 `-Wl,--start-group ... -end-group`。

---

## 三、三模切换逻辑

上电后 `main()`（`APP/hidkbd_main.c`）调用 `vial_init()` 从 flash 读取模式字节，按值进入对应模式：

| 模式字节 | 模式 | 初始化流程 |
|---|---|---|
| `0x0B` | USB 有线 | `USB_INIT()` |
| `0xBE` | 蓝牙 BLE | `CH58X_BLEInit()` → `HAL_Init()` → `GAPRole_PeripheralInit()` → `HidDev_Init()` → `HidEmu_Init()` |
| `0x24` | 2.4G 无线 | `CH58X_BLEInit()` → `HAL_Init()` → `RF_RoleInit()` → `RF_Init()` |
| 其它 | 复位 | `SYS_ResetExecute()` |

模式切换通过特定按键组合触发（见 `HAL/scan_key.c` 中的 `change_mode_USB/BLE/24`），写入 flash 后复位生效。
另外 `GPIOA_IRQHandler` 监听 PA5 下降沿作为硬件强制复位按键。

---

## 四、后期改数字小键盘的计划（待办）

当前代码是参考工程的**全键盘**实现，需按财务小键盘的实际硬件裁剪。以下为后续工作清单：

### 4.1 矩阵适配（高优先）
- 文件：`HAL/include/scan_key.h`、`HAL/scan_key.c`
- 现状：5 行 × 4 列（行在 PA：PA1/2/3/15/14/13，列在 PB：PB0~PB7），`key_data_buf[5][4]`
- 待办：按小键盘 PCB 实际行列数与引脚改 `row_x`/`col_x` 宏、`row_all`/`col_all`、`key_data_buf` 维度、`Scan_init()`/`get_key()` 扫描逻辑。

### 4.2 HID 描述符与键值表（高优先）
- 文件：`APP/USB_MODE.c`（USB HID）、`APP/BLE_MODE.c`（BLE HID）
- 待办：报告描述符改为数字小键盘（Keyboard + Numpad）；键值表映射改为小键盘按键（0~9、+、-、*、/、Enter、.、NumLock 等）；财务场景可能需要的组合键/宏。

### 4.3 模式切换组合键
- 文件：`HAL/scan_key.c`（`find_mode_changekey`、`change_mode_*`）
- 待办：按小键盘可用按键重新定义 USB/BLE/2.4G 切换组合键。

### 4.4 VIAL 键值配置
- `libVIAL.a` 为预编译库，VIAL 协议与键位存储在 flash。
- 待办：用 VIAL 配置工具生成小键盘布局并写入；核对 `vial_init()` 校验逻辑（校验不通过会复位，无法进入任何模式）。

### 4.5 RGB 灯效
- 文件：`HAL/ws2812b.c`、`HAL/include/ws2812.h`
- 待办：若小键盘硬件无 WS2812 灯，需裁掉 `Ws2812_Init()`/`process_RGB_to_pwm()`/`PWM_DATA_DMA_send()` 相关调用，释放 PWM/DMA 资源。

### 4.6 硬件引脚核对
- `hidkbd_main.c` 中 PA5 复位按键、调试串口 TXD1（PA9）等引脚需与小键盘原理图核对。
- `HAL_SLEEP=1` 下上电会将 PA/PB 全部配为上拉输入，确认睡眠唤醒引脚（扫描列/模式键）配置正确。

### 4.7 低功耗与电池（若需）
- `DCDC_ENABLE`、`HAL_SLEEP`、`BLE_SNV` 等参数在 `HAL/include/config.h`。
- 待办：按电池供电需求调整睡眠参数、RTC 唤醒时间、电池电量上报（`Profile/battservice.c`）。

---

## 五、参考资源

- 参考工程：`C:\ClaudeProject\CH582_VIAL_KBD`
- oshwhub 原项目：https://oshwhub.com/bluetooth-keyboard-squad/the-first-stop-of-the-three-mode-keyboard
- WCH 官网：http://www.wch.cn（CH582 数据手册、MounRiver Studio、BLE 库说明）
