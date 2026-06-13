/**
 * @file userlib_encoder.c
 * @brief 编码器驱动（定时器边沿计数模式）。
 *
 * 使用 GPTIMER 边沿检测计数捕获输入脉冲，SysTick 回调定时读取计数差值
 * 并计算转速。支持 3 路编码器独立配置周期和方向检测。
 *
 * 原理：定时器向下计数，每次溢出(归零)触发中断并重置计数器。
 * SysTick 回调中每隔 period 毫秒读取一次计数值作为脉冲增量。
 */

#include "userlib_encoder.h"

/// @brief 编码器配置结构体（内部使用）
typedef struct
{
    GPTIMER_Regs *timer;             // 定时器号
    uint16_t period;                 // 编码器计数周期
    EncoderData_t_Typedef *data_ptr; // 指向编码器数据的指针
    GPIO_Regs *direction_port;       // 方向端口
    uint32_t direction_pin;          // 方向引脚
    bool reverse_install;            // 是否反向安装编码器
    bool initialized;                // 编码器是否已初始化
} EncoderConfig_t_Typedef;

/// @brief 实例化编码器配置结构体数组，用于存储多个编码器的配置参数
EncoderConfig_t_Typedef encoder_config[ENCODER_COUNT];

/// @brief 编码器模块初次调用标志
bool encoder_first_call = true;

/// @brief 编码器系统滴答计数器
uint32_t systick_encoder[ENCODER_COUNT];

/**
 * @brief 编码器 SysTick 周期处理函数。
 * @details 由 SysTick 回调调用，定时读取编码器计数值并计算转速和累计里程。
 */
void USER_SysTick_Handler_Encoder(void)
{
    uint8_t i = 0;
    bool direction_pin_high = false;
    int16_t direction_temp = 0;
    int16_t speed_temp = 0;

    // 如果编码器模块未初始化，直接返回
    if (encoder_first_call)
    {
        return; // 编码器未初始化，直接返回
    }

    for (i = 0; i < ENCODER_COUNT; i++)
    {
        // 如果当前编码器未初始化，或者计数周期为0，跳过当前编码器
        if (encoder_config[i].initialized == false || encoder_config[i].period == 0)
        {
            continue;
        }

        // 增加编码器系统滴答计数器
        systick_encoder[i]++;
        if (systick_encoder[i] >= encoder_config[i].period)
        {
            // 达到周期，重置计数器
            systick_encoder[i] = 0; // 重置计数器

            // 如果当前编码器归零中断标志位置1，表示计数器已溢出
            if (DL_Timer_getPendingInterrupt(encoder_config[i].timer) == DL_TIMERG_IIDX_ZERO)
            {
                // 清除中断状态
                DL_Timer_clearInterruptStatus(encoder_config[i].timer, DL_TIMERG_IIDX_ZERO);
                // 设置状态为溢出
                encoder_config[i].data_ptr->status = ENCODER_STA_OVERFLOW;
                // 重置计数器
                DL_Timer_setTimerCount(encoder_config[i].timer, TIM_MAX_COUNTER);
            }

            // 如果当前编码器未溢出，正常处理
            else
            {
                // 读取当前计数值
                speed_temp = TIM_MAX_COUNTER - DL_Timer_getTimerCount(encoder_config[i].timer);

                // 重置计数器
                DL_Timer_setTimerCount(encoder_config[i].timer, TIM_MAX_COUNTER);

                // 读取方向引脚状态
                direction_pin_high = (DL_GPIO_readPins(encoder_config[i].direction_port, encoder_config[i].direction_pin) & encoder_config[i].direction_pin) != 0;

                // 确定基础方向 (引脚高电平=正方向, 低电平=负方向)
                direction_temp = direction_pin_high ? 1 : -1;

                // 应用反向安装修正
                if (encoder_config[i].reverse_install)
                {
                    direction_temp = -direction_temp;
                }

                // 更新方向数据
                encoder_config[i].data_ptr->direction = direction_temp;

                // 更新带符号的速度数据
                encoder_config[i].data_ptr->speed = direction_temp * speed_temp;

                // 更新行进距离积分
                encoder_config[i].data_ptr->sum_distance += encoder_config[i].data_ptr->speed;

                // 设置状态
                encoder_config[i].data_ptr->status = (speed_temp == 0) ? ENCODER_STA_NO_PULSE : ENCODER_STA_OK;
            }
        }
    }
}

