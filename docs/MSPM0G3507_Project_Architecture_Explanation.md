# MSPM0G3507 智能车工程架构说明

> 本文档用于说明当前 MSPM0G3507 智能车工程的目录分层、模块职责和代码归属规则，供 Codex / Agent 自动化重构和学生开发时参考。

---

## 1. 工程架构总览

当前工程采用“官方库 + 用户分层代码”的结构。

```text
basic - Debug
├─ Examples
├─ SysConfig Generated Files
├─ TI_DriverLib
├─ User_Algorithm
├─ User_Application
├─ User_Devices
├─ User_Peripheral
├─ globals.c
├─ main.c
├─ main.syscfg
└─ Output
```

整体分层关系：

```text
User_Application     // 应用层：比赛流程、状态机、显示业务
        ↑
User_Algorithm       // 算法层：PID、滤波、控制算法
        ↑
User_Devices         // 设备层：电机、编码器、IMU、OLED、雷达、上位机调试接口
        ↑
User_Peripheral      // 外设层：ADC、CAN、PWM、UART、系统时钟等 MCU 片上外设
        ↑
TI_DriverLib / SysConfig Generated Files
```

设计目标是：

**学生主要写 Application，教师或负责人维护 Algorithm / Devices / Peripheral。**

---

## 2. 目录职责说明

### 2.1 Examples

```text
Examples/
```

用于存放示例工程、模块测试入口和功能验证代码。

适合放入：

- LED 闪烁测试
- PWM 输出测试
- ADC 采样测试
- UART 通信测试
- 电机驱动测试
- 编码器读取测试
- OLED 显示测试
- 上位机通信测试

不适合放入：

- 正式比赛逻辑
- 长期运行的主业务代码
- 与某一届比赛强绑定的代码

---

### 2.2 SysConfig Generated Files

```text
SysConfig Generated Files/
```

该目录用于存放 TI SysConfig 自动生成的配置文件，通常包含引脚、外设、中断、时钟等配置。

规则：

- 一般不手工大规模修改
- 如需调整，应优先通过 SysConfig 工具修改配置后重新生成
- Agent 和学生原则上不主动修改该目录

---

### 2.3 TI_DriverLib

```text
TI_DriverLib/
```

该目录用于存放 TI 官方 MSPM0 DriverLib 或相关芯片支持库。

规则：

- 不随意修改
- 不在其中添加用户业务逻辑
- 用户自定义逻辑应写入 `User_Peripheral`、`User_Devices`、`User_Algorithm` 或 `User_Application`

---

## 3. 用户代码分层说明

---

## 3.1 User_Peripheral

```text
User_Peripheral/
├─ userlib_adc.c
├─ userlib_can.c
├─ userlib_pwm.c
├─ userlib_sys.c
└─ userlib_uart.c
```

### 定位

`User_Peripheral` 是 **MCU 片上外设封装层**。

它负责把 MSPM0G3507 芯片内部外设封装成更好用的接口，供上层设备驱动调用。

### 判断标准

如果模块直接操作 MCU 内部外设，则应放入 `User_Peripheral`。

### 当前文件说明

| 文件 | 职责 |
|---|---|
| `userlib_adc.c` | ADC 采样接口封装 |
| `userlib_can.c` | CAN 通信接口封装 |
| `userlib_pwm.c` | PWM 输出接口封装 |
| `userlib_sys.c` | 系统初始化、时钟、延时等基础功能 |
| `userlib_uart.c` | UART 串口收发接口封装 |

### 典型调用方向

```text
User_Devices -> User_Peripheral -> TI_DriverLib
```

例如：

```text
userlib_motor.c 调用 userlib_pwm.c
userlib_modbus.c 调用 userlib_uart.c
userlib_oemt_an.c 调用 userlib_adc.c
```

### 开发规则

- 只封装 MCU 内部资源
- 不写比赛逻辑
- 不写具体设备业务
- 不写状态机
- 不直接依赖 User_Application

