#include "user_os.h"

#include "userlib_sys.h"

#include <stddef.h>

/**
 * @brief 调度器内部任务控制块。
 */
typedef struct
{
    const char *name;
    USER_OS_TaskFunction_t task;
    uint16_t period_ms;
    uint16_t offset_ms;
    uint8_t priority;
    bool enabled;
    bool used;

    uint32_t next_release_tick;
    uint32_t run_count;
    uint32_t overrun_count;
    uint32_t last_start_tick;
    uint32_t last_finish_tick;
    uint32_t last_cost_us;
    uint32_t max_cost_us;
} USER_OS_TaskControlBlock_t;

static volatile uint32_t os_tick = 0u;
static USER_OS_TaskControlBlock_t os_tasks[USER_OS_MAX_TASKS];
static uint8_t os_task_count = 0u;

/**
 * @brief 判断 now 是否已经到达 target，支持 uint32_t 回绕。
 */
static bool USER_OS_TickReached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target) >= 0);
}

/**
 * @brief 判断 candidate 是否比 selected 更早释放，支持 uint32_t 回绕。
 */
static bool USER_OS_IsEarlierRelease(uint32_t candidate, uint32_t selected)
{
    return ((int32_t)(selected - candidate) > 0);
}

/**
 * @brief 选择当前已经到期且优先级最高的任务。
 */
static int16_t USER_OS_SelectReadyTask(uint32_t now)
{
    uint8_t i;
    int16_t selected;

    selected = -1;
    for (i = 0u; i < os_task_count; i++)
    {
        if ((!os_tasks[i].used) || (!os_tasks[i].enabled) ||
            (os_tasks[i].task == NULL) || (os_tasks[i].period_ms == 0u))
        {
            continue;
        }

        if (!USER_OS_TickReached(now, os_tasks[i].next_release_tick))
        {
            continue;
        }

        if (selected < 0)
        {
            selected = (int16_t)i;
        }
        else if (os_tasks[i].priority < os_tasks[selected].priority)
        {
            selected = (int16_t)i;
        }
        else if ((os_tasks[i].priority == os_tasks[selected].priority) &&
                 USER_OS_IsEarlierRelease(os_tasks[i].next_release_tick,
                                           os_tasks[selected].next_release_tick))
        {
            selected = (int16_t)i;
        }
    }

    return selected;
}

/**
 * @brief 根据当前 tick 推进任务下一次释放时间，并统计漏调度次数。
 */
static void USER_OS_UpdateNextRelease(USER_OS_TaskControlBlock_t *tcb, uint32_t now)
{
    uint32_t releases;

    if ((tcb == NULL) || (tcb->period_ms == 0u))
    {
        return;
    }

    releases = 0u;
    do
    {
        tcb->next_release_tick += tcb->period_ms;
        releases++;
    } while (USER_OS_TickReached(now, tcb->next_release_tick) && (releases < 255u));

    if (releases > 1u)
    {
        tcb->overrun_count += (releases - 1u);
    }

    if (USER_OS_TickReached(now, tcb->next_release_tick))
    {
        tcb->next_release_tick = now + tcb->period_ms;
        tcb->overrun_count++;
    }
}

void USER_OS_Init(void)
{
    uint8_t i;

    os_tick = 0u;
    os_task_count = 0u;

    for (i = 0u; i < USER_OS_MAX_TASKS; i++)
    {
        os_tasks[i].name = NULL;
        os_tasks[i].task = NULL;
        os_tasks[i].period_ms = 0u;
        os_tasks[i].offset_ms = 0u;
        os_tasks[i].priority = 0xffu;
        os_tasks[i].enabled = false;
        os_tasks[i].used = false;
        os_tasks[i].next_release_tick = 0u;
        os_tasks[i].run_count = 0u;
        os_tasks[i].overrun_count = 0u;
        os_tasks[i].last_start_tick = 0u;
        os_tasks[i].last_finish_tick = 0u;
        os_tasks[i].last_cost_us = 0u;
        os_tasks[i].max_cost_us = 0u;
    }

    (void)USER_SYSTICK_RegisterCallback(USER_OS_TickISR);
}

