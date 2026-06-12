#include "userapp_race_table.h"

#include "userapp_mcm.h"

/**
 * @file userapp_race_table.c
 * @brief 表格驱动的比赛路线调度器实现。
 *
 * ======== 设计目标 ========
 * 路线动作表是一个静态数组，每一行描述一个"做什么"（直行、旋转、等待等），
 * 调度器负责逐行解释执行，每当遇到需要 MCM 执行的动作时：
 *   1. 调用 MCM 非阻塞启动接口（USER_MCM_Start*）发起运动；
 *   2. 进入 WAIT_ACTION_DONE 状态，等待 MCM 完成；
 *   3. MCM 完成后按 next_index 跳转到下一行。
 *
 * ======== 状态机概览 ========
 * @verbatim
 *   IDLE ──[Start]──> RUNNING ──[启动动作]──> WAIT_ACTION_DONE
 *                      ^                          │
 *                      │    ┌── 动作完成 ─────────┘
 *                      │    │   (跳转 next_index)
 *                      │    │
 *                      └────┘
 *
 *   WAIT_ACTION_DONE ──[超时/失败/非法]──> ERROR
 *   RUNNING           ──[遇到 END 行]──> DONE
 * @endverbatim
 *
 * ======== 设计边界 ========
 * 1. 不直接写电机 PWM。
 * 2. 不直接写左右轮速度 PID 目标。
 * 3. 不读取或清零编码器、IMU 等原始传感器数据。
 * 4. 只通过 USER_MCM_Start*() / USER_MCM_IsBusy() / USER_MCM_GetStatus() 消费 MCM。
 * 5. 调度器本身不包含定时器中断——由上层（userapp_race.c 的 USER_Race_Task）
 *    按固定周期（10 ms）调用 Update()，调度器据此累加经过时间。
 *
 * ======== 动作行字段说明 ========
 * - action_type  : 动作类型（MOVE_DISTANCE / ROTATE_ANGLE / WAIT_MS / STOP / END）
 * - param1       : 主参数（距离 mm / 角度 deg / 等待时间 ms）
 * - param2       : 次参数（最大速度 mm/s 或 最大角速度 deg/s）
 * - timeout_ms   : 本行动作超时时间；0 表示不启用表层超时
 * - next_index   : 成功后跳转的下一行动作索引；-1 表示路线结束
 * - flags        : 预留扩展标志
 */

/**
 * @brief 将执行器置为错误状态并停止当前 MCM 动作。
 * @param executor 执行器上下文。
 * @param result 路线层错误结果。
 */
static void USER_Race_TableExecutor_Fail(USER_Race_TableExecutor_t *executor,
                                         USER_Race_TableResult_t result)
{
    executor->state = USER_Race_TABLE_STATE_ERROR;
    executor->result = result;
    executor->action_started = false;
    USER_MCM_Cancel();
}

/**
 * @brief 启动当前动作行对应的 MCM 动作。
 * @param action 当前动作行。
 * @return true 表示动作已成功启动或不需要 MCM 启动。
 */
static bool USER_Race_StartAction(const USER_Race_Action_t *action)
{
    USER_MCM_Status_t status;

    if (action == (const USER_Race_Action_t *)0)
    {
        return false;
    }

    switch (action->action_type)
    {
    case USER_Race_ACTION_MOVE_DISTANCE:
        status = USER_MCM_StartStraight((int32_t)action->param1,
                                        (int32_t)action->param2);
        return status == USER_MCM_STATUS_OK;

    case USER_Race_ACTION_ROTATE_ANGLE:
        status = USER_MCM_StartSpin((int32_t)action->param1,
                                    (int32_t)action->param2);
        return status == USER_MCM_STATUS_OK;

    case USER_Race_ACTION_WAIT_MS:
        return true;

    case USER_Race_ACTION_STOP:
        USER_MCM_Cancel();
        return true;

    case USER_Race_ACTION_END:
        return true;

    default:
        return false;
    }
}

