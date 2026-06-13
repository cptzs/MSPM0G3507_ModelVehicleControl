# MSPM0G3507 智能小车模型控制系统

基于 **TI MSPM0G3507** (ARM Cortex-M0+, 80MHz, 128KB Flash / 32KB SRAM) 的两轮差速模型车控制工程。

当前工程已全面切换到 **注册表驱动 UI 架构 + 非阻塞运动控制**，采用分层设计：
外设封装 → 设备驱动 → 算法库 → 协作式调度器 → 状态估测 → 非阻塞 MCM → 表格化路线调度 → OLED UI。

## 硬件资源

| 资源 | 规格 | 占用 |
|------|------|------|
| MCU | MSPM0G3507, Cortex-M0+ @ 80MHz | — |
| Flash | 128 KB | ~53 KB (42%) |
| SRAM | 32 KB | ~7 KB (21%) |
| 编译器 | IAR EWARM 9.70.4 | — |
| 传感器 | 6轴 IMU (UART)、编码器×3 (GPIO+Timer)、模拟光电×8 (ADC)、LiDAR×4 (CAN) | — |
| 执行器 | 直流电机×2 (PWM)、舵机×4 (PWM)、LED/蜂鸣器 (GPIO) | — |
| 显示 | OLED 128×64 (SPI+DMA) | — |
| 通信 | Modbus 从机 (UART1)、CAN | — |

## 工程架构

```
ModelVehicleControl_AI/
├── main.c                   # 主入口：初始化 → 注册任务 → while(1) 调度循环
├── globals.c/h              # 全局变量定义（传感器数据/PID/Modbus 寄存器）+ 1ms 全局数据处理
├── user_config.h            # 编译配置：状态估测方法选择
├── ti_msp_dl_config.c/h     # SysConfig 自动生成外设配置（不修改）
├── mspm0g3507.icf           # IAR 链接脚本
├── basic.ewp / .eww         # IAR 工程文件
│
├── ti/driverlib/            # TI MSPM0 SDK 驱动库（不修改）
├── iar/                     # IAR 启动文件 startup_mspm0g350x_iar.c
│
├── User_Peripheral/         # 外设抽象层：ADC、CAN、PWM、SysTick、UART
├── User_Devices/            # 设备驱动层：编码器、IMU、LBB、LiDAR、Modbus、电机、OEMT、OLED、舵机、字库
├── User_Algorithm/          # 算法库：PID 控制器（位置式/增量式/高级/斜坡）
├── User_OS/                 # 1ms tick 协作式调度器
├── User_Application/        # 应用层：状态估测、MCM、路线调度、车辆模型
├── User_UI/                 # UI 子系统：注册表驱动 OLED 12 页框架
│
├── docs/                    # 设计文档
└── tools/                   # 辅助脚本
```

## 分层数据流

```
┌──────────────────────────────────────────────────────────────┐
│ 传感器原始数据 (encoder_data[] / imu_data / adc_data[] / ...) │
│                      globals.c 全局存储                       │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│              State Estimator (每 10ms)                        │
│  唯一读取原始传感器的模块，发布统一 USER_STATE_Estimate_t       │
│  距离(mm) / 速度(mm/s) / 偏航角(deg) / 角速度(deg/s)          │
│  → 支持 raw / weighted / complementary / Kalman / EKF 切换    │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│              MCM 非阻塞运动控制 (每 10ms)                      │
│  消费 Estimate，驱动梯形速度规划 + 航向修正 → 速度 PID → 电机   │
│  支持直行(STRAIGHT) / 原地旋转(SPIN) / 圆弧(ARC,预留)          │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│              Race 路线调度 (每 10ms)                           │
│  表格驱动：逐行启动 MCM 动作 → 等待完成 → 跳转下一行            │
│  模板路线：直行50cm→180°掉头→直行50cm→180°回转                 │
└──────────────────────────────────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│              OLED UI (每 5ms)                                 │
│  12 页注册表驱动，PREV/NEXT 翻页，按键交互                     │
│  路线启动：ENTER 长按 → 充电 2s → 倒计时 2s → 比赛开始          │
└──────────────────────────────────────────────────────────────┘
```

## 任务调度表 (协作式 1ms tick)

| 任务名 | 周期 | 偏移 | 优先级 | 功能 |
|--------|------|------|--------|------|
| `global` | 1ms | 0 | 0 | ADC→电压/温度换算 |
| `state` | 10ms | 3 | 1 | 车辆状态估测 |
| `mcm` | 10ms | 4 | 2 | 非阻塞运动控制 |
| `race` | 10ms | 5 | 3 | 路线调度推进 |
| `lidar` | 5ms | 1 | 4 | LiDAR 轮询通信 |
| `ui` | 5ms | 2 | 6 | OLED UI 刷新 + 500ms LED 心跳 |

## 编译

```bash
& "D:\Program Files\IAR\common\bin\iarbuild.exe" basic.ewp -build Debug -log errors
```

## 各层详细说明

| 目录 | 说明 |
|------|------|
| [User_Peripheral/](User_Peripheral/README.md) | MCU 外设抽象层：ADC、CAN、PWM、SysTick、UART |
| [User_Devices/](User_Devices/README.md) | 设备驱动层：10 个外接硬件模块驱动 |
| [User_Algorithm/](User_Algorithm/README.md) | 纯算法库：标准/增量/高级 PID 控制器 |
| [User_OS/](User_OS/README.md) | 1ms tick 协作式伪 RTOS 调度器 |
| [User_Application/](User_Application/README.md) | 应用层：状态估测、MCM、路线调度 |
| [User_UI/](User_UI/README.md) | 注册表驱动 OLED 12 页 UI 框架 |
| [docs/](docs/) | 设计文档：架构说明、路线表设计、滤波器设计、MCM 状态机方案 |

## 许可

MIT License.