---

## 3.2 User_Devices

```text
User_Devices/
├─ userlib_encoder.c
├─ userlib_imu.c
├─ userlib_lbb.c
├─ userlib_lidar.c
├─ userlib_modbus.c
├─ userlib_motor.c
├─ userlib_oemt_an.c
├─ userlib_oemt_dg.c
├─ userlib_oled.c
└─ userlib_servo.c
```

### 定位

`User_Devices` 是 **板载设备 / 外接设备 / 调试接口抽象层**。

它负责把具体硬件模块封装成可直接使用的设备接口。

### 判断标准

如果模块对应一个外部设备、板载模块、传感器、执行器或上位机调试接口，则应放入 `User_Devices`。

### 当前文件说明

| 文件 | 职责 |
|---|---|
| `userlib_encoder.c` | 编码器设备接口 |
| `userlib_imu.c` | IMU 姿态传感器接口 |
| `userlib_lbb.c` | LBB 模块接口，具体含义以项目定义为准 |
| `userlib_lidar.c` | 雷达 / 测距模块接口 |
| `userlib_modbus.c` | 上位机调试器通信接口 |
| `userlib_motor.c` | 电机驱动抽象 |
| `userlib_oemt_an.c` | OEMT 模拟量传感器接口，具体含义以项目定义为准 |
| `userlib_oemt_dg.c` | OEMT 数字量传感器接口，具体含义以项目定义为准 |
| `userlib_oled.c` | OLED 显示屏驱动接口 |
| `userlib_servo.c` | 舵机控制接口 |

### 关于 `userlib_modbus.c`

本工程中的 `userlib_modbus.c` 不作为通用 Modbus 协议中间件处理。

它更准确的定位是：

```text
上位机调试器接口
```

即：

```text
PC调试软件 / 上位机
        ↓
Modbus协议通信
        ↓
小车控制板
```

因此它属于设备层接口，放在 `User_Devices` 中是合理的。

### 典型调用方向

```text
User_Application -> User_Devices -> User_Peripheral
```

例如：

```text
userapp_display.c 调用 userlib_oled.c
userapp_race.c 调用 userlib_motor.c / userlib_encoder.c
userapp_mcm.c 调用 userlib_modbus.c
```

### 开发规则

- 可以调用 User_Peripheral
- 可以调用 User_Algorithm
- 不应反向依赖 User_Application
- 不应写比赛状态机
- 不应写某一届比赛的专用策略

---

## 3.3 User_Algorithm

```text
User_Algorithm/
└─ userlib_pid.c
```

### 定位

`User_Algorithm` 是 **通用算法库**。

它存放与具体硬件无关、可跨项目复用的控制算法、数学算法和数据处理算法。

### 判断标准

如果一个模块不依赖具体硬件，只处理数据、控制量或数学逻辑，则应放入 `User_Algorithm`。

### 当前文件说明

| 文件 | 职责 |
|---|---|
| `userlib_pid.c` | PID 控制算法 |

### 后续可加入模块

```text
User_Algorithm/
├─ userlib_pid.c
├─ userlib_filter.c
├─ userlib_ramp.c
├─ userlib_math.c
├─ userlib_fsm.c
├─ userlib_path.c
└─ userlib_control.c
```

### 典型调用方向

```text
User_Application -> User_Algorithm
User_Devices     -> User_Algorithm
```

### 开发规则

- 不直接操作 ADC / PWM / UART 等外设
- 不直接控制电机、OLED、IMU 等设备
- 不写比赛状态机
- 只处理输入数据并输出计算结果
- 尽量保持可移植性

---

## 3.4 User_Application

```text
User_Application/
├─ userapp_display.c
├─ userapp_mcm.c
├─ userapp_race.c
├─ userapp_state_estimator.c
└─ userapp_vehicle_model.h
```

### 定位

`User_Application` 是 **应用层 / 业务逻辑层**。

