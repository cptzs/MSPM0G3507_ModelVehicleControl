#include "user_os.h"

#include "userlib_systick.h"

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
static uint32_t os_load_window_start_tick = 0u;
static uint32_t os_load_idle_us = 0u;
static uint16_t os_cpu_load_permille = 0u;

/**
 * @brief 判断 now 是否已经到达 target，支持 uint32_t 回绕。
 * @param now 当前 tick 计数。
 * @param target 目标 tick 计数。
 * @retval true now 已经到达或超过 target，false 还未到达。
 */
static bool USER_OS_TickReached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target) >= 0);
}

/**
 * @brief 判断 candidate 的释放时间是否早于 selected，支持 uint32_t 回绕。
 * @param candidate 待比较的任务释放 tick。
 * @param selected 当前选中任务的释放 tick。
 * @retval true candidate 的释放时间早于 selected，false 否则。
 */
static bool USER_OS_IsEarlierRelease(uint32_t candidate, uint32_t selected)
{
    return ((int32_t)(selected - candidate) > 0);
}

/**
 * @brief 获取基于 1ms SysTick 的近似微秒时间戳。
 *
 * SysTick 已由 SysConfig 配置为 1ms 周期。os_tick 提供毫秒高位，
 * SysTick->VAL 提供当前 1ms 内的向下计数值，用两者组合得到 us 级时间。
 * 该时间戳只用于短时间差统计，允许 uint32_t 自然回绕。
 */
static uint32_t USER_OS_GetTimeUs(void)
{
    uint32_t tick_before;
    uint32_t tick_after;
    uint32_t systick_pending;
    uint32_t systick_val;
    uint32_t load_cycles;
    uint32_t elapsed_cycles;
    uint32_t cycles_per_us;

    do
    {
        tick_before = os_tick;
        systick_val = SysTick->VAL;
        tick_after = os_tick;
        systick_pending = SCB->ICSR & SCB_ICSR_PENDSTSET_Msk;
    } while ((tick_before != tick_after) || (systick_pending != 0u));

    load_cycles = SysTick->LOAD + 1u;
    if (load_cycles > systick_val)
    {
        elapsed_cycles = load_cycles - systick_val;
    }
    else
    {
        elapsed_cycles = 0u;
    }

    cycles_per_us = (uint32_t)(CPUCLK_FREQ / 1000000u);
    if (cycles_per_us == 0u)
    {
        cycles_per_us = 1u;
    }

    return (tick_before * 1000u) + (elapsed_cycles / cycles_per_us);
}

/**
 * @brief 获取当前时间戳的近似值，不等待 SysTick 稳定。
 *
 * 该函数在无法保证 SysTick 稳定的上下文中调用，如空闲等待期间。
 * 可能存在误差，但足够用于统计空闲时间长度，避免在空闲等待中调用 USER_OS_GetTimeUs 导致死锁。
 */
static uint32_t USER_OS_GetTimeUsNoWait(void)
{
    uint32_t tick;
    uint32_t systick_val;
    uint32_t load_cycles;
    uint32_t elapsed_cycles;
    uint32_t cycles_per_us;

    tick = os_tick;
    systick_val = SysTick->VAL;
    load_cycles = SysTick->LOAD + 1u;
    if (load_cycles > systick_val)
    {
        elapsed_cycles = load_cycles - systick_val;
    }
    else
    {
        elapsed_cycles = 0u;
    }

    cycles_per_us = (uint32_t)(CPUCLK_FREQ / 1000000u);
    if (cycles_per_us == 0u)
    {
        cycles_per_us = 1u;
    }

    return (tick * 1000u) + (elapsed_cycles / cycles_per_us);
}

static void USER_OS_RecordIdleTime(uint32_t idle_us)
{
    os_load_idle_us += idle_us;
}

