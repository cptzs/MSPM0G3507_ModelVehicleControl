#include "userlib_servo.h"

/*
舵机控制原理：
舵机通过脉宽信号控制角度，脉宽信号的高电平时间决定舵机的角度，高电平时间通常在0.5ms到2.5ms之间变化。
使用定时器A1生成基准时间信号，配置不同的CC通道来控制不同的舵机。
定时器从0向上计数到指定的周期数后重置，重置时触发归零中断，在这个中断内拉高全部GPIO。
定时器向上计数期间，计数器值匹配CC通道的比较值会触发对应的中断，在中断函数中拉低对应的GPIO引脚。
这样就形成了一个完整的PWM信号周期。
每个舵机的角度通过设置对应CC通道的比较值来控制。
例如，CC0通道控制舵机0的角度，CC1通道控制舵机1的角度，以此类推。
*/

/// @brief CC中断掩码数组
const uint32_t CCP_INTERRUPT_MASK[4] = {
    DL_TIMER_INTERRUPT_CC0_UP_EVENT,
    DL_TIMER_INTERRUPT_CC1_UP_EVENT,
    DL_TIMER_INTERRUPT_CC2_UP_EVENT,
    DL_TIMER_INTERRUPT_CC3_UP_EVENT};

bool servo_initialized[4] = {false};          // 舵机是否已初始化
bool servo_enable[4] = {false};               // 舵机是否已启用
int16_t servo_angle[4] = {0};                 // 舵机当前角度
uint32_t servo_pulse_width_us[4] = {0};       // 舵机脉冲宽度数组，单位：us
uint32_t servo_pulse_width_clk[4] = {0};      // 舵机脉冲宽度数组，单位：脉冲数
float servo_angle_range[4] = {0};             // 舵机角度范围（max - min）
float servo_pulse_range[4] = {0};             // 舵机脉冲宽度范围（max_pulse - min_pulse）
float servo_system_error_correction = 0.015f; // 舵机系统误差修正倍率

// 定时器参数缓存
uint32_t timer_fullperiod_us = 0;                 // 定时器周期，单位：us
uint32_t timer_fullperiod_clk = 0;                // 定时器周期，单位：脉冲数
uint32_t timer_prescaler = 0;                     // 定时器预分频器
uint32_t timer_clock_source_freq = 0;             // 定时器时钟源频率，单位：Hz
uint32_t timer_clock_freq = 0;                    // 定时器时钟频率，单位：Hz
uint32_t timer_clock_divider = 0;                 // 定时器时钟分频器
float timer_us_to_pulse = 0.0f;                   // 定时器每us对应的脉冲数
DL_Timer_ClockConfig timer_clock_config = {0};    // 定时器时钟配置结构体
ServoConfig_Struct_TypeDef servo_config_local[4]; // 本地舵机配置缓存

/// @brief 禁用指定索引的舵机
/// @param servo_index 舵机索引，范围为0-3
/// @return bool 禁用成功返回true，失败返回false
bool USER_SERVO_Disable(Servo_Instance servo_index)
{
    if (servo_index >= 4)
    {
        return false; // 无效索引，直接返回
    }
    // 如果该舵机未初始化，直接返回
    if (!servo_initialized[servo_index])
    {
        return false; // 舵机未初始化，直接返回
    }

    // 禁用对应的舵机
    servo_enable[servo_index] = false;
    // 清除对应的CC通道比较值
    DL_Timer_setCaptureCompareValue(TIMA1, 0, servo_index);
    // 禁用对应通道的CC中断
    DL_Timer_disableInterrupt(TIMA1, CCP_INTERRUPT_MASK[servo_index]);
    return true;
}

/// @brief 启用指定索引的舵机
/// @param servo_index 舵机索引，范围为0-3
/// @return bool 启用成功返回true，失败返回false
bool USER_SERVO_Enable(Servo_Instance servo_index)
{
    if (servo_index >= 4)
    {
        return false; // 无效索引，直接返回
    }
    // 如果该舵机未初始化，直接返回
    if (!servo_initialized[servo_index])
    {
        return false; // 舵机未初始化，直接返回
    }
    // 启用对应的舵机
    servo_enable[servo_index] = true;
    // 设置对应的CC通道的比较值
    DL_Timer_setCaptureCompareValue(TIMA1, servo_pulse_width_clk[servo_index], servo_index);
    // 启用对应通道的CC中断
    DL_Timer_enableInterrupt(TIMA1, CCP_INTERRUPT_MASK[servo_index]);

    return true;
}

