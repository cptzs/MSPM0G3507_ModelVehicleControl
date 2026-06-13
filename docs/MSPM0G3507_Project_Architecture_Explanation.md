# MSPM0G3507 智能车工程架构说明

> 本文档说明当前 MSPM0G3507 智能车工程的目录分层、模块职责、数据流和代码归属规则。
> 最后更新：2026-06-13（基于最新代码状态）

---

## 1. 工程架构总览

当前工程采用"TI 官方库 + 用户分层代码"的结构：

```text
basic - Debug
├─ TI_DriverLib              ← TI MSPM0 SDK 驱动库（不修改）
├─ SysConfig Generated Files ← SysConfig 自动生成外设配置（不修改）
├─ User_Peripheral           ← MCU 外设抽象层（ADC/CAN/PWM/SysTick/UART）
├─ User_Devices              ← 设备驱动层（10个硬件模块）
├─ User_Algorithm            ← 纯算法库（PID）
├─ User_OS                   ← 1ms tick 协作式调度器
├─ User_Application          ← 应用层（状态估测/MCM/路线调度）
├─ User_UI                   ← OLED UI 子系统（12页注册表驱动）
├─ globals.c/h               ← 全局变量定义与 1ms 数据处理
├─ main.c                    ← 主入口：初始化→注册任务→调度循环
└─ main.syscfg               ← SysConfig 项目文件
```

### 分层关系

```text
User_Application     ← 应用层：状态估测、非阻塞 MCM、表格化路线调度
    ↑       ↑
    │   User_Algorithm   ← 算法层：PID、互补滤波(预留)、Kalman(预留)
    │       ↑
User_Devices         ← 设备层：编码器、IMU、电机、OLED、LiDAR、Modbus 等
    ↑
User_Peripheral      ← 外设层：ADC、CAN、PWM、SysTick、UART
    ↑
TI_DriverLib / SysConfig
```

### 设计目标

- **学生写 Application**：比赛流程、路线表填写
- **教师维护 Algorithm / Devices / Peripheral**：算法库、驱动封装、外设抽象
- **UI 通过注册表扩展**：新增页面只需注册，不改导航框架

---

## 2. 启动流程

```c
main()
  ├─ SYSCFG_DL_init()               // SysConfig 外设初始化
  ├─ USER_SYSTEM_Init()             // 系统时钟 + SysTick 启动
  ├─ USER_OS_Init()                 // 调度器初始化（注册 SysTick 回调）
  ├─ USER_GlobalData_Init()         // 全局数据清零
  ├─ USER_Device_Init()             // 设备初始化（IMU/编码器/电机/OLED/LiDAR...）
  ├─ USER_STATE_Init()              // 状态估测器初始化
  ├─ USER_RegisterSchedulerTasks()  // 注册 7 个调度任务
  └─ while(1) USER_OS_Run()         // 主循环：协作式调度
```

## 3. 目录职责说明

### 3.1 TI_DriverLib（不修改）

TI 官方 MSPM0 DriverLib。不随意修改，不添加业务逻辑。

### 3.2 SysConfig Generated Files（不修改）

SysConfig 自动生成的 `ti_msp_dl_config.c/h`。含引脚、外设、中断、时钟配置。需调整时通过 SysConfig 工具修改。

### 3.3 User_Peripheral — MCU 外设抽象层

| 文件 | 外设 | 职责 |
|------|------|------|
| `userlib_adc.c/h` | ADC12 | 7 通道 ADC 采样、温度/电压换算 |
| `userlib_can.c/h` | MCAN | CAN 收发、中断回调 |
| `userlib_pwm.c/h` | GPTIMER | PWM 占空比/频率设置 |
| `userlib_systick.c/h` | SysTick | 系统滴答、回调注册 |
| `userlib_uart.c/h` | UART+DMA | 串口 DMA 收发、回调注册 |

规则：只封装 MCU 内部资源；不写比赛逻辑/设备业务/状态机；不依赖 User_Application。

### 3.4 User_Devices — 设备驱动层

| 文件 | 设备 | 职责 |
|------|------|------|
| `userlib_encoder.c/h` | 编码器×3 | 转速/方向/累计里程（0=路程计, 1=左轮, 2=右轮） |
| `userlib_imu.c/h` | 6轴IMU | UART 帧解析、姿态角、指令控制 |
| `userlib_lbb.c/h` | LED/按钮/蜂鸣器 | 8 按钮计数型事件、LED 闪烁、蜂鸣器 |
| `userlib_lidar.c/h` | LiDAR×4 | CAN 轮询、距离/信号强度 |
| `userlib_modbus.c/h` | Modbus 从机 | 128 寄存器、03/06/10 功能码、CRC |
| `userlib_motor.c/h` | 电机×2 | 三模式：运行/制动/滑行 |
| `userlib_oemt.c/h` | 模拟光电×8 | ADC 回差比较二值化 |
| `userlib_oled.c/h` | OLED 128×64 | SPI DMA 全帧缓冲、图形/文字绘制 |
| `userlib_fonts.c/h` | 字库 | 6×8 和 8×16 ASCII 字模 |
| `userlib_servo.c/h` | 舵机×4 | 角度控制、脉宽换算 |

