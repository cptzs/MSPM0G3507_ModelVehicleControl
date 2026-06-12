# MSPM0G3507 Model Vehicle Control (Basic)

简要说明：这是基于 MSPM0G3507 的模型小车控制基础工程，包含驱动、设备抽象、应用层状态机与算法实现。

构建/调试提示：
- 使用 IAR Embedded Workbench（项目文件在仓库根目录）。
- 可使用 `make`/`makefile` 在支持的环境下构建。

仓库结构（部分）：
- `User_Application/` 应用层
- `User_Algorithm/` 算法库（PID 等）
- `User_Devices/` 设备驱动
- `User_Peripheral/` 外设封装

许可：MIT。作者：cptzs

## 用户模块详解与架构

项目分层（自底向上）：
- **硬件层**：MCU、传感器与外设硬件。
- **外设封装 (`User_Peripheral/`)**：对 MCU 外设（ADC、PWM、UART、CAN 等）的统一封装，提供一致的低级接口。
- **设备驱动 (`User_Devices/`)**：传感器与设备驱动（编码器、IMU、LiDAR、OLED、马达、舵机、Modbus 等），负责数据采集、预处理与设备控制。
- **算法库 (`User_Algorithm/`)**：可复用的控制算法（例如 PID）、滤波器等。
- **应用层 (`User_Application/`)**：系统状态机、比赛策略、运动控制协调与任务调度。
- **入口与集成 (`main.c`)**：系统初始化、模块注册与主循环。

主要文件/模块说明（按目录）：

### User_Algorithm/
- **userlib_pid.c / userlib_pid.h**：PID 控制器实现与封装，供速度、角度或位置回路使用。

### User_Application/
- **user_ui.c / user_ui.h**：人机交互层，包含按键扫描、菜单、OLED 显示与操作提示。
- **userapp_mcm.c / userapp_mcm.h**：运动控制管理（Motion Control Manager），负责接收高层命令并协调下层控制器与执行器。
- **userapp_race.c / userapp_race.h**：比赛逻辑与策略（起跑、巡线/避障、弯道处理等行为决策）。
- **userapp_race_table.c / userapp_race_table.h**：赛道/动作表格数据管理（预设路径点、动作序列、赛段参数等）。
- **userapp_state.c / userapp_state.h**：系统运行状态机与状态转换（运行、暂停、错误处理与恢复）。
- **userapp_state_estimator.c / userapp_state_estimator.h**：传感器融合与状态估计（里程、速度、姿态的滤波与估计）。
- **userapp_vehicle_model.h**：车辆运动学/动力学参数与模型接口（供控制与轨迹计算使用）。

### User_Devices/
- **userlib_encoder.c / userlib_encoder.h**：编码器接口与计数处理，用于里程与速度计算。
- **userlib_imu.c / userlib_imu.h**：IMU 读取、滤波与基础姿态推算接口。
- **userlib_lbb.c / userlib_lbb.h**：项目特定的 LBB 外设驱动（见源文件注释以了解具体用途和接口）。
- **userlib_lidar.c / userlib_lidar.h**：LiDAR 数据接收与距离/点信息解析封装。
- **userlib_modbus.c / userlib_modbus.h**：Modbus 协议封装（寄存器读写、协议帧收发）。
- **userlib_motor.c / userlib_motor.h**：电机控制抽象（PWM、方向，和必要的保护/限幅逻辑）。
- **userlib_oemt_an.c / userlib_oemt_an.h**：OEM 模块模拟量接口（AN），用于采集/输出模拟信号。
- **userlib_oemt_dg.c / userlib_oemt_dg.h**：OEM 模块数字量接口（DG），用于数字 IO 控制。
- **userlib_oled.c / userlib_oled.h**：OLED 显示驱动与便捷显示函数。
- **userlib_servo.c / userlib_servo.h**：舵机控制（占空比到角度映射与控制封装）。

### User_Peripheral/
- **userlib_adc.c / userlib_adc.h**：ADC 初始化与通道采样封装。
- **userlib_can.c / userlib_can.h**：CAN 总线消息的发送/接收封装与处理。
- **userlib_pwm.c / userlib_pwm.h**：通用 PWM 生成接口，用于电机/舵机等设备的占空比控制。
- **userlib_sys.c / userlib_sys.h**：系统级初始化、延时、时钟与错误处理等平台服务。
- **userlib_uart.c / userlib_uart.h**：UART 串口的收发封装与缓冲处理。

快速定位与阅读建议：
- 从 `main.c` 查看系统初始化、模块绑定与主循环逻辑。
- 阅读 `userapp_state.c` 了解系统状态机与主要运行流程。
- 阅读 `userapp_mcm.c` 与 `userlib_pid.c` 理解控制信号的生成与闭环调节链路。
- 新设备接入流程：先在 `User_Peripheral` 实现外设封装，再在 `User_Devices` 实现设备驱动，最后在 `User_Application` 中集成并测试。

备注：每个源文件顶部通常包含实现说明、依赖与使用示例，阅读文件头部注释可快速理解具体实现细节。
