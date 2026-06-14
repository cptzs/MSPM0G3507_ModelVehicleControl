/**
 * @file userlib_lbb.c
 * @brief LED / 蜂鸣器 / 按钮 (LBB) 硬件驱动。
 *
 * 实现计数型按钮事件模型：短按/长按/长按重复各维护独立 pending 计数器，
 * 上层通过消费接口读取并清除事件，避免同一事件被多次处理。
 * 同时管理 4 路 LED 闪烁定时和蜂鸣器单次/定时鸣响。
 */

#include "userlib_lbb.h"

/* ---- GPIO 管理状态 ---- */
uint16_t led_countdown[LED_COUNT] = {0};         /* LED 闪烁倒计时 */
uint16_t buzzer_countdown = 0;                   /* 蜂鸣器倒计时 */
uint16_t button_scan_countdown = 0;              /* 按钮扫描倒计时 */
uint8_t button_state_old[BUTTON_COUNT] = {0};    /* 按钮物理状态旧值 */
uint16_t button_press_time[BUTTON_COUNT] = {0};  /* 按键按下持续时间 */
uint16_t button_repeat_time[BUTTON_COUNT] = {0}; /* 长按重复计时 */

static uint16_t button_down_count[BUTTON_COUNT] = {0};
static uint16_t button_short_count[BUTTON_COUNT] = {0};
static uint16_t button_long_count[BUTTON_COUNT] = {0};
static uint16_t button_long_repeat_count[BUTTON_COUNT] = {0};

static uint16_t button_short_consumed[BUTTON_COUNT] = {0};
static uint16_t button_long_consumed[BUTTON_COUNT] = {0};
static uint16_t button_long_repeat_consumed[BUTTON_COUNT] = {0};

static bool button_long_reported[BUTTON_COUNT] = {false};

/**
 * @brief LED 引脚定义数组。
 */
const uint32_t LEDS_PINS[LED_COUNT] = {
    LED_LED0_PIN, /* LED0 */
    LED_LED1_PIN, /* LED1 */
    LED_LED2_PIN, /* LED2 */
    LED_LED3_PIN, /* LED3 */
};

/**
 * @brief 按钮引脚定义数组（低电平有效）。
 */
const uint32_t BUTTON_PINS[BUTTON_COUNT] = {
    BUTTON_UP_PIN,    /* 上按钮 */
    BUTTON_DOWN_PIN,  /* 下按钮 */
    BUTTON_LEFT_PIN,  /* 左按钮 */
    BUTTON_RIGHT_PIN, /* 右按钮 */
    BUTTON_PREV_PIN,  /* 上一页按钮 */
    BUTTON_NEXT_PIN,  /* 下一页按钮 */
    BUTTON_ENTER_PIN, /* 确认按钮 */
    BUTTON_ESC_PIN    /* 取消按钮 */
};

/**
 * @brief 校验按钮枚举值是否合法。
 * @param button 按钮枚举值。
 * @retval true 合法。
 * @retval false 越界。
 */
static bool USER_BoardIO_Button_IsValid(Button_t button)
{
    int button_index = (int)button;
    return ((button_index >= 0) && (button_index < BUTTON_COUNT));
}

/**
 * @brief 计算未消费的短按事件数量（生成数 - 已消费数）。
 */
static uint16_t USER_BoardIO_Button_PendingShortRaw(uint8_t button)
{
    return (uint16_t)(button_short_count[button] - button_short_consumed[button]);
}

/**
 * @brief 计算未消费的长按事件数量。
 */
static uint16_t USER_BoardIO_Button_PendingLongRaw(uint8_t button)
{
    return (uint16_t)(button_long_count[button] - button_long_consumed[button]);
}

/**
 * @brief 计算未消费的长按重复事件数量。
 */
static uint16_t USER_BoardIO_Button_PendingLongRepeatRaw(uint8_t button)
{
    return (uint16_t)(button_long_repeat_count[button] - button_long_repeat_consumed[button]);
}

/**
 * @brief 安全的 uint16_t 累加（防溢出，达上限后饱和）。
 * @param value 被累加变量指针。
 * @param delta 增量。
 */
static void USER_BoardIO_Button_AddTime(uint16_t *value, uint16_t delta)
{
    if (*value <= (uint16_t)(UINT16_MAX - delta))
    {
        *value = (uint16_t)(*value + delta);
    }
    else
    {
        *value = UINT16_MAX;
    }
}