void USER_OS_TickISR(void)
{
    os_tick++;
}

uint8_t USER_OS_RegisterTask(const char *name,
                             USER_OS_TaskFunction_t task,
                             uint16_t period_ms,
                             uint16_t offset_ms,
                             uint8_t priority)
{
    USER_OS_TaskControlBlock_t *tcb;

    if ((task == NULL) || (period_ms == 0u) || (os_task_count >= USER_OS_MAX_TASKS))
    {
        return USER_OS_INVALID_TASK_ID;
    }

    tcb = &os_tasks[os_task_count];
    tcb->name = name;
    tcb->task = task;
    tcb->period_ms = period_ms;
    tcb->offset_ms = offset_ms;
    tcb->priority = priority;
    tcb->enabled = true;
    tcb->used = true;
    tcb->next_release_tick = USER_OS_GetTick() + offset_ms;
    tcb->run_count = 0u;
    tcb->overrun_count = 0u;
    tcb->last_start_tick = 0u;
    tcb->last_finish_tick = 0u;
    tcb->last_cost_us = 0u;
    tcb->max_cost_us = 0u;

    os_task_count++;
    return (uint8_t)(os_task_count - 1u);
}

void USER_OS_Run(void)
{
    int16_t task_index;
    uint32_t now;
    uint32_t start_tick;
    uint32_t finish_tick;
    uint32_t elapsed_ms;
    USER_OS_TaskControlBlock_t *tcb;

    now = USER_OS_GetTick();
    task_index = USER_OS_SelectReadyTask(now);
    if (task_index < 0)
    {
        return;
    }

    tcb = &os_tasks[task_index];
    USER_OS_UpdateNextRelease(tcb, now);

    start_tick = USER_OS_GetTick();
    tcb->last_start_tick = start_tick;
    tcb->task();
    finish_tick = USER_OS_GetTick();

    elapsed_ms = finish_tick - start_tick;
    tcb->last_finish_tick = finish_tick;
    tcb->last_cost_us = elapsed_ms * 1000u;
    if (tcb->last_cost_us > tcb->max_cost_us)
    {
        tcb->max_cost_us = tcb->last_cost_us;
    }

    if (elapsed_ms >= tcb->period_ms)
    {
        tcb->overrun_count++;
    }

    tcb->run_count++;
}

bool USER_OS_SetTaskEnabled(uint8_t task_id, bool enabled)
{
    if ((task_id >= os_task_count) || (!os_tasks[task_id].used))
    {
        return false;
    }

    os_tasks[task_id].enabled = enabled;
    if (enabled)
    {
        os_tasks[task_id].next_release_tick = USER_OS_GetTick() + os_tasks[task_id].offset_ms;
    }
    return true;
}

bool USER_OS_GetTaskStats(uint8_t task_id, USER_OS_TaskStats_t *stats)
{
    USER_OS_TaskControlBlock_t *tcb;

    if ((stats == NULL) || (task_id >= os_task_count) || (!os_tasks[task_id].used))
    {
        return false;
    }

    tcb = &os_tasks[task_id];
    stats->name = tcb->name;
    stats->period_ms = tcb->period_ms;
    stats->offset_ms = tcb->offset_ms;
    stats->priority = tcb->priority;
    stats->enabled = tcb->enabled;
    stats->next_release_tick = tcb->next_release_tick;
    stats->run_count = tcb->run_count;
    stats->overrun_count = tcb->overrun_count;
    stats->last_start_tick = tcb->last_start_tick;
    stats->last_finish_tick = tcb->last_finish_tick;
    stats->last_cost_us = tcb->last_cost_us;
    stats->max_cost_us = tcb->max_cost_us;

    return true;
}

uint8_t USER_OS_GetTaskCount(void)
{
    return os_task_count;
}

uint32_t USER_OS_GetTick(void)
{
    return os_tick;
}
