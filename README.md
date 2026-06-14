# MSPM0G3507 智能小车模型控制系统

基于 **TI MSPM0G3507** (ARM Cortex-M0+, 80MHz, 128KB Flash / 32KB SRAM) 的两轮差速模型车控制工程。

当前工程采用分层架构：外设抽象 -> 设备驱动 -> 算法库 -> 1ms 协作式调度器 -> 状态估测 -> 非阻塞 MCM -> 表格化路线调度 -> 注册表驱动 OLED UI。

## 当前特性

- **注册表驱动分级 OLED 菜单**：上电进入 `MAIN MENU`，一级分类为 `Run Route`、`Auto Pilot`、`Sensors`、`Service`、`Setting`。
- **13 个业务页面**：包含传感器诊断、路线启动、硬件测试、线程统计和 `SysInfo` 系统信息页面。
- **Service/SysInfo 页面**：显示 CPU 型号、CPU 主频、基于 `__WFI()` 空闲时间统计的 CPU 利用率、RAM/FLASH 占用和固件版本。
- **非阻塞路线启动**：`Fixed Route` 页面通过 `ENTER` 长按进入充电、等待释放、倒计时和路线启动流程，忙碌阶段 `ESC` 可取消。
- **线程统计页面**：`Thread Stats` 页面显示协作式调度任务运行统计，支持滚动查看和清除最大耗时。
- **版本记录**：`VERSION` 保存当前版本号，`CHANGELOG.md` 保存版本变更历史，固件内版本定义位于 `User_OS/user_firmware_info.h`。

## 硬件资源

| 资源 | 规格 | 占用 |
|------|------|------|
| MCU | MSPM0G3507, Cortex-M0+ @ 80MHz | 128KB Flash / 32KB SRAM |
| 编译器 | IAR EWARM | 工程文件 `basic.ewp` |
| 传感器 | 6 轴 IMU (UART0)、编码器 x3、模拟光电 x8、LiDAR x4 (CAN) | 全局数据由 `globals.c/h` 持有 |
| 执行器 | 直流电机 x2、舵机 x4、板载 LED/蜂鸣器 | PWM/GPIO |
| 显示 | OLED 128x64 | SPI + DMA |
| 通信 | Modbus 从机、CAN | UART1 / MCAN |

## 工程架构

```text
ModelVehicleControl_AI/
├── main.c                   # 主入口：初始化 -> 注册任务 -> 调度/空闲睡眠循环
├── globals.c/h              # 全局运行数据和 1ms 全局数据处理
├── user_config.h            # 编译配置：状态估测方法等
├── VERSION                  # 当前固件版本号
├── CHANGELOG.md             # 固件版本变更历史
├── ti_msp_dl_config.c/h     # SysConfig 自动生成外设配置
├── mspm0g3507.icf           # IAR 链接脚本
├── basic.ewp / basic.eww    # IAR 工程文件
│
├── ti/driverlib/            # TI MSPM0 SDK DriverLib
├── iar/                     # IAR 启动文件
│
├── User_Peripheral/         # ADC、CAN、PWM、SysTick、UART
├── User_Devices/            # 编码器、IMU、BoardIO、LiDAR、Modbus、电机、OEMT、OLED、舵机、字库
├── User_Algorithm/          # PID 等纯算法模块
├── User_OS/                 # 1ms 协作式调度器 + 固件/资源信息
├── User_Application/        # 状态估测、MCM、路线调度、车辆模型
├── User_UI/                 # 注册表驱动分级 OLED UI
└── docs/                    # 架构、UI、运动控制和路线设计文档
```

## 启动流程

```text
SYSCFG_DL_init
  -> USER_System_Init
  -> USER_OS_Init
  -> USER_GlobalData_Init
  -> USER_Device_Init
  -> USER_State_Init
  -> USER_UI_Init
  -> USER_Scheduler_RegisterTasks
  -> while(1): USER_OS_Run 或 USER_OS_IdleWait
```

无到期任务时，主循环进入 `USER_OS_IdleWait()`。该函数在 OS 层完成二次就绪检查、`__WFI()` 睡眠和空闲时间累计，`SysInfo` 页面显示的 CPU 利用率按 `1 - WFI idle time / window time` 计算。

## 分层数据流

```text
传感器原始数据 (encoder_data[] / imu_data / adc_data[] / lidar_data[])
    -> State Estimator，每 10ms 发布 USER_STATE_Estimate_t
    -> MCM，每 10ms 执行非阻塞运动控制
    -> Race，每 10ms 推进路线动作表
    -> Motor/PID，输出左右轮速度命令

BoardIO 按键事件
    -> UI input adapter
    -> UI core
    -> 菜单或当前页面 on_key 回调
```

## 任务调度表

| 任务名 | 周期 | 偏移 | 优先级 | 功能 |
|--------|------|------|--------|------|
| `global` | 1ms | 0 | 0 | ADC 电压/温度换算等全局数据处理 |
| `encoder` | 10ms | 2 | 1 | 编码器周期采样和速度/里程更新 |
| `state` | 10ms | 3 | 2 | 统一车辆状态估测 |
| `mcm` | 10ms | 4 | 3 | 非阻塞运动控制 |
| `race` | 10ms | 5 | 4 | 路线动作表推进 |
| `lidar` | 5ms | 1 | 4 | LiDAR 轮询通信 |
| `ui` | 5ms | 2 | 6 | 分级 OLED UI、页面动态刷新、OLED Service 和 LED 心跳 |

## OLED UI 菜单

| 一级分类 | 二级项目 | 页面/状态 |
|----------|----------|-----------|
| `Run Route` | Fixed Route | `PAGE_TEMPLATE` |
| `Auto Pilot` | Photo Pilot / Vision Pilot / Fusion Pilot | SOON 占位 |
| `Sensors` | Encoder Data / Photo Sensors / IMU Data / IMU Sum Data / Smart Camera / ADC Data / LiDAR Sensors | 传感器诊断页面 |
| `Service` | Hardware Test / Motor PID Mon / Servo Manual / Thread Stats / SysInfo | 工程维护与系统诊断页面 |
| `Setting` | Motor PID / Motion Params / Servo Params / System | SOON 占位 |

## 编译

```powershell
& "D:\Program Files\IAR\common\bin\iarbuild.exe" basic.ewp -build Debug
```

## 各层详细说明

| 目录 | 说明 |
|------|------|
| [User_Peripheral/](User_Peripheral/README.md) | MCU 外设抽象层 |
| [User_Devices/](User_Devices/README.md) | 设备驱动层 |
| [User_Algorithm/](User_Algorithm/README.md) | 纯算法库 |
| [User_OS/](User_OS/README.md) | 协作式调度器、CPU 负载和固件信息 |
| [User_Application/](User_Application/README.md) | 状态估测、MCM、路线调度 |
| [User_UI/](User_UI/README.md) | 注册表驱动 OLED UI |
| [docs/](docs/) | 架构、路线、UI 和滤波设计文档 |

## 许可

MIT License.
