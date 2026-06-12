#ifndef USERLIB_PWM_H
#define USERLIB_PWM_H

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  /* 整数类型库函数 */
#include <stdbool.h> /* 布尔类型库函数 */

/// @brief 设置PWM通道的占空比
/// @param timer 定时器实例
/// @param channel 通道号
/// @param dutyCycle 占空比（分辨率0-1000）
void USER_PWM_SetDutyCycle(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX channel, uint16_t dutyCycle);

/// @brief 设置PWM频率
/// @param timer 定时器实例
/// @param frequency PWM频率
/// @param reloadValue 重载值
void USER_PWM_SetFrequency(GPTIMER_Regs *timer, uint32_t frequency, uint16_t reloadValue);

/// @brief 启动PWM模块
/// @param timer 定时器实例
void USER_PWM_Start(GPTIMER_Regs *timer);

/// @brief 停止PWM模块
/// @param timer 定时器实例
void USER_PWM_Stop(GPTIMER_Regs *timer);

#endif //_USERLIB_PWM_H_