/// @brief 单个编码器初始化函数
/// @param data_ptr 指向编码器数据的指针
/// @param slot 编码器槽位编号 (0-2)
/// @param timer 定时器指针
/// @param direction_port 方向GPIO端口
/// @param direction_pin 方向GPIO引脚
/// @param reverse_install 是否反向安装
/// @param period 计数周期(ms)
/// @return 初始化结果
bool USER_ENCODER_InitSlot(EncoderData_t_Typedef *data_ptr,
                           uint8_t slot,
                           GPTIMER_Regs *timer,
                           GPIO_Regs *direction_port,
                           uint32_t direction_pin,
                           bool reverse_install,
                           uint16_t period)
{
    uint16_t i;

    // 检查参数有效性
    if (data_ptr == NULL || slot >= ENCODER_COUNT || timer == NULL)
    {
        return false;
    }

    // 如果是第一次调用初始化函数
    if (encoder_first_call)
    {
        // 初始化编码器配置数组
        for (i = 0; i < ENCODER_COUNT; i++)
        {
            encoder_config[i].timer = NULL;
            encoder_config[i].data_ptr = NULL;
            encoder_config[i].initialized = false;
            systick_encoder[i] = 0;
        }
        // 注册SysTick回调函数
        USER_SYSTICK_RegisterCallback(USER_SysTick_Handler_Encoder);
        // 设置为false，表示已经调用过初始化函数
        encoder_first_call = false;
    }

    // 如果该槽位的编码器未初始化
    if (encoder_config[slot].initialized == false)
    {
        // 配置编码器参数
        encoder_config[slot].timer = timer;
        encoder_config[slot].period = period;
        encoder_config[slot].data_ptr = data_ptr;
        encoder_config[slot].direction_port = direction_port;
        encoder_config[slot].direction_pin = direction_pin;
        encoder_config[slot].reverse_install = reverse_install;

        // 初始化编码器数据
        data_ptr->speed = 0;
        data_ptr->direction = 0;
        data_ptr->status = ENCODER_STA_OK;
        data_ptr->sum_distance = 0;

        // 设置定时器的计数周期
        DL_Timer_setTimerCount(timer, TIM_MAX_COUNTER);
        // 启动定时器输入脉冲计数模式
        DL_Timer_startCounter(timer);

        // 标记该编码器已初始化
        encoder_config[slot].initialized = true;

        return true;
    }
    else
    {
        // 如果该槽位的编码器已经初始化，返回错误
        return false;
    }
}

/**
 * @brief 编码器全部初始化函数。
 * @details 批量初始化 3 路编码器，每路配置对应定时器和方向 IO。
 * @param encoder_data_array 编码器数据数组指针 (3 元素)。
 * @retval true 全部初始化成功。
 * @retval false 部分或全部初始化失败。
 */
bool USER_ENCODER_Init(EncoderData_t_Typedef *encoder_data_array)
{
    uint8_t i;
    bool result = true;

    // 检查参数有效性
    if (encoder_data_array == NULL)
    {
        return false;
    }

    // 初始化所有编码器状态为未初始化
    for (i = 0; i < ENCODER_COUNT; i++)
    {
        encoder_data_array[i].status = ENCODER_STA_UNINIT;
        encoder_data_array[i].speed = 0;
        encoder_data_array[i].direction = 0;
        encoder_data_array[i].sum_distance = 0;
    }

    // 初始化第1个编码器（槽位0）
    if (!USER_ENCODER_InitSlot(&encoder_data_array[0], 0, COMPARE_0_INST,
                               ENCODERA_PORT, ENCODERA_DIR0_PIN, true, 10))
    {
        result = false;
    }

    // 初始化第2个编码器（槽位1）
    if (!USER_ENCODER_InitSlot(&encoder_data_array[1], 1, COMPARE_1_INST,
                               ENCODERB_PORT, ENCODERB_DIR1_PIN, false, 10))
    {
        result = false;
    }

    // 初始化第3个编码器（槽位2）
    if (!USER_ENCODER_InitSlot(&encoder_data_array[2], 2, COMPARE_2_INST,
                               ENCODERB_PORT, ENCODERB_DIR2_PIN, true, 10))
    {
        result = false;
    }

    return result;
}