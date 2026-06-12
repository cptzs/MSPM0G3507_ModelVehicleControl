#include "userlib_lbb.h"

// GPIO管理
uint16_t led_countdown[LED_COUNT] = {0};         // LED倒计时
uint16_t buzzer_countdown = 0;                   // 蜂鸣器倒计时
uint16_t button_scan_countdown = 0;              // 按钮扫描倒计时
uint8_t button_state[BUTTON_COUNT] = {0};        // 兼容旧接口的下一事件状态
uint8_t button_state_old[BUTTON_COUNT] = {0};    // 按钮物理状态旧值
uint16_t button_press_time[BUTTON_COUNT] = {0};  // 按键按下持续时间
uint16_t button_repeat_time[BUTTON_COUNT] = {0}; // 长按重复计时

static uint16_t button_down_count[BUTTON_COUNT] = {0};
static uint16_t button_short_count[BUTTON_COUNT] = {0};
static uint16_t button_long_count[BUTTON_COUNT] = {0};
static uint16_t button_long_repeat_count[BUTTON_COUNT] = {0};

static uint16_t button_short_consumed[BUTTON_COUNT] = {0};
static uint16_t button_long_consumed[BUTTON_COUNT] = {0};
static uint16_t button_long_repeat_consumed[BUTTON_COUNT] = {0};

static bool button_long_reported[BUTTON_COUNT] = {false};

/// @brief LED引脚定义
const uint32_t LEDS_PINS[LED_COUNT] = {
    LED_LED0_PIN, // LED0
    LED_LED1_PIN, // LED1
    LED_LED2_PIN, // LED2
    LED_LED3_PIN, // LED3
};

/// @brief 按钮引脚定义
const uint32_t BUTTON_PINS[BUTTON_COUNT] = {
    BUTTON_UP_PIN,    // 上按钮
    BUTTON_DOWN_PIN,  // 下按钮
    BUTTON_LEFT_PIN,  // 左按钮
    BUTTON_RIGHT_PIN, // 右按钮
    BUTTON_PREV_PIN,  // 上一页按钮
    BUTTON_NEXT_PIN,  // 下一页按钮
    BUTTON_ENTER_PIN, // 确认按钮
    BUTTON_ESC_PIN    // 取消按钮
};

static bool USER_LBB_Button_IsValid(Button_t button)
{
    int button_index = (int)button;
    return ((button_index >= 0) && (button_index < BUTTON_COUNT));
}

static uint16_t USER_LBB_Button_PendingShortRaw(uint8_t button)
{
    return (uint16_t)(button_short_count[button] - button_short_consumed[button]);
}

static uint16_t USER_LBB_Button_PendingLongRaw(uint8_t button)
{
    return (uint16_t)(button_long_count[button] - button_long_consumed[button]);
}

static uint16_t USER_LBB_Button_PendingLongRepeatRaw(uint8_t button)
{
    return (uint16_t)(button_long_repeat_count[button] - button_long_repeat_consumed[button]);
}

static void USER_LBB_Button_UpdateLegacyState(uint8_t button)
{
    if ((USER_LBB_Button_PendingLongRaw(button) > 0u) ||
        (USER_LBB_Button_PendingLongRepeatRaw(button) > 0u))
    {
        button_state[button] = LONG_PRESSED;
    }
    else if (USER_LBB_Button_PendingShortRaw(button) > 0u)
    {
        button_state[button] = PRESSED;
    }
    else
    {
        button_state[button] = RELEASED;
    }
}

static void USER_LBB_Button_AddTime(uint16_t *value, uint16_t delta)
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

static void USER_LBB_Button_OnShort(uint8_t button)
{
    button_short_count[button]++;
    USER_LBB_Button_UpdateLegacyState(button);
    USER_LBB_Buzzer_On(20);
}

static void USER_LBB_Button_OnLong(uint8_t button)
{
    button_long_count[button]++;
    button_long_reported[button] = true;
    button_repeat_time[button] = 0u;
    USER_LBB_Button_UpdateLegacyState(button);
    USER_LBB_Buzzer_On(10);
}

static void USER_LBB_Button_OnLongRepeat(uint8_t button)
{
    button_long_repeat_count[button]++;
    USER_LBB_Button_UpdateLegacyState(button);
    USER_LBB_Buzzer_On(10);
}