它负责组织比赛任务、车辆行为、显示页面、状态切换和整体运行流程。

### 判断标准

如果模块描述的是“车要做什么”，而不是“硬件怎么驱动”，则应放入 `User_Application`。

### 当前文件说明

| 文件 | 职责 |
|---|---|
| `userapp_display.c` | 应用层显示页面和显示业务逻辑 |
| `userapp_mcm.c` | 上位机调试、参数交互或模式控制相关应用逻辑 |
| `userapp_race.c` | 比赛流程、赛道任务、运行策略 |
| `userapp_state_estimator.c` | 统一车辆状态估测和传感器信息包装 |
| `userapp_vehicle_model.h` | 车辆几何参数和编码器标定常量 |

### 典型调用方向

```text
User_Application
    ├─ 调用 User_Devices
    ├─ 调用 User_Algorithm
    └─ 间接使用 User_Peripheral
```

应用层不建议直接调用 TI_DriverLib。  
如需使用硬件能力，应通过 `User_Devices` 或 `User_Peripheral` 间接完成。

### 开发规则

- 比赛策略写在这里
- 状态机写在这里
- UI 页面业务写在这里
- 不直接写底层寄存器
- 不直接修改 TI_DriverLib
- 不把通用算法写死在应用层

---

## 4. 文件命名规则

### 4.1 库文件

```text
userlib_xxx.c
userlib_xxx.h
```

适用于：

- User_Peripheral
- User_Devices
- User_Algorithm

示例：

```text
userlib_uart.c
userlib_motor.c
userlib_pid.c
```

### 4.2 应用文件

```text
userapp_xxx.c
userapp_xxx.h
```

适用于：

- User_Application

示例：

```text
userapp_race.c
userapp_state_estimator.c
```

### 4.3 命名含义

| 前缀 | 含义 |
|---|---|
| `userlib_` | 可复用库代码 |
| `userapp_` | 应用层业务代码 |

---

## 5. 依赖方向规则

为保持架构清晰，依赖关系必须遵守单向原则。

### 5.1 推荐依赖方向

```text
User_Application
        ↓
User_Devices
        ↓
User_Peripheral
        ↓
TI_DriverLib / SysConfig
```

`User_Algorithm` 可以被 `User_Application` 和 `User_Devices` 调用，但它不应依赖硬件层。

```text
User_Application ──→ User_Algorithm
User_Devices     ──→ User_Algorithm
```

### 5.2 禁止依赖方向

禁止出现：

```text
User_Peripheral -> User_Devices
User_Peripheral -> User_Application
User_Devices    -> User_Application
User_Algorithm  -> User_Devices
User_Algorithm  -> User_Peripheral
TI_DriverLib    -> User_xxx
```

---

## 6. 模块归属判断方法

当新增一个 `.c/.h` 文件时，按以下规则判断。

| 问题 | 目录 |
|---|---|
| 是否直接操作 ADC / PWM / UART / CAN / SPI / I2C / Timer / GPIO？ | `User_Peripheral` |
| 是否对应电机、编码器、OLED、IMU、雷达、舵机、上位机调试接口？ | `User_Devices` |
| 是否为 PID、滤波、斜坡、数学工具、控制算法？ | `User_Algorithm` |
| 是否描述比赛流程、模式切换、UI 页面、参数菜单、赛道策略？ | `User_Application` |

---

## 7. 面向学生的开发约定

### 7.1 学生主要修改区域

学生主要修改：

```text
User_Application/
```

包括：

- `userapp_race.c`
- `userapp_state_estimator.c`
- `userapp_vehicle_model.h`
- `userapp_display.c`
- `userapp_mcm.c`

### 7.2 谨慎修改区域

学生可以在指导下修改：

```text
User_Algorithm/
```

例如：

- 调整 PID 参数
- 新增滤波算法
- 新增控制算法

### 7.3 原则上不修改区域