/// @brief 计算舵机脉冲宽度
/// @param servo_index 舵机索引，范围为0-3
/// @param relative_angle 舵机相对角度，范围为[min_angle, max_angle]，左转为负，右转为正
/// @return 计算得到的脉冲宽度，单位为us
uint16_t USER_SERVO_CalculatePulseWidth(Servo_Instance servo_index, int16_t relative_angle)
{
    float angle_offset; /* 角度偏移量 */
    float pulse_width;  /* 计算得到的脉冲宽度 */
    float angle_ratio;  /* 角度比例 */

    /* 确保索引有效 */
    if (servo_index >= 4)
    {
        return 0; /* 无效索引，返回0 */
    }

    /* 确保角度在有效范围内 */
    if (relative_angle < servo_config_local[servo_index].min_angle)
    {
        relative_angle = servo_config_local[servo_index].min_angle;
    }
    else if (relative_angle > servo_config_local[servo_index].max_angle)
    {
        relative_angle = servo_config_local[servo_index].max_angle;
    }

    /* 计算脉冲宽度*/
    /* 步骤1：计算角度偏移量 */
    angle_offset = (float)(relative_angle - servo_config_local[servo_index].min_angle);

    /* 步骤2：计算角度比例（0.0 到 1.0） */
    angle_ratio = angle_offset / servo_angle_range[servo_index];

    /* 步骤3：线性插值计算脉冲宽度 */
    pulse_width = servo_config_local[servo_index].min_angle_pulsewidth +
                  (angle_ratio * servo_pulse_range[servo_index]);

    /* 返回脉冲宽度，单位为us */
    return (uint16_t)pulse_width;
}

/// @brief 设置舵机角度
/// @param servo_index 舵机索引，范围为0-3
/// @param angle 舵机角度，范围为[min_angle, max_angle]
/// @return bool 设置成功返回true，失败返回false
bool USER_SERVO_SetAngle(Servo_Instance servo_index, int16_t angle)
{
    if (servo_index >= 4)
    {
        return false; // 无效索引，直接返回
    }
    // 计算脉冲宽度
    servo_pulse_width_us[servo_index] = USER_SERVO_CalculatePulseWidth(servo_index, angle);
    // 增加系统误差修正
    servo_pulse_width_us[servo_index] += (uint32_t)(servo_pulse_width_us[servo_index] * servo_system_error_correction);
    // 计算对应的计数值
    servo_pulse_width_clk[servo_index] = (uint32_t)(servo_pulse_width_us[servo_index] * timer_us_to_pulse);
    // 设置对应的CC通道的比较值
    DL_Timer_setCaptureCompareValue(TIMA1, servo_pulse_width_clk[servo_index], servo_index);
    // 更新当前角度
    servo_angle[servo_index] = angle;
    return true;
}

/// @brief 获取指定索引的舵机当前相对角度
int16_t USER_SERVO_GetAngle(Servo_Instance servo_index)
{
    if (servo_index >= 4)
    {
        return 0; // 无效索引，返回0
    }
    // 如果该舵机未初始化，直接返回
    if (!servo_initialized[servo_index])
    {
        return 0; // 舵机未初始化，返回0
    }
    return servo_angle[servo_index]; // 返回当前角度
}

/// @brief 获取指定索引的舵机当前脉冲宽度
/// @param servo_index 舵机索引
/// @return 当前脉冲宽度，单位：us
uint16_t USER_SERVO_GetWidth(Servo_Instance servo_index)
{
    return servo_pulse_width_us[servo_index]; // 返回当前脉冲宽度，单位：us
}

/// @brief 获取舵机误差修正系数
/// @return 当前误差修正系数
float USER_SERVO_GetFError()
{
    // 返回当前舵机的误差修正系数
    return servo_system_error_correction;
}

