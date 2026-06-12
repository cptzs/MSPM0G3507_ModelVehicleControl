#ifndef USERLIB_LBB_H
#define USERLIB_LBB_H

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  //整数类型库函数
#include <stdbool.h> //布尔类型库函数
#include <string.h>  //字符串操作库函数
#include <stdlib.h>  //标准库函数
#include <math.h>    //数学库函数

// 用户外设接口头文件
#include "userlib_sys.h" // 系统时间和SysTick相关函数

// LED控制相关宏定义
#define LED_COUNT 4

// LED枚举
typedef enum
{
    LED0 = 0, // LED0
    LED1,     // LED1
    LED2,     // LED2
    LED3      // LED3
} LED_t;

// 按钮控制相关宏定义
#define BUTTON_COUNT 8              // 按钮数量
#define BUTTON_SCAN_INTERVAL 50     // 按钮扫描间隔(ms)
#define BUTTON_LONG_PRESS_TIME 1000 // 长按判定时间(ms)
#define BUTTON_REPEAT_INTERVAL 100  // 长按重复触发间隔(ms)

// 按钮枚举
typedef enum
{
    UP = 0, // 上按钮
    DOWN,   // 下按钮
    LEFT,   // 左按钮
    RIGHT,  // 右按钮
    PREV,   // 上一页按钮
    NEXT,   // 下一页按钮
    ENTER,  // 确认按钮
    ESC     // 取消按钮
} Button_t;

// 按钮状态定义
typedef enum
{
    RELEASED,
    PRESSED,
    LONG_PRESSED // 长按状态
} ButtonState_t;

// 外部变量声明
extern uint16_t led_countdown[LED_COUNT];
extern uint16_t buzzer_countdown;
extern uint8_t button_state[BUTTON_COUNT];
extern uint8_t button_state_old[BUTTON_COUNT];
extern uint16_t button_press_time[BUTTON_COUNT];  // 按键按下持续时间
extern uint16_t button_repeat_time[BUTTON_COUNT]; // 长按重复计时

/// @brief 初始化LED、蜂鸣器和按钮
/// @details 该函数会初始化LED、蜂鸣器和按钮的GPIO端口，并设置初始状态
void USER_LBB_Init(void);

// LED控制内联函数
/// @brief 打开指定LED
/// @param led LED编号（使用LED_t枚举）
/// @param ms 持续时间，单位为毫秒
static inline void USER_LBB_LED_On(LED_t led, uint16_t ms)
{
    led_countdown[led] = ms;
}

/// @brief 关闭指定LED
/// @param led LED编号（使用LED_t枚举）
static inline void USER_LBB_LED_Off(LED_t led)
{
    led_countdown[led] = 0;
}

/// @brief 读取指定LED的点亮倒计时
/// @param led LED编号（使用LED_t枚举）
/// @return LED的点亮倒计时，单位为毫秒
static inline uint16_t USER_LBB_LED_ReadCountdown(LED_t led)
{
    return led_countdown[led];
}

// 蜂鸣器控制内联函数
/// @brief 打开蜂鸣器
/// @param ms 持续时间，单位为毫秒
static inline void USER_LBB_Buzzer_On(uint16_t ms)
{
    buzzer_countdown = ms;
}

/// @brief 关闭蜂鸣器
/// @note 关闭蜂鸣器时，倒计时将被清零
static inline void USER_LBB_Buzzer_Off(void)
{
    buzzer_countdown = 0;
}

/// @brief 获取指定按钮的当前状态（读取后自动清除短按和长按事件）
/// @param button 按钮编号（使用Button_t枚举）
/// @return 按钮状态：RELEASED(未按下), PRESSED(短按), LONG_PRESSED(长按)
static inline ButtonState_t USER_LBB_Button_ReadState(Button_t button)
{
    ButtonState_t state = button_state[button];
    if (state == PRESSED || state == LONG_PRESSED)
    {
        button_state[button] = RELEASED; // 读取后自动清除事件标志
    }
    return state;
}

#endif // USERLIB_LBB_H