/**
 * @brief 从任务表中选择一个已经到期的任务执行。
 *
 * 遍历任务表，选择满足以下条件的任务：
 * 1. 已使用且启用。
 * 2. 任务函数指针非 NULL，周期非 0。
 * 3. 当前 tick 已经到达或超过 next_release_tick。
 *
 * 在满足上述条件的任务中，优先级数值较小的优先执行；如果优先级相同，则选择 next_release_tick 更早的任务。
 *
 * @param now 当前 tick 计数。
 * @retval 选中的任务索引，范围 0 到 os_task_count-1；如果没有到期任务，则返回 -1。
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
 * @brief 更新任务的下次释放时间。
 *
 * 根据当前 tick 和任务的周期，计算下次释放时间。若当前 tick 已经超过下次释放时间，
 * 则将 next_release_tick 向前推进一个或多个周期，直到 next_release_tick 在未来。
 * 如果需要推进多个周期，则认为任务发生了过期，增加 overrun_count 以供统计。
 *
 * @param tcb 任务控制块指针。
 * @param now 当前 tick 计数。
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

/**
 * @brief 初始化操作系统。
 */
void USER_OS_Init(void)
{
    uint8_t i;

    os_tick = 0u;
    os_task_count = 0u;
    os_load_window_start_tick = 0u;
    os_load_idle_us = 0u;
    os_cpu_load_permille = 0u;

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

    (void)USER_SysTick_RegisterCallback(USER_OS_TickISR);
}

/**
 * @brief SysTick 中断服务程序，每 1ms 调用一次。
 *
 * 该函数由 SysTick 定时器中断触发，负责增加系统 tick 计数。
 * 由于 os_tick 是 volatile 类型，确保在中断和主循环之间正确同步。
 */
void USER_OS_TickISR(void)
{
    os_tick++;
}

/**
 * @brief 注册协作式调度任务。
 * @param name 任务名称（调试用，最大 8 字符）。
 * @param task 任务函数指针（须短小、非阻塞）。
 * @param period_ms 执行周期 (ms)。
 * @param offset_ms 首次触发偏移 (ms)，用于错峰降低瞬时负载。
 * @param priority 优先级（数值越小越高）。
 * @retval 任务 ID (0到USER_OS_MAX_TASKS-1)，失败返回 USER_OS_INVALID_TASK_ID。
 */
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

/**
 * @brief 检查当前是否存在已经到期的任务。
 */
bool USER_OS_HasReadyTask(void)
{
    return (USER_OS_SelectReadyTask(USER_OS_GetTick()) >= 0);
}

/**
 * @brief 更新 CPU 负载统计。
 *
 * 基于 1 秒窗口，记录空闲时间占比计算负载百分比。每次调用 USER_OS_Run 后更新一次，
 * 确保负载统计反映实际运行情况。
 *
 * @param now_tick 当前 tick 计数。
 */
static void USER_OS_UpdateCpuLoad(uint32_t now_tick)
{
    uint32_t window_ms;
    uint32_t window_us;
    uint32_t idle_us;
    uint32_t load;

    window_ms = now_tick - os_load_window_start_tick;
    if (window_ms < 1000u)
    {
        return;
    }

    window_us = window_ms * 1000u;
    idle_us = (os_load_idle_us > window_us) ? window_us : os_load_idle_us;
    if (window_us == 0u)
    {
        os_cpu_load_permille = 0u;
    }
    else
    {
        load = ((window_us - idle_us) * 1000u) / window_us;
        os_cpu_load_permille = (load > 1000u) ? 1000u : (uint16_t)load;
    }

    os_load_idle_us = 0u;
    os_load_window_start_tick = now_tick;
}

/**
 * @brief 进入空闲等待状态，直到下一个中断唤醒。
 *
 * 在没有到期任务时调用，记录空闲时间用于 CPU 负载统计。
 */
void USER_OS_IdleWait(void)
{
    uint32_t idle_start_us;
    uint32_t idle_finish_us;

    __disable_irq();
    if (!USER_OS_HasReadyTask())
    {
        idle_start_us = USER_OS_GetTimeUsNoWait();
        __WFI();
        __enable_irq();
        idle_finish_us = USER_OS_GetTimeUs();
        USER_OS_RecordIdleTime(idle_finish_us - idle_start_us);
        USER_OS_UpdateCpuLoad(USER_OS_GetTick());
    }
    else
    {
        __enable_irq();
    }
}

