# MSPM0G3507 智能车工程架构说明

> 本文档说明当前 MSPM0G3507 智能车工程的目录分层、模块职责、数据流和代码归属规则。
> 最后更新：2026-06-14，基于当前代码和 IAR 工程。

## 1. 工程架构总览

当前工程采用“TI 官方库 + 用户分层代码”的结构：

```text
basic - Debug
├─ TI_DriverLib              <- TI MSPM0 SDK 驱动库
├─ SysConfig Generated Files <- SysConfig 自动生成外设配置
├─ User_Peripheral           <- MCU 外设抽象层
├─ User_Devices              <- 设备驱动层
├─ User_Algorithm            <- 纯算法库
├─ User_OS                   <- 1ms 协作式调度器和固件信息
├─ User_Application          <- 状态估测、MCM、路线调度
├─ User_UI                   <- OLED UI 子系统
├─ globals.c/h               <- 全局变量和 1ms 数据处理
├─ main.c                    <- 主入口
└─ main.syscfg               <- SysConfig 项目文件
```

### 分层关系

```text
User_UI ───────────────┐
User_Application ──────┼─> User_Devices -> User_Peripheral -> TI DriverLib
User_Algorithm ────────┘
User_OS 负责时间基准、任务调度、CPU 负载和固件信息查询
```

设计规则：

- `User_Peripheral` 只封装 MCU 片内外设。
- `User_Devices` 封装具体传感器、执行器和 BoardIO 硬件。
- `User_Algorithm` 保持纯算法，不依赖硬件全局变量。
- `User_Application` 负责状态估测、运动控制和路线调度。
- `User_UI` 只通过公开 API 发起操作和显示诊断信息。

## 2. 启动流程

```text
main
  -> SYSCFG_DL_init
  -> USER_System_Init
  -> USER_OS_Init
  -> USER_GlobalData_Init
  -> USER_Device_Init
  -> USER_State_Init
  -> USER_UI_Init
  -> USER_Scheduler_RegisterTasks
  -> while (true)
       -> USER_OS_Run()
       -> 无任务时 USER_OS_IdleWait()
```

`USER_OS_IdleWait()` 内部执行二次任务就绪检查和 `__WFI()`，并累计空闲时间用于 CPU 利用率统计。

## 3. 目录职责

### User_Peripheral

| 文件 | 外设 | 职责 |
|------|------|------|
| `userlib_adc.c/h` | ADC12 | 多通道采样、温度/电压换算 |
| `userlib_can.c/h` | MCAN | CAN 收发和回调 |
| `userlib_pwm.c/h` | GPTIMER | PWM 频率、占空比、启停 |
| `userlib_systick.c/h` | SysTick | 系统滴答、回调注册 |
| `userlib_uart.c/h` | UART + DMA | 串口收发、DMA 接收统计、回调 |

### User_Devices

| 文件 | 设备 | 职责 |
|------|------|------|
| `userlib_encoder.c/h` | 编码器 x3 | 周期采样、速度、方向、累计里程 |
| `userlib_imu.c/h` | 6 轴 IMU | UART 帧解析、姿态角、校准指令 |
| `userlib_lbb.c/h` | BoardIO | LED、按键、蜂鸣器；公开函数为 `USER_BoardIO_*` |
| `userlib_lidar.c/h` | LiDAR x4 | CAN 轮询、距离和强度解析 |
| `userlib_modbus.c/h` | Modbus 从机 | 寄存器读写、CRC、超时 |
| `userlib_motor.c/h` | 电机 x2 | 运行/制动/滑行模式和速度输出 |
| `userlib_oemt.c/h` | OEMT 模拟光电 x8 | ADC 采样、回差比较、自动校准 |
| `userlib_oled.c/h` | OLED 128x64 | GRAM、绘制 API、SPI DMA 刷新 |
| `userlib_fonts.c/h` | 字库 | ASCII 字模 |
| `userlib_servo.c/h` | 舵机 x4 | 角度控制和脉宽换算 |

