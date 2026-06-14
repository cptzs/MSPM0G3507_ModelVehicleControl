/**
 * @file userlib_encoder.c
 * @brief 编码器驱动（GPTIMER 脉冲计数 + GPIO 方向边沿分段计数）。
 *
 * 编码器脉冲由 GPTIMER 作为外部计数器累计，方向信号由 GPIO 双边沿中断捕获。
 * 中断侧只维护本测量周期的正向计数、负向计数和 10ms 影子样本；
 * 前台 USER_Encoder_Task() 再把影子样本转换为对外公开的速度、方向和累计距离。
 */

#include "userlib_encoder.h"

/**
 * @brief 单路编码器测量缓存。
 *
 * active 用于中断侧正在写入的当前周期数据，shadow 用于前台任务读取。
 */
typedef struct
{
    int8_t current_direction; /* 当前逻辑方向，取值为 1 或 -1，已包含反向安装修正 */
    uint32_t forward_count;   /* 本测量周期累计的正向脉冲数 */
    uint32_t reverse_count;   /* 本测量周期累计的负向脉冲数 */
    int32_t total_count;      /* 本测量周期有符号总脉冲数：forward_count - reverse_count */
} EncoderMeasure_t;

/**
 * @brief 单路编码器内部配置和运行状态。
 */
typedef struct
{
    GPTIMER_Regs *timer;             /* 用作外部脉冲计数器的 GPTIMER 实例 */
    uint16_t period;                 /* 发布周期，单位 ms，当前默认 10ms */
    EncoderData_t_Typedef *data_ptr; /* 指向对外公开的编码器数据 */
    GPIO_Regs *direction_port;       /* 方向 GPIO 端口 */
    uint32_t direction_pin;          /* 方向 GPIO 引脚 */
    bool reverse_install;            /* 编码器是否反向安装 */
    bool initialized;                /* 当前槽位是否已初始化 */

    volatile EncoderMeasure_t active; /* ISR/SysTick 写入的当前周期缓存 */
    volatile EncoderMeasure_t shadow; /* SysTick 发布、前台任务读取的影子缓存 */
    volatile bool shadow_ready;       /* 影子缓存已有新样本 */
    volatile bool overflow_latched;   /* 当前周期内计数器曾发生溢出 */
    volatile bool shadow_overflow;    /* 影子样本对应周期内是否发生溢出 */
} EncoderConfig_t_Typedef;

/// @brief 三路编码器内部配置数组。
EncoderConfig_t_Typedef encoder_config[ENCODER_COUNT];

/// @brief 编码器模块首次初始化标志。
bool encoder_first_call = true;

/// @brief 每路编码器的 SysTick 分频计数器，用 1ms tick 累加到发布周期。
uint32_t systick_encoder[ENCODER_COUNT];

/* 快速读取逻辑方向：GPIO 高电平为正、低电平为负，再叠加 reverse_install 修正。 */
#define USER_ENCODER_READ_DIRECTION_FAST(config_)                                                \
    ((int8_t)((((DL_GPIO_readPins((config_)->direction_port, (config_)->direction_pin) &         \
                 (config_)->direction_pin) != 0u)                                                \
                    ? 1                                                                          \
                    : -1) *                                                                      \
               ((config_)->reverse_install ? -1 : 1)))

/* GPTIMER 向下计数，因此已计入脉冲数 = 初始最大值 - 当前计数值。 */
#define USER_ENCODER_READ_PULSE_COUNT_FAST(timer_) ((uint16_t)(TIM_MAX_COUNTER - DL_Timer_getTimerCount(timer_)))

/* 每次分段结算后把外部计数器重新装载到最大值。 */
#define USER_ENCODER_RESET_COUNTER_FAST(timer_)     DL_Timer_setTimerCount((timer_), TIM_MAX_COUNTER)