/// @brief GPIO处理的SysTick回调函数
/// @param  无
/// @return 无
/// @note 该函数在SysTick中断时被调用，用于处理GPIO相关的操作
///       包括LED控制、蜂鸣器控制和按钮状态管理
void USER_SysTick_Callback_GPIO_Process(void)
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
                    USER_LBB_Button_AddTime(&button_press_time[i], BUTTON_SCAN_INTERVAL);

                    if (!button_long_reported[i])
                    {
                        if (button_press_time[i] >= BUTTON_LONG_PRESS_TIME)
                        {
                            USER_LBB_Button_OnLong(i);
                        }
                    }
                    else
                    {
                        USER_LBB_Button_AddTime(&button_repeat_time[i], BUTTON_SCAN_INTERVAL);

                        if (button_repeat_time[i] >= BUTTON_REPEAT_INTERVAL)
                        {
                            button_repeat_time[i] = 0u;
                            USER_LBB_Button_OnLongRepeat(i);
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
                        USER_LBB_Button_OnShort(i);
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

uint16_t USER_LBB_Button_GetPendingShortCount(Button_t button)
{
    if (!USER_LBB_Button_IsValid(button))
    {
        return 0u;
    }

    return USER_LBB_Button_PendingShortRaw((uint8_t)button);
}

uint16_t USER_LBB_Button_GetPendingLongCount(Button_t button)
{
    if (!USER_LBB_Button_IsValid(button))
    {
        return 0u;
    }

    return USER_LBB_Button_PendingLongRaw((uint8_t)button);
}

uint16_t USER_LBB_Button_GetPendingLongRepeatCount(Button_t button)
{
    if (!USER_LBB_Button_IsValid(button))
    {
        return 0u;
    }

    return USER_LBB_Button_PendingLongRepeatRaw((uint8_t)button);
}

bool USER_LBB_Button_ConsumeShort(Button_t button)
{
    uint8_t index;

    if (!USER_LBB_Button_IsValid(button))
    {
        return false;
    }

    index = (uint8_t)button;
    if (USER_LBB_Button_PendingShortRaw(index) == 0u)
    {
        return false;
    }

    button_short_consumed[index]++;
    USER_LBB_Button_UpdateLegacyState(index);
    return true;
}

bool USER_LBB_Button_ConsumeLong(Button_t button)
{
    uint8_t index;

    if (!USER_LBB_Button_IsValid(button))
    {
        return false;
    }

    index = (uint8_t)button;
    if (USER_LBB_Button_PendingLongRaw(index) == 0u)
    {
        return false;
    }

    button_long_consumed[index]++;
    USER_LBB_Button_UpdateLegacyState(index);
    return true;
}

bool USER_LBB_Button_ConsumeLongRepeat(Button_t button)
{
    uint8_t index;

    if (!USER_LBB_Button_IsValid(button))
    {
        return false;
    }

    index = (uint8_t)button;
    if (USER_LBB_Button_PendingLongRepeatRaw(index) == 0u)
    {
        return false;
    }

    button_long_repeat_consumed[index]++;
    USER_LBB_Button_UpdateLegacyState(index);
    return true;
}

USER_LBB_ButtonEvent_t USER_LBB_Button_ConsumeEvent(Button_t button)
{
    if (USER_LBB_Button_ConsumeLong(button))
    {
        return USER_LBB_BUTTON_EVENT_LONG;
    }

    if (USER_LBB_Button_ConsumeLongRepeat(button))
    {
        return USER_LBB_BUTTON_EVENT_LONG_REPEAT;
    }

    if (USER_LBB_Button_ConsumeShort(button))
    {
        return USER_LBB_BUTTON_EVENT_SHORT;
    }

    return USER_LBB_BUTTON_EVENT_NONE;
}

ButtonState_t USER_LBB_Button_ReadState(Button_t button)
{
    USER_LBB_ButtonEvent_t event = USER_LBB_Button_ConsumeEvent(button);

    switch (event)
    {
    case USER_LBB_BUTTON_EVENT_SHORT:
        return PRESSED;

    case USER_LBB_BUTTON_EVENT_LONG:
    case USER_LBB_BUTTON_EVENT_LONG_REPEAT:
        return LONG_PRESSED;

    case USER_LBB_BUTTON_EVENT_NONE:
    default:
        return RELEASED;
    }
}

bool USER_LBB_Button_GetStats(Button_t button, USER_LBB_ButtonStats_t *stats)
{
    uint8_t index;

    if ((stats == NULL) || (!USER_LBB_Button_IsValid(button)))
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

    stats->pending_short_count = USER_LBB_Button_PendingShortRaw(index);
    stats->pending_long_count = USER_LBB_Button_PendingLongRaw(index);
    stats->pending_repeat_count = USER_LBB_Button_PendingLongRepeatRaw(index);

    stats->press_time_ms = button_press_time[index];
    stats->repeat_time_ms = button_repeat_time[index];
    stats->physical_state = (ButtonState_t)button_state_old[index];
    stats->legacy_state = (ButtonState_t)button_state[index];
    stats->is_pressed = (button_state_old[index] == PRESSED);
    stats->long_reported = button_long_reported[index];

    return true;
}

bool USER_LBB_Button_ClearStats(Button_t button)
{
    uint8_t index;

    if (!USER_LBB_Button_IsValid(button))
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
    button_state[index] = RELEASED;

    return true;
}

void USER_LBB_Button_ClearAllStats(void)
{
    uint8_t i;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        (void)USER_LBB_Button_ClearStats((Button_t)i);
    }
}

/// @brief 初始化LED、蜂鸣器和按钮的GPIO引脚
/// @return 无
void USER_LBB_Init(void)
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
        button_state[i] = RELEASED;
        button_state_old[i] = RELEASED;
        button_press_time[i] = 0;
        button_repeat_time[i] = 0;
        button_long_reported[i] = false;
    }
    USER_LBB_Button_ClearAllStats();

    // 初始化蜂鸣器倒计时
    buzzer_countdown = 0;

    // 初始化按钮扫描倒计时
    button_scan_countdown = BUTTON_SCAN_INTERVAL;

    // 注册SysTick回调函数
    USER_SYSTICK_RegisterCallback(USER_SysTick_Callback_GPIO_Process);
}
