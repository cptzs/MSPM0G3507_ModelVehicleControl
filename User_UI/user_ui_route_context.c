/**
 * @file user_ui_route_context.c
 * @brief 路线启动交互状态机（充电 → 倒计时 → 启动）。
 *
 * 在路线页面 (PAGE_TEMPLATE) 内管理 ENTER 长按充电、倒计时和
 * USER_Race_RequestStart() 调用的完整流程。
 *
 * 状态机：
 *   IDLE → (ENTER 按下) → CHARGING → (2s 充满) → COUNTDOWN → (2s 结束) → 启动比赛
 *   任意时刻松开 ENTER 或按 ESC 均可取消回到 IDLE。
 */

#include "user_ui_internal.h"

/** @brief ENTER 长按充电阈值 (ms) */
#define USER_UI_ROUTE_CHARGE_TIME_MS 2000u
/** @brief 启动前倒计时时长 (ms) */
#define USER_UI_ROUTE_COUNTDOWN_MS 2000u

/** @brief 路线交互上下文 */
typedef struct
{
    bool charging_active;         /**< 充电阶段激活 */
    bool countdown_active;        /**< 倒计时阶段激活 */
    uint8_t pending_route;        /**< 待启动的路线编号 */
    uint16_t countdown_remain_ms; /**< 倒计时剩余时间 (ms) */
} USER_UI_RouteContext_t;

/** @brief 全局路线交互上下文（单实例） */
static USER_UI_RouteContext_t route_ctx = {
    false,
    false,
    RACE_ROUTE_NONE,
    0u,
};

/**
 * @brief 重置路线交互状态机到 IDLE。
 */
void USER_UI_Route_Reset(void)
{
    route_ctx.charging_active = false;
    route_ctx.countdown_active = false;
    route_ctx.pending_route = RACE_ROUTE_NONE;
    route_ctx.countdown_remain_ms = 0u;
}

/**
 * @brief 开始充电阶段（ENTER 按下时调用）。
 *
 * @param route 目标路线编号。
 */
void USER_UI_Route_StartCharge(uint8_t route)
{
    route_ctx.charging_active = true;
    route_ctx.countdown_active = false;
    route_ctx.pending_route = route;
    route_ctx.countdown_remain_ms = 0u;
}

/**
 * @brief 取消充电/倒计时，回到 IDLE 状态。
 */
void USER_UI_Route_CancelCharge(void)
{
    route_ctx.charging_active = false;
    route_ctx.pending_route = RACE_ROUTE_NONE;
    route_ctx.countdown_remain_ms = 0u;
}

/**
 * @brief 查询路线交互是否忙（充电或倒计时中）。
 *
 * @return true  忙，core 层将跳过正常按键分发。
 * @return false IDLE 状态。
 */
bool USER_UI_Route_IsBusy(void)
{
    return route_ctx.charging_active || route_ctx.countdown_active;
}

/**
 * @brief 查询是否处于充电阶段。
 */
bool USER_UI_Route_IsCharging(void)
{
    return route_ctx.charging_active;
}

/**
 * @brief 查询是否处于倒计时阶段。
 */
bool USER_UI_Route_IsCountdown(void)
{
    return route_ctx.countdown_active;
}

/**
 * @brief 获取当前待启动的路线编号。
 */
uint8_t USER_UI_Route_GetPendingRoute(void)
{
    return route_ctx.pending_route;
}

/**
 * @brief 获取倒计时剩余时间。
 *
 * @return 剩余毫秒数。
 */
uint16_t USER_UI_Route_GetCountdownRemainMs(void)
{
    return route_ctx.countdown_remain_ms;
}

/**
 * @brief 路线交互状态机 5ms 周期服务。
 *
 * @param enter_is_pressed   ENTER 按键当前是否按下。
 * @param enter_press_time_ms ENTER 按键持续按压时间 (ms)。
 *
 * @note 充电阶段：松开 ENTER 则取消；按压 ≥ 2s 则进入倒计时。
 *       倒计时阶段：每 5ms 递减，归零时调用 USER_Race_RequestStart() 启动比赛。
 */
void USER_UI_Route_Service5ms(bool enter_is_pressed, uint16_t enter_press_time_ms)
{
    if (route_ctx.charging_active)
    {
        if (!enter_is_pressed)
        {
            USER_UI_Route_CancelCharge();
            USER_OLED_CleanScreen();
            USER_UI_Core_MarkStaticDirty();
        }
        else if (enter_press_time_ms >= USER_UI_ROUTE_CHARGE_TIME_MS)
        {
            route_ctx.charging_active = false;
            route_ctx.countdown_active = true;
            route_ctx.countdown_remain_ms = USER_UI_ROUTE_COUNTDOWN_MS;
        }
    }

    if (route_ctx.countdown_active)
    {
        if (route_ctx.countdown_remain_ms >= 5u)
        {
            route_ctx.countdown_remain_ms -= 5u;
        }
        else
        {
            route_ctx.countdown_remain_ms = 0u;
        }

        if (route_ctx.countdown_remain_ms == 0u)
        {
            USER_Race_RequestStart(route_ctx.pending_route);
            USER_UI_Route_Reset();
            USER_OLED_CleanScreen();
            USER_UI_Core_MarkStaticDirty();
        }
    }
}