规则：可调用 User_Peripheral 和 User_Algorithm；不反向依赖 User_Application；不写比赛状态机。

### 3.5 User_Algorithm — 算法库

| 文件 | 职责 |
|------|------|
| `userlib_pid.c/h` | 位置式/增量式/高级 PID、斜坡控制、清零 |

规则：不依赖任何硬件全局变量；不假设物理单位；保持纯数学、可移植。

**后续扩展**（参见 `algorithm_filter_design_plan_zh.md`）：互补滤波器、Kalman/EKF、矩阵运算。

### 3.6 User_OS — 协作式调度器

1ms tick 非抢占式。7 个任务按优先级调度，支持错峰 offset 和微秒级耗时统计。详见 `User_OS/README.md`。

### 3.7 User_Application — 应用层

| 文件 | 职责 |
|------|------|
| `userapp_vehicle_model.h` | 车辆几何参数与编码器标定常量 |
| `userapp_state_estimator.c/h` | 统一车辆状态估测（唯一读传感器） |
| `userapp_mcm.c/h` | 非阻塞运动控制管理器 |
| `userapp_race_table.c/h` | 表格化路线调度器 |
| `userapp_race.c/h` | 比赛路线启动管理 |

详见 `User_Application/README.md`。

### 3.8 User_UI — OLED UI 子系统

注册表驱动 12 页框架。PREV/NEXT 翻页，按键事件消费分发，静态/动态分离绘制。详见 `User_UI/README.md`。

---

## 4. 全局数据 (`globals.c/h`)

集中存储所有运行时数据：

| 变量 | 类型 | 说明 |
|------|------|------|
| `encoder_data[3]` | `EncoderData_t_Typedef` | 3 路编码器数据 |
| `imu_data` | `IMU_Data_StructTypeDef` | IMU 姿态传感器数据 |
| `speed_pid[2]` | `PID_Control_Struct_TypeDef` | 左右轮速度 PID |
| `distance_pid` | `PID_Control_Struct_TypeDef` | 距离 PID（MCM 直行反馈用） |
| `angle_pid` | `PID_Control_Struct_TypeDef` | 角度 PID（MCM 旋转反馈用） |
| `adc_data[7]` | `uint16_t` | ADC 原始采样值 |
| `adc_voltage[7]` | `uint16_t` | ADC 电压换算值 (mV) |
| `lidar_data[5]` | `Lidar_Data_Typedef` | LiDAR 数据（索引 0 保留） |
| `oemt_data[8]` | `uint16_t` | 光电传感器比较结果 |
| `modbus_regs[128]` | `uint16_t` | Modbus 保持寄存器 |
| `modbus_status` | `uint8_t` | Modbus 通信状态 |

`USER_GlobalData_Task()` 每 1ms 执行 ADC→电压/温度换算。

---

## 5. 数据流

```text
传感器原始数据 (globals.c)
    │
    ▼
State Estimator (每10ms) ──► USER_STATE_Estimate_t
    │
    ▼
MCM (每10ms) ──► 速度PID目标 ──► 电机
    ▲
    │ StartStraight/StartSpin
    │
Race 调度器 (每10ms)
    ▲
    │ RequestStart
    │
OLED UI (每5ms) ──► ENTER长按 ──► 充电→倒计时→启动
```

### 关键数据边界

1. **传感器→状态估测器**：唯一桥接点。MCM 不直接读传感器。
2. **状态估测器→MCM**：通过 `USER_STATE_Estimate_t` 单向传递。
3. **Race→MCM**：通过 `USER_MCM_Start*()` / `USER_MCM_IsBusy()` / `USER_MCM_GetStatus()` 交互。
4. **UI→Race**：通过 `USER_Race_RequestStart()` 提交启动请求。

---

## 6. 任务调度

7 个任务在 1ms tick 协作式调度器下运行：

| 任务 | 周期 | 优先级 | 功能 |
|------|------|--------|------|
| `global` | 1ms | 0 | ADC 数据换算 |
| `state` | 10ms | 1 | 车辆状态估测 |
| `mcm` | 10ms | 2 | 非阻塞运动控制 |
| `race` | 10ms | 3 | 路线调度推进 |
| `lidar` | 5ms | 4 | LiDAR 轮询 |
| `ui` | 5ms | 6 | OLED UI 刷新 + 500ms LED 心跳 |

---

## 7. 编译

```bash
& "D:\Program Files\IAR\common\bin\iarbuild.exe" basic.ewp -build Debug -log errors
```
