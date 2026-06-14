# User_Devices - 设备驱动层

设备驱动层封装具体传感器、执行器和人机交互硬件。本层可以调用 `User_Peripheral` 和 `User_Algorithm`，但不依赖 `User_Application`。

## 文件列表

| 文件 | 设备 | 接口 | 核心功能 |
|------|------|------|----------|
| `userlib_encoder.c/h` | 编码器 x3 | GPIO + Timer | 转速测量、方向判断、累计里程、溢出处理 |
| `userlib_imu.c/h` | 6 轴 IMU | UART0 | 加速度/角速度/姿态角解析、角度归一化、指令发送 |
| `userlib_lbb.c/h` | BoardIO: LED/按键/蜂鸣器 | GPIO + SysTick | LED 定时闪烁、蜂鸣器、8 按键计数型事件 |
| `userlib_lidar.c/h` | LiDAR x4 | CAN | 轮询通信、距离解析、信号强度 |
| `userlib_modbus.c/h` | Modbus 从机 | UART1 | 寄存器读写、CRC 校验、帧超时 |
| `userlib_motor.c/h` | 电机 x2 | PWM + GPIO | 速度控制、运行/制动/滑行模式 |
| `userlib_oemt.c/h` | OEMT 模拟光电 x8 | ADC + GPIO | 回差比较、自动校准、扫描率统计 |
| `userlib_oled.c/h` | OLED 128x64 | SPI + DMA | 全帧缓冲绘制、文字/图形/进度条 |
| `userlib_fonts.c/h` | 字库 | - | 6x8 和 8x16 ASCII 字模 |
| `userlib_servo.c/h` | 舵机 x4 | Timer + PWM | 角度控制、脉宽换算、系统误差修正 |

## BoardIO (`userlib_lbb.c/h`)

历史文件名保留为 `userlib_lbb.c/h`，但公开函数统一使用 `USER_BoardIO_*` 前缀。`BoardIO` 表示板载 LED、按键和蜂鸣器这一组用户 I/O。

关键接口：

```c
void USER_BoardIO_Init(void);
void USER_BoardIO_LED_On(LED_t led, uint16_t ms);
void USER_BoardIO_LED_Off(LED_t led);
void USER_BoardIO_Buzzer_On(uint16_t ms);
void USER_BoardIO_Buzzer_Off(void);
USER_LBB_ButtonEvent_t USER_BoardIO_Button_ConsumeEvent(Button_t button);
```

按键事件采用计数型模型，短按、长按和长按重复分别维护 pending 计数，上层通过 `USER_BoardIO_Button_ConsumeEvent()` 消费。

## 编码器 (`userlib_encoder.c/h`)

- 3 路编码器：0=路程计，1=左驱动轮，2=右驱动轮。
- 使用定时器边沿计数和 GPIO 方向检测。
- 通过 `USER_Encoder_Task()` 周期更新速度、方向和累计里程。
- 初始化入口：`USER_Encoder_Init(encoder_data)`。

## IMU (`userlib_imu.c/h`)

- UART0 通信，解析加速度、角速度和姿态角。
- 支持解锁、校准、锁定和角度参考设置。
- 关键接口包括 `USER_IMU_Init()`、`USER_IMU_Comm_Routine()`、`USER_IMU_NormalizeYaw()`、`USER_IMU_SetAngleRef()`、`USER_IMU_SetYawRef()`。

## OLED (`userlib_oled.c/h`)

- 使用 `GRAM[8][128]` 全帧缓冲。
- 绘制 API 只修改缓冲区，`USER_OLED_Service()` 负责限帧和 SPI DMA 后台送显。
- 支持文本、整数、浮点格式化、像素、线、矩形、圆弧和进度条。

## LiDAR (`userlib_lidar.c/h`)

- 4 路 LiDAR 通过 CAN 总线轮询。
- 对外任务入口为 `USER_LiDAR_Task()`，初始化入口为 `USER_LiDAR_Init(lidar_data)`。
- 数据存储在 `lidar_data[1..4]`，索引 0 保留。

## OEMT 模拟光电 (`userlib_oemt.c/h`)

- 8 路 ADC 采样 + 回差比较二值化。
- 不再区分 `AN` / `DG` 二级前缀，对外函数统一为 `USER_OEMT_*`。
- 支持自动校准和扫描率统计。

## 电机与舵机

- 电机接口：`USER_Motor_Init()`、`USER_Motor_SetSpeed()`、`USER_Motor_SetMode()`、`USER_Motor_Stop()`。
- 舵机接口：`USER_Servo_Init()`、`USER_Servo_SetAngle()`、`USER_Servo_GetAngle()` 等。

## Modbus

- UART1 Modbus RTU 从机。
- 128 个保持寄存器：`modbus_regs[]`。
- 支持 03/06/10 功能码、CRC16 校验和帧超时检测。
