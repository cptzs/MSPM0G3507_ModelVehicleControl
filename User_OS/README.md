# User_OS — 1ms tick 协作式伪 RTOS 调度器

## 概述

非抢占式协作调度器。所有任务在 `main()` 的 `while(1)` 前台上下文中运行，
不做 PendSV 上下文切换，不分配独立任务栈。适用于资源受限的 Cortex-M0+ MCU。

## 设计约束

- SysTick 中断只推进 `USER_OS_TickISR()`（累加 `os_tick`），**不在中断中执行任务**
- 任务函数必须**短小、非阻塞**，不允许长时间 delay 或死循环等待外设
- `priority` 数值越小，优先级越高
- 最大支持 16 个任务 (`USER_OS_MAX_TASKS`)

## 核心数据结构

```c
typedef struct {
    const char *name;              // 任务名（调试用）
    uint16_t period_ms;            // 执行周期
    uint16_t offset_ms;            // 首次触发偏移（错峰）
    uint8_t priority;              // 优先级（越小越高）
    bool enabled;                  // 使能状态

    uint32_t next_release_tick;    // 下次释放 tick
    uint32_t run_count;            // 执行次数
    uint32_t overrun_count;        // 超限次数
    uint32_t last_start_tick;      // 最近开始 tick
    uint32_t last_finish_tick;     // 最近完成 tick
    uint32_t last_cost_us;         // 最近耗时 (μs)
    uint32_t max_cost_us;          // 最大耗时 (μs)
} USER_OS_TaskStats_t;
```

## API

| 函数 | 说明 |
|------|------|
| `USER_OS_Init()` | 初始化调度器（清空任务表 + 注册 SysTick 回调） |
| `USER_OS_TickISR()` | SysTick ISR 调用（仅累加 `os_tick`） |
| `USER_OS_Run()` | 主循环调用：选择到期任务 → 按优先级执行 → 更新下次释放 tick |
| `USER_OS_RegisterTask(name, func, period_ms, offset_ms, priority)` | 注册任务，返回 task_id |
| `USER_OS_SetTaskEnabled(task_id, enabled)` | 使能/禁用任务 |
| `USER_OS_GetTaskStats(task_id, &stats)` | 获取任务运行统计 |
| `USER_OS_ClearTaskMaxCost(task_id)` | 清除指定任务最大耗时 |
| `USER_OS_ClearAllTaskMaxCost()` | 清除全部任务最大耗时 |
| `USER_OS_GetTaskCount()` | 获取已注册任务数 |
| `USER_OS_GetTick()` | 获取当前系统 tick |

## 调度策略

1. 每个 1ms tick，SysTick ISR 累加 `os_tick`
2. `USER_OS_Run()` 遍历任务表，找出所有 `next_release_tick ≤ os_tick` 的已启用任务
3. 按 `priority` **升序**排列就绪任务（同优先级按释放时间早的优先）
4. 逐一执行，每个任务执行前后记录微秒级时间戳用于耗时统计
5. 执行完毕后推进 `next_release_tick += period_ms`，并检测是否超限（overrun）

## 微秒级计时

通过组合 `os_tick`（毫秒高位）和 `SysTick->VAL`（当前 1ms 内向下计数值）获取
微秒级近似时间戳。结果只用于短时差统计，允许 uint32_t 自然回绕。

## 当前注册任务（`main.c`）

```c
USER_OS_RegisterTask("global",    USER_GlobalData_Task,  1,   0,  0);  // 1ms, pri 0
USER_OS_RegisterTask("state",     USER_State_Task,      10,  3,  1);  // 10ms, pri 1
USER_OS_RegisterTask("mcm",       USER_MCM_Task,        10,  4,  2);  // 10ms, pri 2
USER_OS_RegisterTask("race",      USER_Race_Task,       10,  5,  3);  // 10ms, pri 3
USER_OS_RegisterTask("lidar",     USER_LIDAR_Task,       5,  1,  4);  // 5ms, pri 4
USER_OS_RegisterTask("ui",        USER_UI_Task,          5,  2,  6);  // 5ms, pri 6
```

LED 心跳由 `USER_UI_Task()` 内部按 500ms 分频处理，不再占用独立调度任务。

**offset 错峰说明**：state/mcm/race 三个 10ms 任务错开 1ms（offset=3/4/5），
lidar/ui 两个 5ms 任务错开 1ms（offset=1/2），避免同一 tick 集中执行。
