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
#include "userlib_systick.h" // SysTick驱动接口

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
    RELEASED = 0,
    PRESSED,
    LONG_PRESSED // 长按状态
} ButtonState_t;

/**
 * @brief 按钮事件类型。
 */
typedef enum
{
    USER_LBB_BUTTON_EVENT_NONE = 0,
    USER_LBB_BUTTON_EVENT_SHORT,
    USER_LBB_BUTTON_EVENT_LONG,
    USER_LBB_BUTTON_EVENT_LONG_REPEAT
} USER_LBB_ButtonEvent_t;

/**
 * @brief 按钮调试统计信息。
 *
 * 生成次数和消费次数都使用 uint16_t 自然回绕。
 * pending = generated - consumed。
 */
typedef struct
{
    uint16_t press_down_count;  // 按下边沿次数
    uint16_t short_press_count; // 短按生成次数（松手时确认）
    uint16_t long_press_count;  // 长按首次触发次数
    uint16_t long_repeat_count; // 长按重复触发次数

    uint16_t consumed_short_count;  // 短按消费次数
    uint16_t consumed_long_count;   // 长按首次触发消费次数
    uint16_t consumed_repeat_count; // 长按重复触发消费次数

    uint16_t pending_short_count;  // 未消费短按次数
    uint16_t pending_long_count;   // 未消费长按首次触发次数
    uint16_t pending_repeat_count; // 未消费长按重复触发次数

    uint16_t press_time_ms;       // 当前持续按下时间
    uint16_t repeat_time_ms;      // 当前长按重复计时
    ButtonState_t physical_state; /* 当前扫描到的物理状态 */
    bool is_pressed;              /* 当前物理上是否按下 */
    bool long_reported;           // 本次按下是否已触发过长按
} USER_LBB_ButtonStats_t;

// 外部变量声明
extern uint16_t led_countdown[LED_COUNT];
extern uint16_t buzzer_countdown;
extern uint16_t button_press_time[BUTTON_COUNT];  /* 按键按下持续时间 */
extern uint16_t button_repeat_time[BUTTON_COUNT]; /* 长按重复计时 */

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

/**
 * @brief 消费一个按钮事件。
 * @details 消费优先级：LONG → LONG_REPEAT → SHORT。
 *          调用后对应事件的 pending 计数减一。
 * @param button 按钮枚举值。
 * @return 事件类型；无事件时返回 USER_LBB_BUTTON_EVENT_NONE。
 */
USER_LBB_ButtonEvent_t USER_LBB_Button_ConsumeEvent(Button_t button);

/// @brief 获取未消费短按次数。
uint16_t USER_LBB_Button_GetPendingShortCount(Button_t button);

/// @brief 获取未消费长按首次触发次数。
uint16_t USER_LBB_Button_GetPendingLongCount(Button_t button);

/// @brief 获取未消费长按重复触发次数。
uint16_t USER_LBB_Button_GetPendingLongRepeatCount(Button_t button);

/// @brief 消费一个短按事件。
bool USER_LBB_Button_ConsumeShort(Button_t button);

/// @brief 消费一个长按首次触发事件。
bool USER_LBB_Button_ConsumeLong(Button_t button);

/// @brief 消费一个长按重复触发事件。
bool USER_LBB_Button_ConsumeLongRepeat(Button_t button);

/// @brief 获取按钮调试统计信息。
bool USER_LBB_Button_GetStats(Button_t button, USER_LBB_ButtonStats_t *stats);

/// @brief 清空指定按钮的事件计数和消费计数。
bool USER_LBB_Button_ClearStats(Button_t button);

/// @brief 清空全部按钮的事件计数和消费计数。
void USER_LBB_Button_ClearAllStats(void);

#endif // USERLIB_LBB_H
