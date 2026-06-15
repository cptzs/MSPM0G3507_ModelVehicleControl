#include "userapp_race.h"

/**
 * @file userapp_race.c
 * @brief 比赛路线启动与动作表调度。
 *
 * 本模块只处理路线级流程：接收启动请求、启动动作表执行器、周期推进执行器。
 * 它不直接操作 OLED、LED、蜂鸣器或任何传感器/执行器。
 * 倒计时、显示、按键交互均由 user_ui 模块负责。
 */

#define USER_Race_TASK_PERIOD_MS 10u
#define USER_Race_TABLE_COUNT(table) ((uint16_t)(sizeof(table) / sizeof((table)[0])))

/**
 * @brief 模板路线动作表：直行 50 cm，掉头，返回 50 cm，再旋转回原方向。
 */
static const USER_Race_Action_t race_template_actions[] = {
    {USER_Race_ACTION_MOVE_DISTANCE, 500.0f, 300.0f, 3000u, 1, 0u},
    {USER_Race_ACTION_ROTATE_ANGLE, 180.0f, 90.0f, 6500u, 2, 0u},
    {USER_Race_ACTION_MOVE_DISTANCE, 500.0f, 300.0f, 3000u, 3, 0u},
    {USER_Race_ACTION_ROTATE_ANGLE, 180.0f, 90.0f, 6500u, 4, 0u},
    {USER_Race_ACTION_END, 0.0f, 0.0f, 0u, -1, 0u},
};

/**
 * @brief 圆弧测试路线：左前方 90°/R500，逆时针自旋 90°，前进 50 cm，顺时针自旋 180°。
 */
static const USER_Race_Action_t race_arc_test_actions[] = {
    {USER_Race_ACTION_ARC, 500.0f, 90.0f, 10000u, 1, 0u},
    {USER_Race_ACTION_ROTATE_ANGLE, 90.0f, 90.0f, 5000u, 2, 0u},
    {USER_Race_ACTION_MOVE_DISTANCE, 500.0f, 300.0f, 3000u, 3, 0u},
    {USER_Race_ACTION_ROTATE_ANGLE, -180.0f, 90.0f, 8000u, 4, 0u},
    {USER_Race_ACTION_END, 0.0f, 0.0f, 0u, -1, 0u},
};

/**
 * @brief 当前路线动作表执行器。
 */
static USER_Race_TableExecutor_t race_executor;

/**
 * @brief 路线动作表执行器是否已经初始化。
 */
static bool race_executor_initialized = false;

/**
 * @brief 当前等待启动的路线编号。
 *
 * `RACE_ROUTE_NONE` 表示当前没有新的启动请求。
 */
static uint8_t requested_route = RACE_ROUTE_NONE;

/**
 * @brief 确保路线动作表执行器完成一次性初始化。
 */
static void USER_Race_EnsureExecutorInit(void)
{
    if (!race_executor_initialized)
    {
        USER_Race_TableExecutor_Init(&race_executor, USER_Race_TASK_PERIOD_MS);
        race_executor_initialized = true;
    }
}

/**
 * @brief 获取路线显示名称。
 *
 * @param race_route `RaceRoute_t` 路线编号。
 *
 * @return 路线名称字符串；未知路线返回 `Unknown`。
 */
const char *USER_Race_GetRouteName(uint8_t race_route)
{
    switch (race_route)
    {
    case RACE_ROUTE_TEMPLATE:
        return "Template";

    case RACE_ROUTE_ARC_TEST:
        return "ArcTest";

    default:
        return "Unknown";
    }
}

/**
 * @brief 获取当前正在执行的路线动作信息。
 *
 * @param action_ptr 输出当前动作内容；允许传入 NULL。
 * @param step_index_ptr 输出当前步骤索引，从 0 开始；允许传入 NULL。
 * @param remain_timeout_ms_ptr 输出当前动作表层超时剩余时间，单位 ms；允许传入 NULL。
 *
 * @return true 表示当前存在有效动作；false 表示路线未运行、已结束或当前索引无效。
 */
bool USER_Race_GetCurrentAction(USER_Race_Action_t *action_ptr,
                                int16_t *step_index_ptr,
                                uint32_t *remain_timeout_ms_ptr)
{
    int16_t current_index;
    const USER_Race_Action_t *current_action;

    /* 步骤1：确保执行器初始化，避免 UI 在 Race_Task() 前读取未初始化上下文。 */
    USER_Race_EnsureExecutorInit();

    /* 步骤2：路线没有处于运行或等待动作完成状态时，不返回当前动作。 */
    if ((race_executor.state != USER_Race_TABLE_STATE_RUNNING) &&
        (race_executor.state != USER_Race_TABLE_STATE_WAIT_ACTION_DONE))
    {
        return false;
    }

    /* 步骤3：检查当前动作索引是否在表范围内。 */
    current_index = race_executor.current_index;
    if ((current_index < 0) || ((uint16_t)current_index >= race_executor.table_size) ||
        (race_executor.table == NULL))
    {
        return false;
    }

    current_action = &race_executor.table[current_index];

    /* 步骤4：根据调用者需要拷贝动作、步骤号和剩余超时时间。 */
    if (action_ptr != NULL)
    {
        *action_ptr = *current_action;
    }

    if (step_index_ptr != NULL)
    {
        *step_index_ptr = current_index;
    }

    if (remain_timeout_ms_ptr != NULL)
    {
        if ((current_action->timeout_ms == 0u) ||
            (race_executor.action_elapsed_ms >= current_action->timeout_ms))
        {
            *remain_timeout_ms_ptr = 0u;
        }
        else
        {
            *remain_timeout_ms_ptr = current_action->timeout_ms - race_executor.action_elapsed_ms;
        }
    }

    return true;
}

