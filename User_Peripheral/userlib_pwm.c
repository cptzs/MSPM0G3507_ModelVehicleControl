#include "userlib_pwm.h"

#define PWM_CLOCK_FREQUENCY CPUCLK_FREQ // 定义PWM时钟频率为CPU时钟频率

/// @brief 设置PWM通道的占空比
/// @param timer 定时器实例
/// @param channel 通道号
/// @param dutyCycle 占空比（分辨率0-1000）
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

/// @brief 初始化PWM模块
/// @param timer 定时器实例
void USER_PWM_Start(GPTIMER_Regs *timer)
{
    // 启动计数器
    timer->COUNTERREGS.CTRCTL |= GPTIMER_CTRCTL_EN_ENABLED;
}

/// @brief 停止定时器所有PWM输出
/// @param timer 定时器实例
void USER_PWM_Stop(GPTIMER_Regs *timer)
{
    // 停止定时器的PWM输出
    timer->COUNTERREGS.CTRCTL &= ~(GPTIMER_CTRCTL_EN_ENABLED);
}