学生原则上不直接修改：

```text
User_Peripheral/
User_Devices/
TI_DriverLib/
SysConfig Generated Files/
```

除非明确负责对应模块维护。

---

## 8. 面向 Agent / Codex 的操作规则

### 8.1 总体原则

Agent 在修改工程时，应先判断文件层级，再执行修改。

不要因为文件名类似就随意移动。  
应根据模块职责判断归属。

### 8.2 允许操作

Agent 可以：

- 分析依赖关系
- 整理 include 路径
- 生成说明文档
- 补充模块注释
- 新增 Examples 测试代码
- 在 User_Application 中新增业务逻辑
- 在 User_Algorithm 中新增纯算法
- 在 User_Devices 中新增设备驱动封装
- 在 User_Peripheral 中新增片上外设封装

### 8.3 谨慎操作

Agent 修改以下内容前必须说明理由：

```text
User_Peripheral/
User_Devices/
main.c
globals.c
main.syscfg
```

原因：

- 这些文件可能影响整车运行
- 修改后需要硬件验证
- 错误修改可能导致电机误动作或通信异常

### 8.4 禁止操作

Agent 禁止：

- 删除已有文件
- 大范围重命名
- 修改 TI_DriverLib
- 修改 SysConfig 生成代码
- 将应用层逻辑写入 User_Peripheral
- 将底层驱动逻辑写入 User_Application
- 让 User_Algorithm 依赖具体硬件
- 未经确认修改电机控制逻辑

---

## 9. 推荐代码调用关系示例

### 9.1 电机速度控制

```text
userapp_race.c
    ↓
userlib_motor.c
    ↓
userlib_pwm.c
    ↓
TI_DriverLib
```

如果使用速度闭环：

```text
userapp_race.c
    ↓
userlib_motor.c
    ↓
userlib_pid.c
```

### 9.2 上位机调试通信

```text
PC调试软件
    ↓
Modbus通信
    ↓
userlib_modbus.c
    ↓
userlib_uart.c
    ↓
TI_DriverLib
```

### 9.3 OLED 显示业务

```text
userapp_display.c
    ↓
userlib_oled.c
    ↓
User_Peripheral / TI_DriverLib
```

### 9.4 编码器速度采集

```text
userapp_race.c
    ↓
userlib_encoder.c
    ↓
User_Peripheral / TI_DriverLib
```

---

## 10. 后续扩展建议

### 10.1 可新增的算法模块

```text
User_Algorithm/
├─ userlib_filter.c
├─ userlib_ramp.c
├─ userlib_motion_control.c
├─ userlib_path_tracking.c
└─ userlib_math.c
```

### 10.2 可新增的设备模块

```text
User_Devices/
├─ userlib_camera.c
├─ userlib_tof.c
├─ userlib_buzzer.c
├─ userlib_bluetooth.c
└─ userlib_debughost.c
```

### 10.3 可新增的应用模块

```text
User_Application/
├─ userapp_param.c
├─ userapp_menu.c
├─ userapp_debug.c
├─ userapp_test.c
└─ userapp_schedule.c
```

---

## 11. 架构总结

本工程当前采用四层用户代码结构：

| 层级 | 目录 | 核心职责 |
|---|---|---|
| 应用层 | `User_Application` | 比赛流程、状态机、显示业务 |
| 算法层 | `User_Algorithm` | PID、滤波、控制算法 |
| 设备层 | `User_Devices` | 电机、传感器、显示屏、调试接口 |
| 外设层 | `User_Peripheral` | ADC、PWM、UART、CAN、系统基础功能 |

一句话理解：

```text
User_Peripheral 解决“芯片怎么用”
User_Devices    解决“硬件模块怎么驱动”
User_Algorithm  解决“数据和控制量怎么算”
User_Application 解决“小车具体要做什么”
```

该结构适合后续比赛迭代，也便于学生分工和 Agent 自动化辅助开发。