/**
 * @brief 清空一个测量缓存，并设置其当前方向。
 * @param measure 指向需要清空的测量缓存。
 * @param direction 清空后保留的当前方向。
 * @retval 无。
 */
static void USER_Encoder_ClearMeasure(volatile EncoderMeasure_t *measure, int8_t direction)
{
    /* 保留方向作为下一段脉冲归类依据。 */
    measure->current_direction = direction;

    /* 清除本周期正向、负向和总和计数。 */
    measure->forward_count = 0u;
    measure->reverse_count = 0u;
    measure->total_count = 0;
}

/**
 * @brief 按缓存中的当前方向累加一段脉冲数。
 * @param measure 指向当前周期 active 缓存。
 * @param pulse_count 本次从 GPTIMER 读取到的脉冲数。
 * @retval 无。
 */
static void USER_Encoder_AddPulseCount(volatile EncoderMeasure_t *measure, uint16_t pulse_count)
{
    /* 当前方向为正，则本段脉冲归入正向计数。 */
    if (measure->current_direction > 0)
    {
        measure->forward_count += pulse_count;
    }
    /* 当前方向为负，则本段脉冲归入负向计数。 */
    else if (measure->current_direction < 0)
    {
        measure->reverse_count += pulse_count;
    }
}

/**
 * @brief 检查并处理指定槽位的 GPTIMER 计数溢出。
 * @param slot 编码器槽位编号。
 * @retval true 计数器溢出已处理，本次不能再使用当前计数值。
 * @retval false 未发生溢出，可继续读取当前计数值。
 */
static bool USER_Encoder_TimerOverflowPending(uint8_t slot)
{
    /* 查询 GPTIMER 归零中断标志，归零说明本周期计数超出 16bit 范围。 */
    if (DL_Timer_getPendingInterrupt(encoder_config[slot].timer) == DL_TIMERG_IIDX_ZERO)
    {
        /* 清除硬件中断标志并重装计数器，准备后续继续计数。 */
        DL_Timer_clearInterruptStatus(encoder_config[slot].timer, DL_TIMERG_IIDX_ZERO);
        USER_ENCODER_RESET_COUNTER_FAST(encoder_config[slot].timer);

        /* 锁存溢出状态，并丢弃当前 active 计数，避免发布错误速度。 */
        encoder_config[slot].overflow_latched = true;
        USER_Encoder_ClearMeasure(&encoder_config[slot].active,
                                  encoder_config[slot].active.current_direction);
        return true;
    }

    return false;
}

/**
 * @brief 结算当前方向下尚未归档的一段脉冲。
 * @param slot 编码器槽位编号。
 * @retval 无。
 *
 * 该函数在方向边沿中断和 10ms 周期发布时共用：
 * 先读取 GPTIMER 计数值，再按旧方向归入正向或负向计数，最后清空 GPTIMER。
 */
static void USER_Encoder_AccumulateCurrentSegment(uint8_t slot)
{
    uint16_t pulse_count;

    /* 槽位未初始化时直接返回，避免访问无效硬件指针。 */
    if ((slot >= ENCODER_COUNT) || (encoder_config[slot].initialized == false))
    {
        return;
    }

    /* 溢出时本段计数不可用，溢出状态留给发布周期处理。 */
    if (USER_Encoder_TimerOverflowPending(slot))
    {
        return;
    }

    /* 读取当前段脉冲数，并立即重装计数器开始下一段计数。 */
    pulse_count = USER_ENCODER_READ_PULSE_COUNT_FAST(encoder_config[slot].timer);
    USER_ENCODER_RESET_COUNTER_FAST(encoder_config[slot].timer);

    /* 按中断前/结算前的方向，把本段脉冲累计到正向或负向计数。 */
    USER_Encoder_AddPulseCount(&encoder_config[slot].active, pulse_count);
}

/**
 * @brief 处理方向 GPIO 边沿。
 * @param slot 发生方向边沿的编码器槽位编号。
 * @retval 无。
 *
 * 方向变化时必须先按旧方向结算上一段脉冲，再读取新方向作为后续脉冲归类依据。
 */
