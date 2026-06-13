#ifndef USER_OS_H
#define USER_OS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file user_os.h
 * @brief 1ms tick 驱动的协作式伪 RTOS 调度器。
 *
 * 第一版调度器不做 PendSV 上下文切换，也不为任务分配独立栈。
 * 所有任务仍在 main while(1) 的前台上下文中运行，但由统一任务表管理周期、
 * offset 错峰、优先级、使能状态和基础运行统计。
 *
 * 设计约束：
 * - SysTick 中断只推进 USER_OS_TickISR()，不在中断中执行任务。
 * - 任务函数必须短小、非阻塞，不允许使用长时间 delay 或等待外设死循环。
 * - priority 数值越小，优先级越高。
 */

#define USER_OS_MAX_TASKS 16u
#define USER_OS_INVALID_TASK_ID 0xffu

typedef void (*USER_OS_TaskFunction_t)(void);

typedef struct
{
    const char *name;
    uint16_t period_ms;
    uint16_t offset_ms;
    uint8_t priority;
    bool enabled;

    uint32_t next_release_tick;
    uint32_t run_count;
    uint32_t overrun_count;
    uint32_t last_start_tick;
    uint32_t last_finish_tick;
    uint32_t last_cost_us;
    uint32_t max_cost_us;
} USER_OS_TaskStats_t;

void USER_OS_Init(void);
void USER_OS_TickISR(void);
void USER_OS_Run(void);

uint8_t USER_OS_RegisterTask(const char *name,
                             USER_OS_TaskFunction_t task,
                             uint16_t period_ms,
                             uint16_t offset_ms,
                             uint8_t priority);

bool USER_OS_SetTaskEnabled(uint8_t task_id, bool enabled);
bool USER_OS_GetTaskStats(uint8_t task_id, USER_OS_TaskStats_t *stats);
bool USER_OS_ClearTaskMaxCost(uint8_t task_id);
void USER_OS_ClearAllTaskMaxCost(void);
uint8_t USER_OS_GetTaskCount(void);
uint32_t USER_OS_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_OS_H */
