# User_Peripheral — MCU 外设抽象层

对 MSPM0G3507 片内外设的薄封装，提供应用层无关的硬件接口。
所有函数假定外设已由 SysConfig (`ti_msp_dl_config.c`) 完成底层初始化。

## 文件列表

| 文件 | 外设 | 核心功能 |
|------|------|---------|
| `userlib_adc.c/h` | ADC12 | 多通道 ADC 采样（7 通道）、温度/电压换算 |
| `userlib_can.c/h` | MCAN | CAN 收发、中断回调注册 |
| `userlib_pwm.c/h` | GPTIMER | PWM 占空比/频率设置、启停控制 |
| `userlib_systick.c/h` | SysTick | 系统滴答初始化和启动、回调注册/注销、系统时钟获取 |
| `userlib_uart.c/h` | UART + DMA | 串口 DMA 收发、中断回调注册、超时检测 |

## 设计原则

- 只封装 MCU 内部资源，不做具体设备业务逻辑
- 提供中断回调注册机制 (`RegisterCallback`)，供设备驱动层使用
- 不编写比赛逻辑或状态机
- 不直接依赖 `User_Application`

## 关键接口

### SysTick (`userlib_systick.h`)

```c
void USER_System_Init(void);                                          // 系统时钟和 SysTick 初始化
bool USER_SysTick_RegisterCallback(void (*callback)(void));           // 注册 1ms 回调
bool USER_SysTick_UnregisterCallback(void (*callback)(void));         // 注销回调
```

SysTick 被配置为 1ms 周期定时器。`USER_OS` 通过注册回调 `USER_OS_TickISR` 来驱动
协作式调度器的时间基准。

### UART + DMA (`userlib_uart.h`)

```c
void USER_UART_Init(UART_Instance uart_inst);
void USER_UART_Transmit_DMA(UART_Instance, uint8_t *data, uint16_t len);
void USER_UART_Receive_DMA(UART_Instance, uint8_t *buf, uint16_t len);
uint16_t USER_UART_GetReceivedBytes_DMA(UART_Instance);
void USER_UART_Abort_Receive(UART_Instance);
void USER_UART_RegisterCallback(UART_Instance, UART_Interrupt, void(*handler)(void));
```

- UART0 → IMU 通信（33 字节帧）
- UART1 → Modbus 从机

### ADC (`userlib_adc.h`)

```c
void USER_ADC_Init(uint16_t *adc_output, uint16_t number_of_channels);
float USER_ADC_GetInnerTemperature(uint16_t rawdata);   // ADC原始值→°C
uint16_t USER_ADC_GetPowerVoltage(uint16_t rawdata);    // ADC原始值→mV
uint16_t USER_ADC_ValueToVoltage(uint16_t rawdata);     // 通用ADC→mV
```

7 个 ADC 通道：4 个通用模拟输入 + 1 个电位器 + 温度传感器 + 电源电压。

### PWM (`userlib_pwm.h`)

```c
void USER_PWM_SetFrequency(uint8_t timer, uint32_t frequency_hz);
void USER_PWM_SetDuty(uint8_t timer, uint8_t channel, uint16_t duty);
void USER_PWM_Start(uint8_t timer);
void USER_PWM_Stop(uint8_t timer);
```

### CAN (`userlib_can.h`)

```c
void USER_CAN_Init(void);
void USER_CAN_Transmit(uint32_t id, uint8_t *data, uint8_t dlc);
void USER_CAN_RegisterRxCallback(void (*callback)(uint32_t id, uint8_t *data, uint8_t dlc));
```

## 典型调用方向

```
User_Devices → User_Peripheral → TI_DriverLib
```

例如：
- `userlib_motor.c` → `userlib_pwm.c`
- `userlib_modbus.c` → `userlib_uart.c`
- `userlib_oemt.c` → `userlib_adc.c`
- `userlib_imu.c` → `userlib_uart.c`