static void USER_Encoder_OnDirectionEdge(uint8_t slot)
{
    /* 槽位无效或未初始化时直接忽略该边沿。 */
    if ((slot >= ENCODER_COUNT) || (encoder_config[slot].initialized == false))
    {
        return;
    }

    /* 先用旧方向结算从上次清零到本次边沿之间的脉冲数。 */
    USER_Encoder_AccumulateCurrentSegment(slot);

    /* 再读取方向 GPIO，更新当前方向供下一段脉冲使用。 */
    encoder_config[slot].active.current_direction = USER_ENCODER_READ_DIRECTION_FAST(&encoder_config[slot]);
}

/**
 * @brief 将 32bit 有符号周期计数限幅到对外公开的 int16_t speed 字段。
 * @param signed_count 本周期有符号总脉冲数。
 * @param status 输出状态，可能为 OK、NO_PULSE 或 OVERFLOW。
 * @return 限幅后的 int16_t 速度计数。
 */
static int16_t USER_Encoder_ClampSignedCount(int32_t signed_count, EncoderStatus_t_Typedef *status)
{
    /* 正向计数超过 int16_t 范围时，上报溢出并限幅。 */
    if (signed_count > INT16_MAX)
    {
        *status = ENCODER_STA_OVERFLOW;
        return INT16_MAX;
    }

    /* 负向计数超过 int16_t 范围时，上报溢出并限幅。 */
    if (signed_count < INT16_MIN)
    {
        *status = ENCODER_STA_OVERFLOW;
        return INT16_MIN;
    }

    /* 无脉冲时单独给出 NO_PULSE，其余正常发布。 */
    *status = (signed_count == 0) ? ENCODER_STA_NO_PULSE : ENCODER_STA_OK;
    return (int16_t)signed_count;
}

/**
 * @brief 将一个编码器槽位的 active 当前周期数据发布到 shadow。
 * @param slot 编码器槽位编号。
 * @retval 无。
 *
 * 该函数由 SysTick 1ms 回调累计到 period 后调用，相当于编码器 10ms 周期结算。
 */
static void USER_Encoder_PublishShadow(uint8_t slot)
{
    uint32_t forward_count;
    uint32_t reverse_count;
    int32_t total_count;
    int8_t direction;

    /* 周期结束前，先把最后一段尚未因方向边沿结算的脉冲纳入 active。 */
    USER_Encoder_AccumulateCurrentSegment(slot);

    /* 读取 active 四字段到局部变量，避免 volatile 表达式求值顺序不确定。 */
    forward_count = encoder_config[slot].active.forward_count;
    reverse_count = encoder_config[slot].active.reverse_count;
    total_count = (int32_t)forward_count - (int32_t)reverse_count;
    direction = encoder_config[slot].active.current_direction;

    /* 写回 active 总和计数，便于调试观察当前周期最终值。 */
    encoder_config[slot].active.total_count = total_count;

    /* 发布完整影子样本，供前台 USER_Encoder_Task() 读取。 */
    encoder_config[slot].shadow.current_direction = direction;
    encoder_config[slot].shadow.forward_count = forward_count;
    encoder_config[slot].shadow.reverse_count = reverse_count;
    encoder_config[slot].shadow.total_count = total_count;
    encoder_config[slot].shadow_overflow = encoder_config[slot].overflow_latched;
    encoder_config[slot].shadow_ready = true;

    /* 清除本周期状态，保留当前方向作为下一周期起点。 */
    encoder_config[slot].overflow_latched = false;
    USER_Encoder_ClearMeasure(&encoder_config[slot].active, direction);
}

/**
 * @brief 编码器 SysTick 周期回调函数。
 * @retval 无。
 *
 * 该函数由 1ms SysTick 回调调用，只负责按 period 分频并发布影子样本。
 * 它不直接更新 encoder_data，避免在中断上下文中执行上层数据计算。
 */