/**
 * @brief 判断当前动作是否完成。
 * @param executor 执行器上下文。
 * @param action 当前动作行。
 * @return true 表示动作完成。
 */
static bool USER_Race_IsActionDone(const USER_Race_TableExecutor_t *executor,
                                   const USER_Race_Action_t *action)
{
    if ((executor == (const USER_Race_TableExecutor_t *)0) ||
        (action == (const USER_Race_Action_t *)0))
    {
        return true;
    }

    switch (action->action_type)
    {
    case USER_Race_ACTION_MOVE_DISTANCE:
    case USER_Race_ACTION_ROTATE_ANGLE:
        return !USER_MCM_IsBusy();

    case USER_Race_ACTION_WAIT_MS:
        return executor->action_elapsed_ms >= (uint32_t)action->param1;

    case USER_Race_ACTION_STOP:
    case USER_Race_ACTION_END:
        return true;

    default:
        return true;
    }
}

/**
 * @brief 判断当前动作是否成功完成。
 * @param action 当前动作行。
 * @return true 表示动作成功。
 */
static bool USER_Race_IsActionSuccess(const USER_Race_Action_t *action)
{
    if (action == (const USER_Race_Action_t *)0)
    {
        return false;
    }

    switch (action->action_type)
    {
    case USER_Race_ACTION_MOVE_DISTANCE:
    case USER_Race_ACTION_ROTATE_ANGLE:
        return USER_MCM_GetStatus() == USER_MCM_STATUS_OK;

    case USER_Race_ACTION_WAIT_MS:
    case USER_Race_ACTION_STOP:
    case USER_Race_ACTION_END:
        return true;

    default:
        return false;
    }
}

/**
 * @brief 初始化路线表执行器。
 *
 * 将执行器重置为 IDLE 状态，清空所有运行时数据。
 * 必须在首次使用执行器前调用，也可在路线结束后调用以复位。
 *
 * @param executor 执行器上下文。
 * @param scheduler_period_ms 调度周期（调用 Update 的间隔），用于累加经过时间。
 */
void USER_Race_TableExecutor_Init(USER_Race_TableExecutor_t *executor,
                                  uint32_t scheduler_period_ms)
{
    if (executor == (USER_Race_TableExecutor_t *)0)
    {
        return;
    }

    executor->table = (const USER_Race_Action_t *)0;
    executor->table_size = 0u;
    executor->current_index = -1;
    executor->action_started = false;
    executor->state = USER_Race_TABLE_STATE_IDLE;
    executor->result = USER_Race_TABLE_RESULT_NONE;
    executor->action_elapsed_ms = 0u;
    executor->scheduler_period_ms = scheduler_period_ms;
}

/**
 * @brief 启动路线表执行。
 *
 * 将执行器从 IDLE 切换到 RUNNING，并指向动作表的第 0 行。
 * 若执行器正忙（IsBusy 返回 true），启动失败。
 *
 * @param executor 执行器上下文。
 * @param table 路线动作表（静态数组首地址）。
 * @param table_size 动作表行数。
 * @return true 启动成功，false 表示参数无效或执行器正忙。
 */
bool USER_Race_TableExecutor_Start(USER_Race_TableExecutor_t *executor,
                                   const USER_Race_Action_t *table,
                                   uint16_t table_size)
{
    if ((executor == (USER_Race_TableExecutor_t *)0) ||
        (table == (const USER_Race_Action_t *)0) ||
        (table_size == 0u))
    {
        return false;
    }

    if (USER_Race_TableExecutor_IsBusy(executor))
    {
        return false;
    }

    executor->table = table;
    executor->table_size = table_size;
    executor->current_index = 0;
    executor->action_started = false;
    executor->state = USER_Race_TABLE_STATE_RUNNING;
    executor->result = USER_Race_TABLE_RESULT_NONE;
    executor->action_elapsed_ms = 0u;
    return true;
}

