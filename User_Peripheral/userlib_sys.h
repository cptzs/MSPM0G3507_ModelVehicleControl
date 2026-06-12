#ifndef USERLIB_SYS_H
#define USERLIB_SYS_H

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  /* 整数类型库函数 */
#include <stdbool.h> /* 布尔类型库函数 */
#include <string.h>  /* 字符串操作库函数 */
#include <stdlib.h>  /* 标准库函数 */

// SysTick 回调函数最大数量
#define SYSTICK_CALLBACK_MAX 16

// 周期任务挂起计数最大值，达到上限后继续饱和，避免前台长时间阻塞后计数溢出
#define SYSTEM_TIME_PENDING_MAX 255U

// 微秒级软件延时宏定义
#define delay_us(us) delay_cycles((us) * (CPUCLK_FREQ / 1000000))

// 毫秒级软件延时宏定义
#define delay_ms(ms) delay_cycles((ms) * (CPUCLK_FREQ / 1000))

/// @brief 系统周期任务挂起计数表
typedef struct
{
    volatile uint8_t t_1ms;    // 1ms 待执行次数
    volatile uint8_t t_2ms;    // 2ms 待执行次数
    volatile uint8_t t_5ms;    // 5ms 待执行次数
    volatile uint8_t t_10ms;   // 10ms 待执行次数
    volatile uint8_t t_20ms;   // 20ms 待执行次数
    volatile uint8_t t_50ms;   // 50ms 待执行次数
    volatile uint8_t t_100ms;  // 100ms 待执行次数
    volatile uint8_t t_200ms;  // 200ms 待执行次数
    volatile uint8_t t_500ms;  // 500ms 待执行次数
    volatile uint8_t t_1000ms; // 1000ms 待执行次数
} SystemTime_t;

// 函数声明
/// @brief 用户系统初始化函数
void USER_SYSTEM_Init(void);

/// @brief 注册SysTick回调函数
/// @param callback 要注册的回调函数指针
/// @return true: 注册成功或已注册, false: 注册失败（槽位已满）
bool USER_SYSTICK_RegisterCallback(void (*callback)(void));

/// @brief 注销SysTick回调函数
/// @param callback 要注销的回调函数指针
/// @return true: 注销成功, false: 注销失败（未找到该函数）
bool USER_SYSTICK_UnregisterCallback(void (*callback)(void));

/// @brief 获取已注册的回调函数数量
/// @return 已注册的回调函数数量
uint8_t USER_SYSTICK_GetCallbackCount(void);

/// @brief 检查指定回调函数是否已注册
/// @param callback 要检查的回调函数指针
/// @return true: 已注册, false: 未注册
bool USER_SYSTICK_IsCallbackRegistered(void (*callback)(void));

/// @brief 清除所有已注册的回调函数
void USER_SYSTICK_ClearAllCallbacks(void);

/// @brief 消耗一个周期任务挂起计数。
/// @param time_counter 要消耗的 system_time 成员地址。
/// @return true 表示成功消耗 1 次，false 表示当前没有待执行任务。
bool USER_SYSTEM_ConsumeTime(volatile uint8_t *time_counter);

extern uint32_t sysTick;         // 全局系统Tick计数器
extern SystemTime_t system_time; // 系统周期任务挂起计数表

#endif // USERLIB_SYS_H
