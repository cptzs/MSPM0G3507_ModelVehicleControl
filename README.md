# MSPM0G3507 Model Vehicle Control

基于 TI MSPM0G3507 的两轮模型车控制工程，面向大学生电赛类智能小车开发。当前 `ai_assist` 分支采用分层架构：外设封装、设备驱动、全局数据、统一车辆状态估测、非阻塞运动控制、表格化路线调度、本地 UI，以及 1ms tick 驱动的协作式调度器。

## 构建与入口

- 使用 **IAR Embedded Workbench** 构建和调试：`basic.eww`、`basic.ewp`。
- MSPM0 外设配置由 SysConfig 维护：`main.syscfg`、`ti_msp_dl_config.c/h`。
- 命令行 `makefile` 构建入口已移除，后续以 IAR 工程文件为准。
- 程序入口为 `main.c`。

## 运行流程

`main.c` 启动后依次执行：`SYSCFG_DL_init()`、`USER_SYSTEM_Init()`、`USER_OS_Init()`、`USER_GlobalData_Init()`、`USER_Device_Init()`、`USER_STATE_Init()`、`USER_RegisterSchedulerTasks()`，随后在主循环中持续调用 `USER_OS_Run()`。

`User_OS` 不是抢占式 RTOS，不做上下文切换，也不分配独立任务栈。所有任务仍运行在 `main` 前台上下文中，任务函数必须短小、非阻塞。

## 当前调度任务

- `global`：1ms，刷新 ADC、电位器、温度、电源电压等全局数据。
- `lidar`：5ms，维护 LiDAR 通信和数据。
- `ui`：5ms，处理 OLED、按键和路线启动交互。
- `state`：10ms，刷新统一车辆状态估计。
- `mcm`：10ms，执行非阻塞运动控制状态机。
- `race`：10ms，处理路线请求和动作表调度。
- `heartbeat`：500ms，LED0 心跳。

任务注册支持周期、offset 错峰和 priority 优先级；priority 数值越小优先级越高。

## 核心模块

- `User_Peripheral/`：ADC、CAN、PWM、UART、系统服务等 MCU 外设封装。
- `User_Devices/`：LBB、OLED/字库、编码器、IMU、LiDAR、电机、舵机、Modbus、OEMT 光电等设备驱动。
- `globals.c/h`：集中保存传感器数据、PID 实例、ADC 数据和 Modbus 数据区。
- `User_OS/user_os.c/h`：1ms 协作式任务调度器。
- `userapp_state_estimator.c/h`：统一状态估测层，是应用层中唯一直接读取编码器和 IMU 原始数据的模块。
- `userapp_vehicle_model.h`：车辆几何、编码器和路程计标定参数。
- `userapp_mcm.c/h`：非阻塞运动控制管理器，当前支持直行/后退和原地旋转。
- `userapp_race_table.c/h`：表格化路线调度器，只解释动作表并调用 MCM，不直接写电机或传感器。
- `userapp_race.c/h`：路线启动请求和动作表推进；当前包含模板路线。
- `user_ui.c/h`：唯一直接操作 OLED、按键、LED、蜂鸣器的应用层模块。
- `User_Algorithm/`：当前主要提供 PID 控制器。

## 当前控制链路

```text
编码器 / IMU / ADC / LiDAR 等设备数据
        ↓
globals + User_GlobalData_Task
        ↓
USER_State_Task 统一状态估测
        ↓
USER_MCM_Task 非阻塞运动控制
        ↓
速度 PID + userlib_motor 电机输出
```

路线启动链路：

```text
user_ui 长按 ENTER 蓄力与倒计时
        ↓
USER_Race_RequestStart
        ↓
USER_Race_Task
        ↓
userapp_race_table 动作表
        ↓
USER_MCM_StartStraight / USER_MCM_StartSpin
```

## 配置项与限制

- `OEMT_SENSOR_MODE`：选择 OEMT 模拟光电或数字光电。两者共享物理接口，不能同时启用。
- `USER_STATE_DEFAULT_ESTIMATOR`：选择状态估测方法。当前只有 RAW 实际可用，其它方法为预留入口并回落到 RAW。
- 当前默认初始化全部外部设备，不再通过宏裁剪设备启动逻辑。
- `USER_MCM_StartArc()` 只是预留接口，当前未实现。
- 当前模板路线为：直行 500mm、旋转 180°、返回 500mm、再旋转 180°。

## 许可

MIT License。