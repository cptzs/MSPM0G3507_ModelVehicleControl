# User_Devices — 设备驱动层

对具体传感器、执行器和人机交互硬件的完整驱动封装。
本层可调用 `User_Peripheral` 和 `User_Algorithm`，但不依赖 `User_Application`。

## 文件列表

| 文件 | 设备 | 接口 | 核心功能 |
|------|------|------|---------|
| `userlib_encoder.c/h` | 编码器(×3) | GPIO+Timer | 转速测量、方向判断、累计里程、溢出处理 |
| `userlib_imu.c/h` | 6轴IMU | UART0 | 加速度/角速度/姿态角解析、角度归一化、指令发送 |
| `userlib_lbb.c/h` | LED/按钮/蜂鸣器 | GPIO | LED闪烁、蜂鸣器、8按钮状态/事件计数 |
| `userlib_lidar.c/h` | LiDAR(×4) | CAN | 轮询通信、距离解析、信号强度 |
| `userlib_modbus.c/h` | Modbus从机 | UART1 | 寄存器读写、CRC校验、帧超时 |
| `userlib_motor.c/h` | 电机(×2) | PWM+GPIO | 速度控制、模式切换（制动/运行/滑行） |
| `userlib_oemt.c/h` | 模拟光电(×8) | ADC+GPIO | 回差比较、自动校准、扫描率统计 |
| `userlib_oled.c/h` | OLED 128×64 | SPI+DMA | 全帧缓冲绘制、图形/文字/进度条 |
| `userlib_fonts.c/h` | 字库 | — | 6×8 和 8×16 ASCII 字模数据 |
| `userlib_servo.c/h` | 舵机(×4) | Timer+PWM | 角度控制、脉冲宽度换算、系统误差修正 |

## 关键驱动详解

### 编码器 (`userlib_encoder.c`)

- 3 路编码器（索引 0=路程计, 1=左驱动轮, 2=右驱动轮）
- 使用定时器边沿计数模式 + GPIO 方向检测
- SysTick 回调中定时（默认每 10ms）读取计数差值
- 结构体：
  ```c
  typedef struct {
      Encoder_Status_Typedef status;  // UNINIT/NORMAL/OVERFLOW
      int32_t speed;                  // 速度（计数/周期）
      int32_t sum_distance;           // 累计计数
      uint8_t direction;              // 0=正转, 1=反转
  } EncoderData_t_Typedef;
  ```

### IMU (`userlib_imu.c`)

- 采用 UART 通信协议，33 字节数据帧
- 数据包含：加速度(ax/ay/az)、角速度(gx/gy/gz)、角度(Roll/Pitch/Yaw)
- `imu_data.sum_gyroz` — 局部积分陀螺仪角度（短时低延迟）
- `imu_data.yaw` — 绝对展开航向角
- 通信状态机：请求→等待→接收→解析
- 支持解锁/校准/锁定指令序列
- 关键函数：
  - `USER_IMU_Comm_Routine()` — 1ms 通信状态机
  - `USER_IMU_NormalizeYaw()` — 角度归一化 ±180°
  - `USER_IMU_SetAngleRef()` / `USER_IMU_SetYawRef()` — 多步指令序列

### 按钮 (`userlib_lbb.c`)

- **计数型事件模型**：短按/长按/长按重复各维护独立 pending 计数器
- 上层通过 `USER_LBB_Button_ConsumeEvent()` 消费事件
- 支持 8 个独立按钮：UP/DOWN/LEFT/RIGHT/ENTER/ESC/PREV/NEXT
- 事件类型：`NONE` / `SHORT` / `LONG` / `LONG_REPEAT`

### OLED (`userlib_oled.c`)

- 全帧缓冲架构：`GRAM[8][128]` 存储完整画面
- SPI DMA 后台持续刷新，不阻塞 CPU
- 所有绘制 API 只修改 GRAM 缓冲区
- 提供：文字输出（6×8/8×16字体）、整型/浮点格式化输出、像素/线/矩形/圆弧/进度条绘制

### 电机 (`userlib_motor.c`)

- 三模式控制：
  - `MOTOR_MODE_NORMAL_RUN` — 正常运行（方向 + 速度 PWM）
  - `MOTOR_MODE_REGEN_BRAKE` — 能量回收制动
  - `MOTOR_MODE_COAST` — 空挡滑行
- 两路电机：`MOTOR_0_LEFT` / `MOTOR_1_RIGHT`
- 接口：`USER_Motor_SetSpeed()` / `USER_Motor_SetMode()` / `USER_Motor_Stop()`

### LiDAR (`userlib_lidar.c`)

- 4 路 LiDAR 传感器，通过 CAN 总线轮询
- 结构体：`id`、`distance`（单位 mm）、`strength`（信号强度）
- 数据存储在 `lidar_data[1..4]`（索引 0 保留）

### Modbus (`userlib_modbus.c`)

- 上位机调试器接口，通过 UART1 通信
- 128 个保持寄存器 (`modbus_regs[]`)
- 标准 Modbus RTU 协议：03/06/10 功能码、CRC16 校验、帧超时检测

### OEMT 模拟光电 (`userlib_oemt.c`)

- 8 路 ADC 采样 + 回差比较二值化
- 自动校准、扫描率统计