/// @brief 配置舵机用定时器
void USER_SERVO_TimerConfig()
{
    // 获取TIMA1的计数周期
    timer_fullperiod_clk = DL_Timer_getLoadValue(TIMA1) + 1;

    // 获取TIMA1的时钟源信息
    DL_Timer_getClockConfig(TIMA1, &timer_clock_config);
    // 提取定时器的时钟源频率
    switch (timer_clock_config.clockSel)
    {
    case DL_TIMER_CLOCK_BUSCLK:
        timer_clock_source_freq = CPUCLK_FREQ;
        break;
    default:
        timer_clock_source_freq = 0; // 禁用时钟源，频率为0
        break;
    }
    // 提取定时器的时钟分频器
    timer_clock_divider = timer_clock_config.divideRatio + 1;
    // 提取定时器的预分频器
    timer_prescaler = timer_clock_config.prescale + 1;
    // 计算定时器的时钟频率
    timer_clock_freq = timer_clock_source_freq / (timer_clock_divider * timer_prescaler);
    // 计算定时器的周期，单位为us
    timer_fullperiod_us = (timer_fullperiod_clk * 1000000) / timer_clock_freq;
    // 计算每个us对应的脉冲数
    timer_us_to_pulse = (float)timer_clock_freq / 1000000.f;

    // 启用定时器A1归零中断
    DL_Timer_enableInterrupt(TIMA1, DL_TIMER_INTERRUPT_ZERO_EVENT);

    // 启用定时器A1总中断
    NVIC_EnableIRQ(TIMA1_INT_IRQn);
}

/// @brief 启动舵机控制
void USER_SERVO_Start()
{
    // 启动定时器
    DL_Timer_startCounter(TIMA1);
}

/// @brief 停止舵机控制
void USER_SERVO_Stop()
{
    uint8_t i;
    // 停止定时器
    DL_Timer_stopCounter(TIMA1);

    // 拉低所有存在的舵机引脚
    for (i = 0; i < 4; i++)
    {
        if (servo_initialized[i])
        {
            DL_GPIO_clearPins(servo_config_local[i].port, servo_config_local[i].pin);
        }
    }
}

/// @brief 初始化舵机
/// @param config 舵机配置结构体指针，包括4个舵机的配置
/// @return 初始化成功返回true，失败返回false
bool USER_SERVO_Config(ServoConfig_Struct_TypeDef *config)
{
    if (config == NULL) // 检查配置指针是否为NULL
    {
        return false; // 配置指针为空，初始化失败
    }

    if (servo_initialized[config->instance]) // 如果该舵机已初始化
    {
        return false; // 已初始化，直接返回
    }

    /* 复制配置到本地缓存 */
    servo_config_local[config->instance].center_angle = config->center_angle;
    servo_config_local[config->instance].max_angle = config->max_angle;
    servo_config_local[config->instance].min_angle = config->min_angle;
    servo_config_local[config->instance].max_angle_pulsewidth = config->max_angle_pulsewidth;
    servo_config_local[config->instance].min_angle_pulsewidth = config->min_angle_pulsewidth;
    servo_config_local[config->instance].port = config->port;
    servo_config_local[config->instance].pin = config->pin;
    // 计算角度范围和脉冲宽度范围
    servo_angle_range[config->instance] = (float)(config->max_angle - config->min_angle);
    servo_pulse_range[config->instance] = (float)(config->max_angle_pulsewidth - config->min_angle_pulsewidth);

    // 初始化舵机控制端口和引脚
    /* 初始状态为低电平 */
    DL_GPIO_clearPins(servo_config_local[config->instance].port, servo_config_local[config->instance].pin);
    // 启用对应通道的CC中断
    DL_Timer_enableInterrupt(TIMA1, CCP_INTERRUPT_MASK[config->instance]);
    // 设置舵机初始角度
    USER_SERVO_SetAngle(config->instance, servo_config_local[config->instance].center_angle);
    // 标记为已初始化
    servo_initialized[config->instance] = true;
    // 启用舵机
    servo_enable[config->instance] = true;

    return true; // 初始化成功
}

