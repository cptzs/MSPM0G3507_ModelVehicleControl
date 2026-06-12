#include "user_ui_internal.h"

#define USER_UI_ROUTE_CHARGE_TIME_MS 2000u
#define USER_UI_ROUTE_COUNTDOWN_MS 2000u

typedef struct
{
    bool charging_active;
    bool countdown_active;
    uint8_t pending_route;
    uint16_t countdown_remain_ms;
} USER_UI_RouteContext_t;

static USER_UI_RouteContext_t route_ctx = {
    false,
    false,
    RACE_ROUTE_NONE,
    0u,
};

void USER_UI_Route_Reset(void)
{
    route_ctx.charging_active = false;
    route_ctx.countdown_active = false;
    route_ctx.pending_route = RACE_ROUTE_NONE;
    route_ctx.countdown_remain_ms = 0u;
}

void USER_UI_Route_StartCharge(uint8_t route)
{
    route_ctx.charging_active = true;
    route_ctx.countdown_active = false;
    route_ctx.pending_route = route;
    route_ctx.countdown_remain_ms = 0u;
}

void USER_UI_Route_CancelCharge(void)
{
    route_ctx.charging_active = false;
    route_ctx.pending_route = RACE_ROUTE_NONE;
    route_ctx.countdown_remain_ms = 0u;
}

bool USER_UI_Route_IsBusy(void)
{
    return route_ctx.charging_active || route_ctx.countdown_active;
}

bool USER_UI_Route_IsCharging(void)
{
    return route_ctx.charging_active;
}

bool USER_UI_Route_IsCountdown(void)
{
    return route_ctx.countdown_active;
}

uint8_t USER_UI_Route_GetPendingRoute(void)
{
    return route_ctx.pending_route;
}

uint16_t USER_UI_Route_GetCountdownRemainMs(void)
{
    return route_ctx.countdown_remain_ms;
}

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
