# User_OS - 1ms tick 协作式调度器

## 概述

`User_OS` 提供非抢占式协作调度。所有任务都在 `main()` 的前台 `while(1)` 上下文中执行，不做 PendSV 上下文切换，不为任务分配独立栈。

当没有到期任务时，主循环调用 `USER_OS_IdleWait()` 进入 `__WFI()`。OS 层统计 WFI 空闲时间，并以此计算整体 CPU 利用率。

## 文件列表

| 文件 | 功能 |
|------|------|
| `user_os.c/h` | 任务注册、就绪选择、周期调度、任务耗时统计、CPU 利用率统计 |
| `user_firmware_info.c/h` | 固件版本、CPU 型号/主频、RAM/FLASH 占用统计 |

## 设计约束

- SysTick 中断只推进 `USER_OS_TickISR()`，不在中断中执行任务。
- 任务函数必须短小、非阻塞，不允许长时间 delay 或死循环等待外设。
- `priority` 数值越小，优先级越高。
- 最多支持 16 个任务 (`USER_OS_MAX_TASKS`)。
- CPU 利用率口径为“非 WFI 睡眠时间占比”，不是单纯任务函数耗时占比。

## API

| 函数 | 说明 |
|------|------|
| `USER_OS_Init()` | 初始化调度器并注册 SysTick 回调 |
| `USER_OS_TickISR()` | SysTick ISR 回调，仅累加 `os_tick` |
| `USER_OS_RegisterTask(name, func, period_ms, offset_ms, priority)` | 注册周期任务 |
| `USER_OS_Run()` | 执行一个到期任务；执行任务返回 `true`，无任务返回 `false` |
| `USER_OS_HasReadyTask()` | 检查当前是否存在到期任务 |
| `USER_OS_IdleWait()` | 无到期任务时进入 WFI，并累计空闲时间 |
| `USER_OS_GetCpuLoadPermille()` | 获取 CPU 利用率，单位为千分比 |
| `USER_OS_SetTaskEnabled(task_id, enabled)` | 使能或禁用任务 |
| `USER_OS_GetTaskStats(task_id, &stats)` | 获取任务运行统计 |
| `USER_OS_ClearTaskMaxCost(task_id)` | 清除指定任务最大耗时 |
| `USER_OS_ClearAllTaskMaxCost()` | 清除全部任务最大耗时 |
| `USER_OS_GetTaskCount()` | 获取注册任务数量 |
| `USER_OS_GetTick()` | 获取当前 1ms tick |

## 调度策略

1. SysTick 每 1ms 调用 `USER_OS_TickISR()`，递增 `os_tick`。
2. `USER_OS_Run()` 选择一个已经到期且优先级最高的任务。
3. 同优先级任务按更早的释放时间优先。
4. 任务执行前后记录微秒时间戳，更新 `last_cost_us` 和 `max_cost_us`。
5. 执行后推进 `next_release_tick`，并统计 overrun。
6. 当前没有任务时，主循环调用 `USER_OS_IdleWait()` 进入 WFI。

## CPU 利用率

CPU 利用率每约 1 秒更新一次：

```text
CPU load = 1 - idle_time_in_WFI / window_time
```

其中 `idle_time_in_WFI` 由 `USER_OS_IdleWait()` 在进入和退出 `__WFI()` 时累计。这个口径会把前台任务、调度开销和中断唤醒后的非睡眠时间都计入负载，更适合 SysInfo 页面显示整体系统忙闲程度。

## 固件信息

`user_firmware_info.c/h` 位于 `User_OS`，提供：

- 固件版本：`USER_FIRMWARE_VERSION`
- CPU 型号：`USER_FIRMWARE_CPU_MODEL`
- CPU 主频：`USER_Firmware_GetCpuClockHz()`
- RAM 总量/占用：`USER_Firmware_GetRamTotalBytes()` / `USER_Firmware_GetRamUsedBytes()`
- FLASH 总量/占用：`USER_Firmware_GetFlashTotalBytes()` / `USER_Firmware_GetFlashUsedBytes()`

当前版本号同时维护在：

- `main.c` 文件头 `@version`
- `User_OS/user_firmware_info.h`
- 仓库根目录 `VERSION`
- `CHANGELOG.md`

## 当前注册任务

```c
USER_OS_RegisterTask("global",  USER_GlobalData_Task, 1u,  0u, 0u);
USER_OS_RegisterTask("encoder", USER_Encoder_Task,    10u, 2u, 1u);
USER_OS_RegisterTask("state",   USER_State_Task,      10u, 3u, 2u);
USER_OS_RegisterTask("mcm",     USER_MCM_Task,        10u, 4u, 3u);
USER_OS_RegisterTask("race",    USER_Race_Task,       10u, 5u, 4u);
USER_OS_RegisterTask("lidar",   USER_LiDAR_Task,      5u,  1u, 4u);
USER_OS_RegisterTask("ui",      USER_UI_Task,         5u,  2u, 6u);
```

LED 心跳由 `USER_UI_Task()` 内部分频处理，不再占用独立调度任务。
