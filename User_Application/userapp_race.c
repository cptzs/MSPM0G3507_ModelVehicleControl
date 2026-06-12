#include "userapp_race.h"

#include "userapp_race_table.h"

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
    {USER_Race_ACTION_ROTATE_ANGLE, 180.0f, 90.0f, 2500u, 2, 0u},
    {USER_Race_ACTION_MOVE_DISTANCE, 500.0f, 300.0f, 3000u, 3, 0u},
    {USER_Race_ACTION_ROTATE_ANGLE, 180.0f, 90.0f, 2500u, 4, 0u},
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

    default:
        USER_MCM_FreezeAllMotions();
        return true;
    }
}