/**
 * @brief 周期推进路线表执行器（核心状态机）。
 *
 * 该函数由上层（userapp_race.c）每 10 ms 调用一次，是调度器的核心。
 * 每次调用执行以下步骤：
 *
 *   [步骤1] 校验当前索引 —— 确保 action_index 在合法范围内
 *   [步骤2] 遇到 END 行 —— 整条路线正常结束，进入 DONE 状态
 *   [步骤3] 首次启动动作 —— 若 action_started == false，调用 MCM 非阻塞启动
 *   [步骤4] 超时检查 —— 累加经过时间，若超过 timeout_ms 则报错
 *   [步骤5] 等待完成 —— 若 MCM 未完成（或等待时间未到），直接返回
 *   [步骤6] 结果校验 —— MCM 完成后检查其状态，失败则进入 ERROR
 *   [步骤7] 跳转下一行 —— 按 next_index 跳转，-1 表示路线结束
 *
 * 关键：调用者通过 USER_Race_TableExecutor_IsBusy() 判断是否需要继续调用。
 *
 * @param executor 执行器上下文。
 */
void USER_Race_TableExecutor_Update(USER_Race_TableExecutor_t *executor)
{
    const USER_Race_Action_t *action;

    if (executor == (USER_Race_TableExecutor_t *)0)
    {
        return;
    }

    if ((executor->state != USER_Race_TABLE_STATE_RUNNING) &&
        (executor->state != USER_Race_TABLE_STATE_WAIT_ACTION_DONE))
    {
        return;
    }

    /* 步骤1：检查当前动作索引是否有效。 */
    if ((executor->current_index < 0) ||
        ((uint16_t)executor->current_index >= executor->table_size))
    {
        USER_Race_TableExecutor_Fail(executor, USER_Race_TABLE_RESULT_INVALID_INDEX);
        return;
    }

    action = &executor->table[executor->current_index];

    /* 步骤2：遇到 END 行时完成整条路线。 */
    if (action->action_type == USER_Race_ACTION_END)
    {
        executor->state = USER_Race_TABLE_STATE_DONE;
        executor->result = USER_Race_TABLE_RESULT_OK;
        executor->action_started = false;
        USER_MCM_Cancel();
        return;
    }

    /* 步骤3：如果当前动作尚未启动，则调用对应 MCM 启动接口。 */
    if (!executor->action_started)
    {
        if (!USER_Race_StartAction(action))
        {
            USER_Race_TableExecutor_Fail(executor, USER_Race_TABLE_RESULT_INVALID_ACTION);
            return;
        }

        executor->action_started = true;
        executor->action_elapsed_ms = 0u;
        executor->state = USER_Race_TABLE_STATE_WAIT_ACTION_DONE;
    }

    /* 步骤4：更新表层动作计时并检查表层超时。 */
    executor->action_elapsed_ms += executor->scheduler_period_ms;
    if ((action->timeout_ms > 0u) &&
        (executor->action_elapsed_ms > action->timeout_ms))
    {
        USER_Race_TableExecutor_Fail(executor, USER_Race_TABLE_RESULT_TIMEOUT);
        return;
    }

    /* 步骤5：动作未完成则继续等待。 */
    if (!USER_Race_IsActionDone(executor, action))
    {
        return;
    }

    /* 步骤6：动作完成但 MCM 报错时，路线进入错误状态。 */
    if (!USER_Race_IsActionSuccess(action))
    {
        USER_Race_TableExecutor_Fail(executor, USER_Race_TABLE_RESULT_ACTION_FAILED);
        return;
    }

    /* 步骤7：动作成功，跳转到下一行动作或结束路线。 */
    executor->action_started = false;
    executor->action_elapsed_ms = 0u;
    if (action->next_index < 0)
    {
        executor->state = USER_Race_TABLE_STATE_DONE;
        executor->result = USER_Race_TABLE_RESULT_OK;
        USER_MCM_Cancel();
        return;
    }

    executor->current_index = action->next_index;
    executor->state = USER_Race_TABLE_STATE_RUNNING;
}