/**
 * @brief 确认一次短按事件：递增短按计数、更新兼容状态、蜂鸣反馈 20ms。
 */
static void USER_BoardIO_Button_OnShort(uint8_t button)
{
    button_short_count[button]++;
    USER_BoardIO_Buzzer_On(20);
}

/**
 * @brief 确认一次长按事件：递增长按计数、标记已触发、重置重复计时、蜂鸣反馈 10ms。
 */
static void USER_BoardIO_Button_OnLong(uint8_t button)
{
    button_long_count[button]++;
    button_long_reported[button] = true;
    button_repeat_time[button] = 0u;
    USER_BoardIO_Buzzer_On(10);
}

/**
 * @brief 确认一次长按重复事件：递增重复计数、更新兼容状态、蜂鸣反馈 10ms。
 */
static void USER_BoardIO_Button_OnLongRepeat(uint8_t button)
{
    button_long_repeat_count[button]++;
    USER_BoardIO_Buzzer_On(10);
}

/**
 * @brief GPIO 处理的 SysTick 回调 (每 1ms)。
 * @details 同时管理 LED 闪烁、蜂鸣器定时和按钮扫描状态机。
 *          按钮扫描策略：每隔 BUTTON_SCAN_INTERVAL ms 采样一次物理电平，
 *          通过边沿和时长判断生成短按/长按/长按重复事件。
 */
void USER_BoardIO_ButtonSysTickCallback(void)
{
    uint8_t i;
    ButtonState_t button_physical_state;

    // LED输出管理
    for (i = 0; i < LED_COUNT; i++)
    {
        // 只处理有效的LED引脚（非0）
        if (LEDS_PINS[i] != 0)
        {
            if (led_countdown[i] > 0)
            {
                led_countdown[i]--;
                DL_GPIO_clearPins(LED_PORT, LEDS_PINS[i]); // 打开LED
            }
            else
            {
                DL_GPIO_setPins(LED_PORT, LEDS_PINS[i]); // 关闭LED
            }
        }
    }

    // 蜂鸣器输出管理
    if (buzzer_countdown > 0)
    {
        buzzer_countdown--;
        DL_GPIO_clearPins(BUZZER_PORT, BUZZER_BUZZER0_PIN); // 打开蜂鸣器
    }
    else
    {
        DL_GPIO_setPins(BUZZER_PORT, BUZZER_BUZZER0_PIN); // 关闭蜂鸣器
    }

    // 按钮状态管理
    // 策略：每隔 BUTTON_SCAN_INTERVAL 扫描一次物理电平。
    // 短按：按下后未触发长按，松手时生成 SHORT 事件。
    // 长按：持续按下超过 BUTTON_LONG_PRESS_TIME 时生成 LONG 事件。
    // 长按重复：长按触发后，每 BUTTON_REPEAT_INTERVAL 生成 LONG_REPEAT 事件。
    if (button_scan_countdown > 0)
    {
        button_scan_countdown--;
    }
    else
    {
        button_scan_countdown = BUTTON_SCAN_INTERVAL;

        for (i = 0; i < BUTTON_COUNT; i++)
        {
            // 低电平有效：按下为 PRESSED，释放为 RELEASED。
            button_physical_state = ((DL_GPIO_readPins(BUTTON_PORT, BUTTON_PINS[i]) & BUTTON_PINS[i]) == 0)
                                        ? PRESSED
                                        : RELEASED;

            if (button_physical_state == PRESSED)
            {
                if (button_state_old[i] == RELEASED)
                {
                    // 按下边沿：只记录按下次数，不立即认定短按。
                    button_down_count[i]++;
                    button_press_time[i] = 0u;
                    button_repeat_time[i] = 0u;
                    button_long_reported[i] = false;
                }
                else
                {
                    // 持续按下：更新按下时长。
                    USER_BoardIO_Button_AddTime(&button_press_time[i], BUTTON_SCAN_INTERVAL);

                    if (!button_long_reported[i])
                    {
                        if (button_press_time[i] >= BUTTON_LONG_PRESS_TIME)
                        {
                            USER_BoardIO_Button_OnLong(i);
                        }
                    }
                    else
                    {
                        USER_BoardIO_Button_AddTime(&button_repeat_time[i], BUTTON_SCAN_INTERVAL);

                        if (button_repeat_time[i] >= BUTTON_REPEAT_INTERVAL)
                        {
                            button_repeat_time[i] = 0u;
                            USER_BoardIO_Button_OnLongRepeat(i);
                        }
                    }
                }
            }
            else
            {
                if (button_state_old[i] == PRESSED)
                {
                    // 松手边沿：本次按下没有触发过长按，则确认一个短按。
                    if (!button_long_reported[i])
                    {
                        USER_BoardIO_Button_OnShort(i);
                    }
                }

                button_press_time[i] = 0u;
                button_repeat_time[i] = 0u;
                button_long_reported[i] = false;
            }

            // 更新物理状态记录。
            button_state_old[i] = (uint8_t)button_physical_state;
        }
    }
}