void USER_Encoder_SysTickHandler(void)
{
    uint8_t i;

    /* 编码器模块尚未初始化时不处理。 */
    if (encoder_first_call)
    {
        return;
    }

    /* 逐路编码器累加 1ms tick，达到各自周期后发布 shadow。 */
    for (i = 0; i < ENCODER_COUNT; i++)
    {
        /* 跳过未初始化或周期非法的槽位。 */
        if ((encoder_config[i].initialized == false) || (encoder_config[i].period == 0u))
        {
            continue;
        }

        /* 1ms tick 分频，未达到发布周期则继续等待。 */
        systick_encoder[i]++;
        if (systick_encoder[i] < encoder_config[i].period)
        {
            continue;
        }

        /* 达到发布周期，清分频计数并发布 10ms 影子样本。 */
        systick_encoder[i] = 0;
        USER_Encoder_PublishShadow(i);
    }
}

/**
 * @brief 编码器前台任务。
 * @retval 无。
 *
 * 由协作式调度器周期调用。该函数读取 SysTick 发布的 shadow 样本，
 * 并更新对上层公开的 speed、direction、status 和 sum_distance。
 */
void USER_Encoder_Task(void)
{
    uint8_t i;
    int32_t signed_count;
    int8_t measured_direction;
    bool overflow;
    bool sample_ready;
    EncoderStatus_t_Typedef status;
    int16_t speed;

    /* 编码器未初始化时不发布数据。 */
    if (encoder_first_call)
    {
        return;
    }

    /* 逐路读取影子样本并转换为公开数据。 */
    for (i = 0; i < ENCODER_COUNT; i++)
    {
        /* 槽位未初始化或没有绑定输出数据结构时跳过。 */
        if ((encoder_config[i].initialized == false) || (encoder_config[i].data_ptr == NULL))
        {
            continue;
        }

        /* shadow 由 SysTick 写入，读取和清 ready 标志需要短暂关中断保护。 */
        __disable_irq();
        sample_ready = encoder_config[i].shadow_ready;
        if (sample_ready)
        {
            signed_count = encoder_config[i].shadow.total_count;
            measured_direction = encoder_config[i].shadow.current_direction;
            overflow = encoder_config[i].shadow_overflow;
            encoder_config[i].shadow_ready = false;
        }
        __enable_irq();

        /* 没有新周期样本时保持上一次公开数据不变。 */
        if (sample_ready == false)
        {
            continue;
        }

        /* 周期内曾发生硬件计数溢出时，本次速度置 0 并上报 OVERFLOW。 */
        if (overflow)
        {
            encoder_config[i].data_ptr->speed = 0;
            encoder_config[i].data_ptr->direction = measured_direction;
            encoder_config[i].data_ptr->status = ENCODER_STA_OVERFLOW;
            continue;
        }

        /* 将本周期有符号总脉冲数转换为公开 speed 字段和状态。 */
        speed = USER_Encoder_ClampSignedCount(signed_count, &status);
        encoder_config[i].data_ptr->speed = speed;
        encoder_config[i].data_ptr->status = status;

        /* 累计有符号里程，单位仍为编码器脉冲。 */
        encoder_config[i].data_ptr->sum_distance += speed;

        /* 上层方向按速度正负更新；零速时保留当前测得方向。 */
        if (speed > 0)
        {
            encoder_config[i].data_ptr->direction = 1;
        }
        else if (speed < 0)
        {
            encoder_config[i].data_ptr->direction = -1;
        }
        else
        {
            encoder_config[i].data_ptr->direction = measured_direction;
        }
    }
}