/**
 * @brief 调度器主循环：执行一个到期任务。
 * @details 每个 1ms tick 遍历任务表，按优先级升序执行到期任务。
 *          每个任务执行前后记录 us 级时间戳用于耗时统计。
 * @retval true 本次调用执行了一个任务。
 * @retval false 当前没有到期任务。
 */
bool USER_OS_Run(void)
{
    int16_t task_index;
    uint32_t now;
    uint32_t start_tick;
    uint32_t finish_tick;
    uint32_t start_us;
    uint32_t finish_us;
    uint32_t elapsed_us;
    USER_OS_TaskControlBlock_t *tcb;

    now = USER_OS_GetTick();
    task_index = USER_OS_SelectReadyTask(now);
    if (task_index < 0)
    {
        return false;
    }

    tcb = &os_tasks[task_index];
    USER_OS_UpdateNextRelease(tcb, now);

    start_tick = USER_OS_GetTick();
    start_us = USER_OS_GetTimeUs();
    tcb->last_start_tick = start_tick;

    tcb->task();

    finish_us = USER_OS_GetTimeUs();
    finish_tick = USER_OS_GetTick();
    elapsed_us = finish_us - start_us;

    tcb->last_finish_tick = finish_tick;
    tcb->last_cost_us = elapsed_us;
    if (tcb->last_cost_us > tcb->max_cost_us)
    {
        tcb->max_cost_us = tcb->last_cost_us;
    }

    if (elapsed_us >= ((uint32_t)tcb->period_ms * 1000u))
    {
        tcb->overrun_count++;
    }

    tcb->run_count++;
    USER_OS_UpdateCpuLoad(finish_tick);
    return true;
}

/**
 * @brief 获取当前 CPU 负载百分比，单位千分之一。
 *
 * 负载统计基于 1 秒窗口，记录空闲时间占比计算负载百分比。
 * 每次调用 USER_OS_Run 后更新一次，确保负载统计反映实际运行情况。
 * @retval 当前 CPU 负载，范围 0 到 1000 (0% 到 100%)。
 */
uint16_t USER_OS_GetCpuLoadPermille(void)
{
    USER_OS_UpdateCpuLoad(USER_OS_GetTick());
    return os_cpu_load_permille;
}

/**
 * @brief 启用或禁用指定任务。
 * @param task_id 任务 ID。
 * @param enabled true 启用任务，false 禁用任务。
 * @retval true 成功，false 任务 ID 无效。
 */
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

/**
 * @brief 获取指定任务的统计信息。
 * @param task_id 任务 ID。
 * @param stats 输出参数，返回任务统计信息。
 * @retval true 成功，false 任务 ID 无效或 stats 参数为 NULL。
 */
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

/**
 * @brief 清除指定任务的最大耗时统计。
 * @param task_id 任务 ID。
 * @retval true 成功，false 任务 ID 无效。
 */
bool USER_OS_ClearTaskMaxCost(uint8_t task_id)
{
    if ((task_id >= os_task_count) || (!os_tasks[task_id].used))
    {
        return false;
    }

    os_tasks[task_id].max_cost_us = 0u;
    return true;
}

/**
 * @brief 清除所有任务的最大耗时统计。
 */
void USER_OS_ClearAllTaskMaxCost(void)
{
    uint8_t i;

    for (i = 0u; i < os_task_count; i++)
    {
        if (os_tasks[i].used)
        {
            os_tasks[i].max_cost_us = 0u;
        }
    }
}

/**
 * @brief 获取当前注册的任务数量。
 * @retval 当前任务数量，范围 0 到 USER_OS_MAX_TASKS。
 */
uint8_t USER_OS_GetTaskCount(void)
{
    return os_task_count;
}

/**
 * @brief 获取当前系统 tick 计数。
 * @retval 当前 tick 计数，单位 ms。
 */
uint32_t USER_OS_GetTick(void)
{
    return os_tick;
}
