/**
 * @file userlib_systick.c
 * @brief 系统滴答定时器 (SysTick) 驱动与服务。
 *
 * 提供 1ms SysTick 中断、回调函数注册/注销。
 * 时间片轮询调度已由 USER_OS 协作式调度器替代。
 */

#include "userlib_systick.h"

/// @brief 全局系统Tick计数器，用于记录SysTick中断的次数
uint32_t sysTick = 0;

/// @brief SysTick回调函数数组，用于存储不同的回调函数
void (*systick_callback[SYSTICK_CALLBACK_MAX])(void) = {0};

/**
 * @brief 系统服务初始化。
 * @details 清空回调槽位、重置 tick 计数器。
 */
void USER_SYSTEM_Init(void)
{
    memset(systick_callback, 0, sizeof(systick_callback));
    sysTick = 0;
}

/**
 * @brief SysTick 定时器中断服务程序 (1ms)。
 * @details 递增系统 tick 计数器，遍历调用所有已注册的回调函数。
 */
void SysTick_Handler(void)
{
    uint16_t i;
    sysTick++; // 增加系统Tick计数器

    /* 检查是否有注册的回调函数 */
    for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
    {
        if (systick_callback[i] != 0)
        {
            /* 调用注册的回调函数 */
            systick_callback[i]();
        }
    }
}

/**
 * @brief 注册 SysTick 回调函数。
 * @param callback 要注册的回调函数指针。
 * @retval true 注册成功或已存在。
 * @retval false 槽位已满或参数为空。
 */
bool USER_SYSTICK_RegisterCallback(void (*callback)(void))
{
    uint16_t i;

    /* 输入参数检查 */
    if (callback == 0)
    {
        return false;
    }

    /* 如果回调函数已经注册，直接返回成功，避免重复占用槽位 */
    for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
    {
        if (systick_callback[i] == callback)
        {
            return true;
        }
    }

    /* 查找第一个空闲的回调函数槽位 */
    for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
    {
        if (systick_callback[i] == 0)
        {
            /* 注册回调函数 */
            systick_callback[i] = callback;
            return true; /* 成功注册回调函数 */
        }
    }
    /* 如果没有空闲槽位，返回失败 */
    return false;
}

/**
 * @brief 注销 SysTick 回调函数。
 * @param callback 要注销的回调函数指针。
 * @retval true 注销成功。
 * @retval false 未找到该回调或参数为空。
 */
bool USER_SYSTICK_UnregisterCallback(void (*callback)(void))
{
    uint16_t i;

    /* 输入参数检查 */
    if (callback == 0)
    {
        return false;
    }

    /* 查找并取消注册回调函数 */
    for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
    {
        if (systick_callback[i] == callback)
        {
            systick_callback[i] = 0; /* 清除回调函数 */
            return true;             /* 成功取消注册 */
        }
    }
    /* 如果没有找到回调函数，返回失败 */
    return false;
}

/**
 * @brief 获取当前已注册的回调函数数量。
 * @return 已注册回调数量。
 */
uint8_t USER_SYSTICK_GetCallbackCount(void)
{
    uint8_t count;
    uint16_t i;

    count = 0;
    for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
    {
        if (systick_callback[i] != 0)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief 检查指定回调是否已注册。
 * @param callback 要检查的回调函数指针。
 * @retval true 已注册。
 * @retval false 未注册或参数为空。
 */
bool USER_SYSTICK_IsCallbackRegistered(void (*callback)(void))
{
    uint16_t i;

    if (callback == 0)
    {
        return false;
    }

    for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
    {
        if (systick_callback[i] == callback)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief 清除所有已注册的回调函数。
 */
void USER_SYSTICK_ClearAllCallbacks(void)
{
    memset(systick_callback, 0, sizeof(systick_callback));
}