/**
 * @brief 初始化单路编码器槽位。
 * @param data_ptr 指向对外公开的编码器数据结构。
 * @param slot 编码器槽位编号，范围 0 到 ENCODER_COUNT-1。
 * @param timer 用作外部脉冲计数器的 GPTIMER 实例。
 * @param direction_port 方向 GPIO 端口。
 * @param direction_pin 方向 GPIO 引脚。
 * @param reverse_install 是否反向安装，true 表示逻辑方向取反。
 * @param period 编码器发布周期，单位 ms。
 * @retval true 初始化成功。
 * @retval false 参数非法或槽位已初始化。
 */
bool USER_Encoder_InitSlot(EncoderData_t_Typedef *data_ptr,
                           uint8_t slot,
                           GPTIMER_Regs *timer,
                           GPIO_Regs *direction_port,
                           uint32_t direction_pin,
                           bool reverse_install,
                           uint16_t period)
{
    uint16_t i;
    int8_t initial_direction;

    /* 参数检查：输出指针、槽位编号和定时器实例必须有效。 */
    if (data_ptr == NULL || slot >= ENCODER_COUNT || timer == NULL)
    {
        return false;
    }

    /* 第一次初始化任意槽位时，先清空整个编码器模块内部状态。 */
    if (encoder_first_call)
    {
        for (i = 0; i < ENCODER_COUNT; i++)
        {
            encoder_config[i].timer = NULL;
            encoder_config[i].period = 0u;
            encoder_config[i].data_ptr = NULL;
            encoder_config[i].direction_port = NULL;
            encoder_config[i].direction_pin = 0u;
            encoder_config[i].reverse_install = false;
            encoder_config[i].initialized = false;
            USER_Encoder_ClearMeasure(&encoder_config[i].active, 0);
            USER_Encoder_ClearMeasure(&encoder_config[i].shadow, 0);
            encoder_config[i].shadow_ready = false;
            encoder_config[i].overflow_latched = false;
            encoder_config[i].shadow_overflow = false;
            systick_encoder[i] = 0;
        }

        /* 注册 1ms SysTick 回调，用于累加到 period 后发布影子样本。 */
        USER_SysTick_RegisterCallback(USER_Encoder_SysTickHandler);
        encoder_first_call = false;
    }

    /* 同一槽位不允许重复初始化，避免覆盖正在使用的硬件配置。 */
    if (encoder_config[slot].initialized != false)
    {
        return false;
    }

    /* 保存当前槽位硬件资源和运行参数。 */
    encoder_config[slot].timer = timer;
    encoder_config[slot].period = period;
    encoder_config[slot].data_ptr = data_ptr;
    encoder_config[slot].direction_port = direction_port;
    encoder_config[slot].direction_pin = direction_pin;
    encoder_config[slot].reverse_install = reverse_install;

    /* 读取初始方向，并用它初始化 active 和 shadow。 */
    initial_direction = USER_ENCODER_READ_DIRECTION_FAST(&encoder_config[slot]);
    USER_Encoder_ClearMeasure(&encoder_config[slot].active, initial_direction);
    USER_Encoder_ClearMeasure(&encoder_config[slot].shadow, initial_direction);
    encoder_config[slot].shadow_ready = false;
    encoder_config[slot].overflow_latched = false;
    encoder_config[slot].shadow_overflow = false;

    /* 初始化对外公开数据，保证上层读取到一致的初始状态。 */
    data_ptr->speed = 0;
    data_ptr->direction = initial_direction;
    data_ptr->status = ENCODER_STA_OK;
    data_ptr->sum_distance = 0;

    /* 重装并启动 GPTIMER 外部脉冲计数器。 */
    DL_Timer_setTimerCount(timer, TIM_MAX_COUNTER);
    DL_Timer_startCounter(timer);

    /* 标记槽位初始化完成。 */
    encoder_config[slot].initialized = true;

    return true;
}

/**
 * @brief 初始化全部编码器。
 * @param encoder_data_array 指向长度为 ENCODER_COUNT 的编码器数据数组。
 * @retval true 三路编码器全部初始化成功。
 * @retval false 至少一路编码器初始化失败。
 */