/**
 * @brief 获取指定按钮的未消费短按事件数量。
 * @param button 按钮枚举值。
 * @return 待消费的短按计数。
 */
uint16_t USER_BoardIO_Button_GetPendingShortCount(Button_t button)
{
    if (!USER_BoardIO_Button_IsValid(button))
    {
        return 0u;
    }

    return USER_BoardIO_Button_PendingShortRaw((uint8_t)button);
}

/**
 * @brief 获取指定按钮的未消费长按事件数量。
 * @param button 按钮枚举值。
 * @return 待消费的长按计数。
 */
uint16_t USER_BoardIO_Button_GetPendingLongCount(Button_t button)
{
    if (!USER_BoardIO_Button_IsValid(button))
    {
        return 0u;
    }

    return USER_BoardIO_Button_PendingLongRaw((uint8_t)button);
}

/**
 * @brief 获取指定按钮的未消费长按重复事件数量。
 * @param button 按钮枚举值。
 * @return 待消费的长按重复计数。
 */
uint16_t USER_BoardIO_Button_GetPendingLongRepeatCount(Button_t button)
{
    if (!USER_BoardIO_Button_IsValid(button))
    {
        return 0u;
    }

    return USER_BoardIO_Button_PendingLongRepeatRaw((uint8_t)button);
}

/**
 * @brief 消费一个短按事件。
 * @details 读取并清除一个待处理的短按事件。
 * @param button 按钮枚举值。
 * @retval true 存在并成功消费了一个短按事件。
 * @retval false 无待消费的短按事件或按钮无效。
 */
bool USER_BoardIO_Button_ConsumeShort(Button_t button)
{
    uint8_t index;

    if (!USER_BoardIO_Button_IsValid(button))
    {
        return false;
    }

    index = (uint8_t)button;
    if (USER_BoardIO_Button_PendingShortRaw(index) == 0u)
    {
        return false;
    }

    button_short_consumed[index]++;
    return true;
}

/**
 * @brief 消费一个长按事件。
 * @param button 按钮枚举值。
 * @retval true 存在并成功消费了一个长按事件。
 * @retval false 无待消费的长按事件或按钮无效。
 */
bool USER_BoardIO_Button_ConsumeLong(Button_t button)
{
    uint8_t index;

    if (!USER_BoardIO_Button_IsValid(button))
    {
        return false;
    }

    index = (uint8_t)button;
    if (USER_BoardIO_Button_PendingLongRaw(index) == 0u)
    {
        return false;
    }

    button_long_consumed[index]++;
    return true;
}

/**
 * @brief 消费一个长按重复事件。
 * @param button 按钮枚举值。
 * @retval true 存在并成功消费了一个长按重复事件。
 * @retval false 无待消费的长按重复事件或按钮无效。
 */
bool USER_BoardIO_Button_ConsumeLongRepeat(Button_t button)
{
    uint8_t index;

    if (!USER_BoardIO_Button_IsValid(button))
    {
        return false;
    }

    index = (uint8_t)button;
    if (USER_BoardIO_Button_PendingLongRepeatRaw(index) == 0u)
    {
        return false;
    }

    button_long_repeat_consumed[index]++;
    return true;
}

/**
 * @brief 消费按钮事件（按优先级：长按 > 长按重复 > 短按）。
 * @details 调用后对应事件的 pending 计数减一，事件不会被重复消费。
 * @param button 按钮枚举值。
 * @return 事件类型；无事件时返回 USER_LBB_BUTTON_EVENT_NONE。
 */
