/**
 * @file userlib_pwm.c
 * @brief MSPM0 GPTIMER PWM 输出驱动。
 *
 * 提供 PWM 占空比、频率设置和启停控制，直接操作定时器寄存器。
 */

#include "userlib_pwm.h"

#define PWM_CLOCK_FREQUENCY CPUCLK_FREQ /* PWM时钟频率 = CPU时钟频率 */

/**
 * @brief 设置指定 PWM 通道的占空比。
 * @param timer 定时器实例基地址。
 * @param channel 比较通道索引 (DL_TIMER_CC_INDEX)。
 * @param dutyCycle 占空比（分辨率 0–1000，对应 0%–100%）。
 */
void USER_PWM_SetDutyCycle(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX channel, uint16_t dutyCycle)
{
    volatile uint32_t *pReg;
    pReg = &timer->COUNTERREGS.CC_01[0];
    pReg += (uint32_t)channel;
    *pReg = (uint32_t)dutyCycle;
}

/// @brief 设置PWM频率
/// @param timer 定时器实例
/// @param frequency PWM频率
/// @param reloadValue 重载值
void USER_PWM_SetFrequency(GPTIMER_Regs *timer, uint32_t frequency, uint16_t reloadValue)
{
    // 根据重载值、时钟频率和PWM频率计算计数器的分频值
    uint32_t prescaler = (CPUCLK_FREQ / (frequency * reloadValue)) - 1;
    // 设置时钟源为CPU时钟
    timer->CLKSEL = (uint32_t)DL_TIMER_CLOCK_BUSCLK;
    // 设置时钟源分频系数
    timer->CLKDIV = (uint32_t)DL_TIMER_CLOCK_DIVIDE_1;
    // 设置计数器的预分频值
    timer->COMMONREGS.CPS = prescaler;
    // 设置计数器重载值
    timer->COUNTERREGS.LOAD = reloadValue - 1;
}

/**
 * @brief 启动 PWM 计数器输出。
 * @param timer 定时器实例基地址。
 */
void USER_PWM_Start(GPTIMER_Regs *timer)
{
    // 启动计数器
    timer->COUNTERREGS.CTRCTL |= GPTIMER_CTRCTL_EN_ENABLED;
}

/**
 * @brief 停止定时器所有 PWM 输出。
 * @param timer 定时器实例基地址。
 */
void USER_PWM_Stop(GPTIMER_Regs *timer)
{
    // 停止定时器的PWM输出
    timer->COUNTERREGS.CTRCTL &= ~(GPTIMER_CTRCTL_EN_ENABLED);
}