/**
 * @brief 获取模板路线第一步动作，供路线页面在未启动时显示预览。
 *
 * @param action_ptr 输出第一步动作内容；允许传入 NULL。
 * @param remain_timeout_ms_ptr 输出第一步超时时间，单位 ms；允许传入 NULL。
 *
 * @return true 表示预览动作有效。
 */
bool USER_Race_GetTemplatePreviewAction(USER_Race_Action_t *action_ptr,
                                        uint32_t *remain_timeout_ms_ptr)
{
    return USER_Race_GetRoutePreviewAction(RACE_ROUTE_TEMPLATE,
                                           action_ptr,
                                           remain_timeout_ms_ptr);
}

bool USER_Race_GetRoutePreviewAction(uint8_t race_route,
                                     USER_Race_Action_t *action_ptr,
                                     uint32_t *remain_timeout_ms_ptr)
{
    const USER_Race_Action_t *preview_action;

    switch (race_route)
    {
    case RACE_ROUTE_ARC_TEST:
        preview_action = &race_arc_test_actions[0];
        break;

    case RACE_ROUTE_TEMPLATE:
    default:
        preview_action = &race_template_actions[0];
        break;
    }

    if (action_ptr != NULL)
    {
        *action_ptr = *preview_action;
    }

    if (remain_timeout_ms_ptr != NULL)
    {
        *remain_timeout_ms_ptr = preview_action->timeout_ms;
    }

    return true;
}

/**
 * @brief 请求启动指定路线。
 *
 * 显示层或后续通信层只通过该接口提交路线启动请求，不直接改写 race 模块内部状态。
 *
 * @param race_route 路线编号。
 */
void USER_Race_RequestStart(uint8_t race_route)
{
    USER_Race_EnsureExecutorInit();
    requested_route = race_route;
}

/**
 * @brief 比赛路线周期任务。
 *
 * 该函数由主循环每 10 ms 调用一次。收到启动请求后立即启动动作表，
 * 并在后续周期持续推进动作表执行器，直到路线结束或出错。
 *
 * 倒计时和 UI 提示由 user_ui 模块在调用 USER_Race_RequestStart() 之前完成，
 * 本模块不再处理任何与显示或提示相关的逻辑。
 */
void USER_Race_Task(void)
{
    USER_Race_EnsureExecutorInit();

    /* 步骤1：如果动作表正在运行，持续推进路线执行器。 */
    if (USER_Race_TableExecutor_IsBusy(&race_executor))
    {
        USER_Race_TableExecutor_Update(&race_executor);
        return;
    }

    /* 步骤2：执行器刚完成或报错时，结束本次路线并复位执行器。 */
    if (USER_Race_TableExecutor_IsDone(&race_executor))
    {
        USER_Race_TableExecutor_Init(&race_executor, USER_Race_TASK_PERIOD_MS);
    }

    /* 步骤3：有待启动路线时立即启动，不做倒计时。 */
    if (requested_route != RACE_ROUTE_NONE)
    {
        uint8_t route = requested_route;
        requested_route = RACE_ROUTE_NONE;
        (void)USER_Race_RunSelected(route);
    }
}

/**
 * @brief 启动模板路线动作表。
 * @return true 表示没有启动成功或路线已经结束，false 表示动作表已经启动并正在运行。
 */
bool USER_Race_TemplatePath(void)
{
    USER_Race_EnsureExecutorInit();
    if (USER_Race_TableExecutor_IsBusy(&race_executor))
    {
        return false;
    }

    return !USER_Race_TableExecutor_Start(&race_executor,
                                          race_template_actions,
                                          USER_Race_TABLE_COUNT(race_template_actions));
}

bool USER_Race_ArcTestPath(void)
{
    USER_Race_EnsureExecutorInit();
    if (USER_Race_TableExecutor_IsBusy(&race_executor))
    {
        return false;
    }

    return !USER_Race_TableExecutor_Start(&race_executor,
                                          race_arc_test_actions,
                                          USER_Race_TABLE_COUNT(race_arc_test_actions));
}

/**
 * @brief 根据路线编号启动对应路线。
 * @param race_route `RaceRoute_t` 路线编号。
 * @return true 表示路线未运行或启动失败，false 表示路线已经启动。
 */
bool USER_Race_RunSelected(uint8_t race_route)
{
    switch (race_route)
    {
    case RACE_ROUTE_TEMPLATE:
        return USER_Race_TemplatePath();

    case RACE_ROUTE_ARC_TEST:
        return USER_Race_ArcTestPath();

    default:
        USER_MCM_FreezeAllMotions();
        return true;
    }
}