USER_LBB_ButtonEvent_t USER_BoardIO_Button_ConsumeEvent(Button_t button)
{
    if (USER_BoardIO_Button_ConsumeLong(button))
    {
        return USER_LBB_BUTTON_EVENT_LONG;
    }

    if (USER_BoardIO_Button_ConsumeLongRepeat(button))
    {
        return USER_LBB_BUTTON_EVENT_LONG_REPEAT;
    }

    if (USER_BoardIO_Button_ConsumeShort(button))
    {
        return USER_LBB_BUTTON_EVENT_SHORT;
    }

    return USER_LBB_BUTTON_EVENT_NONE;
}

/**
 * @brief 获取指定按钮的完整统计信息。
 * @param button 按钮枚举值。
 * @param stats 输出统计结构体指针（不可为 NULL）。
 * @retval true 获取成功。
 * @retval false 参数无效。
 */
bool USER_BoardIO_Button_GetStats(Button_t button, USER_LBB_ButtonStats_t *stats)
{
    uint8_t index;

    if ((stats == NULL) || (!USER_BoardIO_Button_IsValid(button)))
    {
        return false;
    }

    index = (uint8_t)button;

    stats->press_down_count = button_down_count[index];
    stats->short_press_count = button_short_count[index];
    stats->long_press_count = button_long_count[index];
    stats->long_repeat_count = button_long_repeat_count[index];

    stats->consumed_short_count = button_short_consumed[index];
    stats->consumed_long_count = button_long_consumed[index];
    stats->consumed_repeat_count = button_long_repeat_consumed[index];

    stats->pending_short_count = USER_BoardIO_Button_PendingShortRaw(index);
    stats->pending_long_count = USER_BoardIO_Button_PendingLongRaw(index);
    stats->pending_repeat_count = USER_BoardIO_Button_PendingLongRepeatRaw(index);

    stats->press_time_ms = button_press_time[index];
    stats->repeat_time_ms = button_repeat_time[index];
    stats->physical_state = (ButtonState_t)button_state_old[index];
    stats->is_pressed = (button_state_old[index] == PRESSED);
    stats->long_reported = button_long_reported[index];

    return true;
}

/**
 * @brief 清零指定按钮的全部统计计数和状态。
 * @param button 按钮枚举值。
 * @retval true 清零成功。
 * @retval false 按钮无效。
 */
bool USER_BoardIO_Button_ClearStats(Button_t button)
{
    uint8_t index;

    if (!USER_BoardIO_Button_IsValid(button))
    {
        return false;
    }

    index = (uint8_t)button;

    button_down_count[index] = 0u;
    button_short_count[index] = 0u;
    button_long_count[index] = 0u;
    button_long_repeat_count[index] = 0u;
    button_short_consumed[index] = 0u;
    button_long_consumed[index] = 0u;
    button_long_repeat_consumed[index] = 0u;
    button_press_time[index] = 0u;
    button_repeat_time[index] = 0u;
    button_long_reported[index] = false;

    return true;
}

/**
 * @brief 清零所有按钮的全部统计计数和状态。
 */
void USER_BoardIO_Button_ClearAllStats(void)
{
    uint8_t i;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        (void)USER_BoardIO_Button_ClearStats((Button_t)i);
    }
}

/**
 * @brief 初始化 LBB 模块（LED、蜂鸣器、按钮 GPIO 及 SysTick 回调）。
 * @details 清零所有 LED/蜂鸣器倒计时和按钮统计，注册 SysTick 扫描回调。
 */
void USER_BoardIO_Init(void)
{
    uint8_t i;

    // 初始化LED倒计时
    for (i = 0; i < LED_COUNT; i++)
    {
        led_countdown[i] = 0;
    }

    // 初始化按钮状态
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        button_state_old[i] = RELEASED;
        button_press_time[i] = 0;
        button_repeat_time[i] = 0;
        button_long_reported[i] = false;
    }
    USER_BoardIO_Button_ClearAllStats();

    // 初始化蜂鸣器倒计时
    buzzer_countdown = 0;

    // 初始化按钮扫描倒计时
    button_scan_countdown = BUTTON_SCAN_INTERVAL;

    // 注册SysTick回调函数
    USER_SysTick_RegisterCallback(USER_BoardIO_ButtonSysTickCallback);
}