/// @brief 初始化全部舵机
/// @return bool 初始化成功返回true，失败返回false
bool USER_SERVO_Init()
{
    ServoConfig_Struct_TypeDef servo_config[4];

    // 初始化舵机配置
    servo_config[SERVO_0].instance = SERVO_0;
    servo_config[SERVO_0].center_angle = 0;
    servo_config[SERVO_0].max_angle = 180;
    servo_config[SERVO_0].min_angle = -180;
    servo_config[SERVO_0].max_angle_pulsewidth = 2500;
    servo_config[SERVO_0].min_angle_pulsewidth = 500;
    servo_config[SERVO_0].port = SERVO_PORT;
    servo_config[SERVO_0].pin = SERVO_SERVO1_PIN;

    servo_config[SERVO_1].instance = SERVO_1;
    servo_config[SERVO_1].center_angle = 0;
    servo_config[SERVO_1].max_angle = 180;
    servo_config[SERVO_1].min_angle = -180;
    servo_config[SERVO_1].max_angle_pulsewidth = 2500;
    servo_config[SERVO_1].min_angle_pulsewidth = 500;
    servo_config[SERVO_1].port = SERVO_PORT;
    servo_config[SERVO_1].pin = SERVO_SERVO2_PIN;

    // 配置舵机定时器
    USER_SERVO_TimerConfig();

    // 初始化每个舵机
    if (!USER_SERVO_Config(&servo_config[SERVO_0]))
    {
        return false; // 初始化失败
    }
    if (!USER_SERVO_Config(&servo_config[SERVO_1]))
    {
        return false; // 初始化失败
    }

    // 启动舵机控制
    USER_SERVO_Start();
    return true;
}

/// @brief 定时器A1中断处理函数
void TIMA1_IRQHandler(void)
{
    uint8_t i;
    uint8_t servo_index;

    /* 读取当前定时器中断源 */
    DL_TIMER_IIDX interrupt_source = DL_Timer_getPendingInterrupt(TIMA1);

    switch (interrupt_source)
    {
    case DL_TIMER_IIDX_ZERO:
        /* 归零中断处理：PWM周期开始，拉高所有舵机引脚 */
        DL_Timer_clearInterruptStatus(TIMA1, DL_TIMER_INTERRUPT_ZERO_EVENT);
        for (i = 0; i < 4; i++)
        {
            if (servo_initialized[i] && servo_enable[i])
            {
                DL_GPIO_setPins(servo_config_local[i].port, servo_config_local[i].pin);
            }
        }
        break;

    case DL_TIMER_IIDX_CC0_UP:
        /* CC0通道中断：舵机0脉冲宽度到达，拉低引脚 */
        servo_index = 0;
        DL_Timer_clearInterruptStatus(TIMA1, CCP_INTERRUPT_MASK[servo_index]);
        if (servo_initialized[servo_index] && servo_enable[servo_index])
        {
            DL_GPIO_clearPins(servo_config_local[servo_index].port, servo_config_local[servo_index].pin);
        }
        break;

    case DL_TIMER_IIDX_CC1_UP:
        /* CC1通道中断：舵机1脉冲宽度到达，拉低引脚 */
        servo_index = 1;
        DL_Timer_clearInterruptStatus(TIMA1, CCP_INTERRUPT_MASK[servo_index]);
        if (servo_initialized[servo_index] && servo_enable[servo_index])
        {
            DL_GPIO_clearPins(servo_config_local[servo_index].port, servo_config_local[servo_index].pin);
        }
        break;

    case DL_TIMER_IIDX_CC2_UP:
        /* CC2通道中断：舵机2脉冲宽度到达，拉低引脚 */
        servo_index = 2;
        DL_Timer_clearInterruptStatus(TIMA1, CCP_INTERRUPT_MASK[servo_index]);
        if (servo_initialized[servo_index] && servo_enable[servo_index])
        {
            DL_GPIO_clearPins(servo_config_local[servo_index].port, servo_config_local[servo_index].pin);
        }
        break;

    case DL_TIMER_IIDX_CC3_UP:
        /* CC3通道中断：舵机3脉冲宽度到达，拉低引脚 */
        servo_index = 3;
        DL_Timer_clearInterruptStatus(TIMA1, CCP_INTERRUPT_MASK[servo_index]);
        if (servo_initialized[servo_index] && servo_enable[servo_index])
        {
            DL_GPIO_clearPins(servo_config_local[servo_index].port, servo_config_local[servo_index].pin);
        }
        break;

    default:
        break;
    }
}
