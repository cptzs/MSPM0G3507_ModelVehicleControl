# User_UI - 注册表驱动 OLED UI

## 概述

`User_UI` 提供 128x64 OLED 上的分级菜单和业务页面框架。UI 每 5ms 由 `USER_UI_Task()` 调度，页面由注册表统一描述，菜单由只读表驱动。

当前 UI 入口为 `MAIN MENU`，进入路径为：

```text
MAIN MENU -> SUB MENU -> PAGE VIEW / PLACEHOLDER
```

## 文件说明

| 文件 | 功能 |
|------|------|
| `user_ui_public.h` | 对外公开 `USER_UI_Init()` / `USER_UI_Task()` |
| `user_ui_internal.h` | UI 内部类型、页面枚举、页面描述符和内部函数声明 |
| `user_ui_core.c` | 5ms 主循环、视图状态切换、按键消费、动态刷新、OLED service |
| `user_ui_core_state.c` | 当前视图、当前页面、菜单光标和静态重绘状态 |
| `user_ui_menu.c/h` | 一级/二级菜单表、菜单绘制、同分类页面查找 |
| `user_ui_dispatch.c` | 页面注册表分发：静态绘制、动态刷新、按键和生命周期 |
| `user_ui_input.c` | BoardIO 按键事件到 UI 事件的转换 |
| `user_ui_pages.c` | 13 个页面描述符注册表 |
| `user_ui_page_*.c` | 各页面静态布局、动态刷新和页面内按键逻辑 |
| `user_ui_route_context.c` | 路线启动交互状态机 |
| `user_ui_route_map.c/h` | 路线示意图图元表 |
| `user_ui_unittest_actions.c/h` | Hardware Test 动作封装 |

## 一级菜单

| 一级分类 | 用途 |
|----------|------|
| Run Route | 固定路线选择与启动 |
| Auto Pilot | 自动驾驶算法预留入口 |
| Sensors | 传感器观察、校准和输入诊断 |
| Service | 工程维护、执行器手动控制、硬件测试和系统诊断 |
| Setting | PID、运动参数和系统参数设置预留 |

## 二级页面

| 一级分类 | 二级项目 | 页面 |
|----------|----------|------|
| Run Route | Fixed Route | `PAGE_TEMPLATE` |
| Auto Pilot | Photo Pilot | SOON |
| Auto Pilot | Vision Pilot | SOON |
| Auto Pilot | Fusion Pilot | SOON |
| Sensors | Encoder Data | `PAGE_ENCODER` |
| Sensors | Photo Sensors | `PAGE_PHOTOELECTRIC` |
| Sensors | IMU Data | `PAGE_GYROSCOPE` |
| Sensors | IMU Sum Data | `PAGE_IMU_SUM` |
| Sensors | Smart Camera | `PAGE_CAMERA` |
| Sensors | ADC Data | `PAGE_ADC` |
| Sensors | LiDAR Sensors | `PAGE_LIDAR` |
| Service | Hardware Test | `PAGE_UNITTEST` |
| Service | Motor PID Mon | `PAGE_MOTOR` |
| Service | Servo Manual | `PAGE_SERVO` |
| Service | Thread Stats | `PAGE_THREADS` |
| Service | SysInfo | `PAGE_SYSINFO` |
| Setting | Motor PID / Motion Params / Servo Params / System | SOON |

当前页面注册表共 13 页：

```text
Motor PID Mon, Encoder Data, Photo Sensors, ADC Data, LiDAR Sensors,
IMU Data, IMU Sum Data, Smart Camera, Servo Manual, Threads,
SysInfo, Template Path, UnitTest
```

## 按键规则

| 位置 | 按键 | 行为 |
|------|------|------|
| MAIN MENU | UP/DOWN 或 PREV/NEXT | 移动一级分类 |
| MAIN MENU | ENTER | 进入二级列表 |
| SUB MENU | UP/DOWN 或 PREV/NEXT | 移动二级项目 |
| SUB MENU | ENTER | 打开页面或 SOON 占位 |
| SUB MENU | ESC | 返回 MAIN MENU |
| PAGE VIEW | PREV/NEXT | 仅切换当前分类内相邻已实现页面 |
| PAGE VIEW | ESC | 返回所属二级列表 |
| PLACEHOLDER | ESC | 返回所属二级列表 |

页面内的 UP/DOWN/LEFT/RIGHT/ENTER 继续交给当前页面 `on_key` 回调处理。

## 页面摘要

| 页面 | 标题 | 交互 |
|------|------|------|
| `PAGE_MOTOR` | Motor PID Mon | 只读显示左右电机 PID 状态 |
| `PAGE_ENCODER` | Encoder Data | UP/DOWN 选择编码器，ENTER 清零累计里程 |
| `PAGE_PHOTOELECTRIC` | Photo Sensors | UP/DOWN 选择 HIGH/LOW，LEFT/RIGHT 调阈值，ENTER 自动校准 |
| `PAGE_ADC` | ADC Data | 只读显示 ADC/电压/温度 |
| `PAGE_LIDAR` | LiDAR Sensors | UP/DOWN 选择 LiDAR |
| `PAGE_GYROSCOPE` | IMU Data | ENTER 短按设置 angle reference，长按设置 yaw reference |
| `PAGE_IMU_SUM` | IMU Sum Data | ENTER 清零累计量 |
| `PAGE_CAMERA` | Smart Camera | 只读/预留 |
| `PAGE_SERVO` | Servo Manual | ENTER 进入/退出 MAN，UP/DOWN 选舵机，LEFT/RIGHT 以 1 度步进调整 |
| `PAGE_THREADS` | Threads | UP/DOWN 滚动，ENTER 长按清除最大耗时 |
| `PAGE_SYSINFO` | SysInfo | 显示 CPU、LOAD、RAM、FLASH、FW 版本 |
| `PAGE_TEMPLATE` | Template Path | ENTER 长按启动路线，ESC 忙碌时取消 |
| `PAGE_UNITTEST` | UnitTest | UP/DOWN 选择测试项，ENTER 执行 |

## 路线启动交互

```text
IDLE
  -> ENTER 长按 2000ms
  -> WAIT_RELEASE
  -> 离手后倒计时 1000ms
  -> USER_Race_RequestStart()
  -> IDLE
```

充电、等待释放或倒计时阶段按 `ESC` 会取消流程。离开路线页面时，`on_exit` 也会取消未完成的启动流程。

## SysInfo 页面

`SysInfo` 位于 Service 分类，显示：

- CPU 型号：`USER_FIRMWARE_CPU_MODEL`
- CPU 主频：`USER_Firmware_GetCpuClockHz()`
- CPU 利用率：`USER_OS_GetCpuLoadPermille()`
- RAM 占用：`USER_Firmware_GetRamUsedBytes()` / `USER_Firmware_GetRamUsedPermille()`
- FLASH 占用：`USER_Firmware_GetFlashUsedBytes()` / `USER_Firmware_GetFlashUsedPermille()`
- 固件版本：`USER_Firmware_GetVersion()`

## 添加新页面

1. 创建 `user_ui_page_xxx.c`，实现静态绘制、动态刷新和按键回调。
2. 在 `user_ui_internal.h` 增加函数声明，必要时增加页面枚举。
3. 在 `user_ui_pages.c` 注册页面描述符。
4. 在 `user_ui_menu.c` 把页面加入合适的一级分类。
5. 在 `basic.ewp` 的 `User_UI` 分组加入新的 `.c` 文件。
6. 使用 IAR `Debug` 配置全量编译验证。
