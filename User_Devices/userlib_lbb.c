#include "userlib_lbb.h"

// GPIO管理
uint16_t led_countdown[LED_COUNT] = {0};         // LED倒计时
uint16_t buzzer_countdown = 0;                   // 蜂鸣器倒计时
uint16_t button_scan_countdown = 0;              // 按钮扫描倒计时
uint8_t button_state[BUTTON_COUNT] = {0};        // 按钮状态
uint8_t button_state_old[BUTTON_COUNT] = {0};    // 按钮状态旧值
uint16_t button_press_time[BUTTON_COUNT] = {0};  // 按键按下持续时间
uint16_t button_repeat_time[BUTTON_COUNT] = {0}; // 长按重复计时

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

/// @brief GPIO处理的SysTick回调函数
/// @param  无
/// @return 无
/// @note 该函数在SysTick中断时被调用，用于处理GPIO相关的操作
///       包括LED控制、蜂鸣器控制和按钮状态管理
void USER_SysTick_Callback_GPIO_Process(void)
{
    uint8_t i;
    uint32_t button_state_temp;

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
    // 策略：每隔一段时间扫描一次按钮状态，避免频繁读取引脚
    // 支持短按和长按连续触发：
    // 1. 短按：按下边沿触发一次PRESSED事件
    // 2. 长按：持续按下超过BUTTON_LONG_PRESS_TIME后，每隔BUTTON_REPEAT_INTERVAL触发LONG_PRESSED事件
    // 用户读取状态时会自动清除事件标志，确保事件处理的一致性
    if (button_scan_countdown > 0)
    {
        button_scan_countdown--;
    }
    else
    {
        button_scan_countdown = BUTTON_SCAN_INTERVAL;
        for (i = 0; i < BUTTON_COUNT; i++)
        {
            // 读取当前按钮的物理状态
            button_state_temp = ((DL_GPIO_readPins(BUTTON_PORT, BUTTON_PINS[i]) & BUTTON_PINS[i]) == 0)
                                    ? PRESSED
                                    : RELEASED;

            if (button_state_temp == PRESSED)
            {
                // 按钮当前被按下
                if (button_state_old[i] == RELEASED)
                {
                    // 检测到按下边沿：触发短按事件
                    button_state[i] = PRESSED;
                    button_press_time[i] = 0;  // 重置按下时间计数
                    button_repeat_time[i] = 0; // 重置重复计时
                    USER_LBB_Buzzer_On(20);    // 短按音效
                }
                else
                {
                    // 按钮持续按下：更新计时器
                    button_press_time[i] += BUTTON_SCAN_INTERVAL;

                    if (button_press_time[i] >= BUTTON_LONG_PRESS_TIME)
                    {
                        // 达到长按时间
                        button_repeat_time[i] += BUTTON_SCAN_INTERVAL;

                        if (button_repeat_time[i] >= BUTTON_REPEAT_INTERVAL)
                        {
                            // 达到重复触发间隔：触发长按事件
                            button_state[i] = LONG_PRESSED;
                            button_repeat_time[i] = 0; // 重置重复计时
                            USER_LBB_Buzzer_On(10);    // 长按音效（更短）
                        }
                    }
                }
            }
            else
            {
                // 按钮当前未被按下：重置所有计时器
                button_press_time[i] = 0;
                button_repeat_time[i] = 0;
            }

            // 更新物理状态记录
            button_state_old[i] = button_state_temp;
        }
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
    }

    // 初始化蜂鸣器倒计时
    buzzer_countdown = 0;

    // 初始化按钮扫描倒计时
    button_scan_countdown = BUTTON_SCAN_INTERVAL;

    // 注册SysTick回调函数
    USER_SYSTICK_RegisterCallback(USER_SysTick_Callback_GPIO_Process);
}