### User_OS

| 文件 | 职责 |
|------|------|
| `user_os.c/h` | 1ms 协作式调度器、任务统计、WFI 空闲负载统计 |
| `user_firmware_info.c/h` | 固件版本、CPU/RAM/FLASH 信息 |

### User_Application

| 文件 | 职责 |
|------|------|
| `userapp_vehicle_model.h` | 车辆几何和编码器标定常量 |
| `userapp_state_estimator.c/h` | 统一车辆状态估测 |
| `userapp_mcm.c/h` | 非阻塞运动控制管理器 |
| `userapp_race_table.c/h` | 路线动作表解释器 |
| `userapp_race.c/h` | 路线启动请求和周期推进 |

### User_UI

注册表驱动 OLED UI。当前 13 页，包括 `SysInfo`、`Thread Stats`、`Hardware Test`、传感器页面和路线页面。

## 4. 全局数据

| 变量 | 类型 | 说明 |
|------|------|------|
| `encoder_data[3]` | `EncoderData_t_Typedef` | 路程计、左轮、右轮编码器 |
| `imu_data` | `IMU_Data_StructTypeDef` | IMU 姿态和角速度数据 |
| `speed_pid[2]` | `PID_Control_Struct_TypeDef` | 左右轮速度 PID |
| `distance_pid` | `PID_Control_Struct_TypeDef` | 直线距离 PID |
| `angle_pid` | `PID_Control_Struct_TypeDef` | 旋转角度 PID |
| `adc_data[7]` | `uint16_t` | ADC 原始值 |
| `adc_voltage[7]` | `uint16_t` | ADC 电压值 |
| `lidar_data[5]` | `Lidar_Data_Typedef` | LiDAR 数据，索引 0 保留 |
| `oemt_data[8]` | `uint16_t` | OEMT 二值化结果 |
| `modbus_regs[128]` | `uint16_t` | Modbus 保持寄存器 |
| `modbus_status` | `uint8_t` | Modbus 通信状态 |

`USER_GlobalData_Task()` 每 1ms 执行全局数据换算。

## 5. 数据流

```text
传感器原始数据
    -> USER_State_Task，每 10ms 发布 USER_STATE_Estimate_t
    -> USER_MCM_Task，每 10ms 生成车体/轮速命令
    -> PID 和电机驱动

UI 路线页面
    -> USER_Race_RequestStart()
    -> USER_Race_Task()
    -> USER_Race_TableExecutor_Update()
    -> USER_MCM_StartStraight / USER_MCM_StartSpin
```

关键边界：

1. MCM 只消费 `USER_STATE_Estimate_t`，不直接读原始传感器。
2. Race 只通过 MCM 公开 API 启动和查询动作。
3. UI 只发起请求和显示状态，不直接控制运动闭环。

## 6. 任务调度

| 任务 | 周期 | 偏移 | 优先级 | 功能 |
|------|------|------|--------|------|
| `global` | 1ms | 0 | 0 | 全局数据处理 |
| `encoder` | 10ms | 2 | 1 | 编码器更新 |
| `state` | 10ms | 3 | 2 | 状态估测 |
| `mcm` | 10ms | 4 | 3 | 运动控制 |
| `race` | 10ms | 5 | 4 | 路线调度 |
| `lidar` | 5ms | 1 | 4 | LiDAR 轮询 |
| `ui` | 5ms | 2 | 6 | OLED UI 和 LED 心跳 |

## 7. UI 页面

Service 分类当前包含：

- `Hardware Test`
- `Motor PID Mon`
- `Servo Manual`
- `Thread Stats`
- `SysInfo`

`SysInfo` 显示 CPU 型号、主频、WFI 口径 CPU 利用率、RAM、FLASH 和固件版本。

## 8. 编译

```powershell
& "D:\Program Files\IAR\common\bin\iarbuild.exe" basic.ewp -build Debug
```