/**
 * @brief 强制停止路线执行。
 *
 * 立即取消当前 MCM 动作，将执行器置回 IDLE 并标记 ABORTED。
 * 调用后可通过 Init() 或 Start() 重新开始。
 *
 * @param executor 执行器上下文。
 */
void USER_Race_TableExecutor_Stop(USER_Race_TableExecutor_t *executor)
{
    if (executor == (USER_Race_TableExecutor_t *)0)
    {
        return;
    }

    executor->state = USER_Race_TABLE_STATE_IDLE;
    executor->result = USER_Race_TABLE_RESULT_ABORTED;
    executor->current_index = -1;
    executor->action_started = false;
    executor->action_elapsed_ms = 0u;
    USER_MCM_Cancel();
}

/**
 * @brief 查询执行器是否正在运行。
 *
 * RUNNING 或 WAIT_ACTION_DONE 状态均视为"忙"。
 * 调用者应在 IsBusy 为 true 时持续调用 Update()。
 *
 * @param executor 执行器上下文。
 * @return true 表示路线正在执行中。
 */
bool USER_Race_TableExecutor_IsBusy(const USER_Race_TableExecutor_t *executor)
{
    if (executor == (const USER_Race_TableExecutor_t *)0)
    {
        return false;
    }

    return (executor->state == USER_Race_TABLE_STATE_RUNNING) ||
           (executor->state == USER_Race_TABLE_STATE_WAIT_ACTION_DONE);
}

/**
 * @brief 查询执行器是否已结束（成功或失败）。
 *
 * DONE 或 ERROR 状态均视为"已结束"。
 * 调用者可通过 GetResult() 区分成功还是出错。
 *
 * @param executor 执行器上下文。
 * @return true 表示路线已结束。
 */
bool USER_Race_TableExecutor_IsDone(const USER_Race_TableExecutor_t *executor)
{
    if (executor == (const USER_Race_TableExecutor_t *)0)
    {
        return false;
    }

    return (executor->state == USER_Race_TABLE_STATE_DONE) ||
           (executor->state == USER_Race_TABLE_STATE_ERROR);
}

/**
 * @brief 获取执行器当前状态。
 * @param executor 执行器上下文。
 * @return 当前状态枚举值；executor 为空时返回 ERROR。
 */
USER_Race_TableState_t USER_Race_TableExecutor_GetState(const USER_Race_TableExecutor_t *executor)
{
    if (executor == (const USER_Race_TableExecutor_t *)0)
    {
        return USER_Race_TABLE_STATE_ERROR;
    }

    return executor->state;
}

/**
 * @brief 获取执行结果。
 *
 * 仅在 IsDone() 返回 true 后结果有意义。OK 表示路线正常结束，
 * 其他值对应超时、动作失败、非法跳转等错误原因。
 *
 * @param executor 执行器上下文。
 * @return 执行结果枚举值。
 */
USER_Race_TableResult_t USER_Race_TableExecutor_GetResult(const USER_Race_TableExecutor_t *executor)
{
    if (executor == (const USER_Race_TableExecutor_t *)0)
    {
        return USER_Race_TABLE_RESULT_INVALID_ACTION;
    }

    return executor->result;
}

/**
 * @brief 获取当前执行到的动作行索引。
 *
 * 可用于调试或外部监控当前路线进度。
 *
 * @param executor 执行器上下文。
 * @return 当前动作行索引；executor 为空或未启动时返回 -1。
 */
int16_t USER_Race_TableExecutor_GetCurrentIndex(const USER_Race_TableExecutor_t *executor)
{
    if (executor == (const USER_Race_TableExecutor_t *)0)
    {
        return -1;
    }

    return executor->current_index;
}