bool USER_Encoder_Init(EncoderData_t_Typedef *encoder_data_array)
{
    uint8_t i;
    bool result = true;

    /* 参数检查：必须传入对外公开数据数组。 */
    if (encoder_data_array == NULL)
    {
        return false;
    }

    /* 先把全部公开数据置为未初始化状态。 */
    for (i = 0; i < ENCODER_COUNT; i++)
    {
        encoder_data_array[i].status = ENCODER_STA_UNINIT;
        encoder_data_array[i].speed = 0;
        encoder_data_array[i].direction = 0;
        encoder_data_array[i].sum_distance = 0;
    }

    /* 初始化 0 号编码器：路程计。 */
    if (!USER_Encoder_InitSlot(&encoder_data_array[0], 0, COMPARE_0_INST,
                               ENCODER_ODOM_DIR_PORT, ENCODER_ODOM_DIR_PIN, true, 10))
    {
        result = false;
    }

    /* 初始化 1 号编码器：左轮。 */
    if (!USER_Encoder_InitSlot(&encoder_data_array[1], 1, COMPARE_1_INST,
                               ENCODER_LEFT_DIR_PORT, ENCODER_LEFT_DIR_PIN, false, 10))
    {
        result = false;
    }

    /* 初始化 2 号编码器：右轮。 */
    if (!USER_Encoder_InitSlot(&encoder_data_array[2], 2, COMPARE_2_INST,
                               ENCODER_RIGHT_DIR_PORT, ENCODER_RIGHT_DIR_PIN, true, 10))
    {
        result = false;
    }

    /* 使能方向 GPIO 所在的 GROUP1 中断。 */
    NVIC_EnableIRQ(ENCODER_GPIOA_INT_IRQN);
    NVIC_EnableIRQ(ENCODER_GPIOB_INT_IRQN);

    return result;
}

/**
 * @brief GPIO GROUP1 中断服务函数，处理三路编码器方向边沿。
 * @retval 无。
 *
 * ODOM 方向引脚在 GPIOA，LEFT/RIGHT 方向引脚在 GPIOB。
 * 任意方向边沿到来时，先清除 GPIO 中断标志，再按对应槽位结算旧方向脉冲段。
 */
void GROUP1_IRQHandler(void)
{
    uint32_t gpio_status;

    /* 处理 0 号路程计方向边沿。 */
    gpio_status = DL_GPIO_getEnabledInterruptStatus(ENCODER_ODOM_DIR_PORT, ENCODER_ODOM_DIR_PIN);
    if ((gpio_status & ENCODER_ODOM_DIR_PIN) != 0u)
    {
        DL_GPIO_clearInterruptStatus(ENCODER_ODOM_DIR_PORT, ENCODER_ODOM_DIR_PIN);
        USER_Encoder_OnDirectionEdge(0u);
    }

    /* 一次读取 GPIOB 上左轮和右轮方向中断状态。 */
    gpio_status = DL_GPIO_getEnabledInterruptStatus(ENCODER_LEFT_DIR_PORT,
                                                    ENCODER_LEFT_DIR_PIN | ENCODER_RIGHT_DIR_PIN);

    /* 处理 1 号左轮方向边沿。 */
    if ((gpio_status & ENCODER_LEFT_DIR_PIN) != 0u)
    {
        DL_GPIO_clearInterruptStatus(ENCODER_LEFT_DIR_PORT, ENCODER_LEFT_DIR_PIN);
        USER_Encoder_OnDirectionEdge(1u);
    }

    /* 处理 2 号右轮方向边沿。 */
    if ((gpio_status & ENCODER_RIGHT_DIR_PIN) != 0u)
    {
        DL_GPIO_clearInterruptStatus(ENCODER_RIGHT_DIR_PORT, ENCODER_RIGHT_DIR_PIN);
        USER_Encoder_OnDirectionEdge(2u);
    }
}
