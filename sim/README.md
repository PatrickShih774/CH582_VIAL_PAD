# NV3007 UI PC 模拟器（SDL2）

用 SDL2 在 PC 上直接运行**同一份** `HAL/bm_ui.c` + `HAL/bm_font.c`
绘图代码（仅加 `BM_SIM` 宏隔离 CH582 硬件依赖：TMR0/CH58x RTC 改为 PC
本地时钟），无需烧录即可验证页面渲染、主题与按键路由。

## 构建

1. `download_tools.bat`：下载免安装 MinGW-w64（w64devkit）与 SDL2 mingw
   开发包到 `sim/vendor/`（首次需要联网）。
2. `build.bat`：编译生成 `nv3007_sim.exe`（并复制 SDL2.dll）。
3. 双击 `nv3007_sim.exe` 运行。

## 按键（与真机路由一致）

| 按键 | 行为 |
| --- | --- |
| Tab + Backspace | 翻页：主页 -> 计算器 -> 设置 -> ... |
| 计算器页 0-9 + - * / . | 输入表达式 |
| Enter / 小键盘 Enter | `=` 计算 |
| Backspace | 退格 |
| Esc | 清除 `C`（计算器页）；其他页退出模拟器 |
| 设置页 1/2/3/4 | 亮度 / 睡眠 / 主题 / 重置连接 |
| M 或 F | 循环 USB / BT / RF 模式 |
| T | 任意页循环主题（快速验证） |
| B | 任意页循环亮度（快速验证） |

## 与真机的差异

- 模拟后端直接按逻辑 428×142 画帧缓冲，不做物理列转置（转置是
  `NV3007.c` 的 SPI 映射，PC 模拟不需要）。
- `ST7789_*` API 与 `NV3007.h` 宏完全复用；`NV3007.c`（GPIO bit-bang）
  不参与编译。
- RTC 使用 PC 本地时间；`g_bm_tick_ms` 由 SDL 主循环按实际经过毫秒累加。

## 目录

- `main.c`：SDL 窗口/纹理/键盘事件，驱动 `ui_init/ui_bm_process`
- `sim_st7789.c`：模拟 ST7789_* 后端（428×142 RGB565 帧缓冲）
- `bm_ui.c` / `bm_font.c`：来自 `HAL/`，原样编译（`BM_SIM` 分支）
