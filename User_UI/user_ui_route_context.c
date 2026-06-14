#include "user_ui_internal.h"

#include "userlib_oled.h"
#include "userapp_race.h"

#define USER_UI_ROUTE_CHARGE_TIME_MS 2000u
#define USER_UI_ROUTE_COUNTDOWN_MS 1000u

typedef struct
{
    bool charging_active;
    bool confirmed_wait_release;
    bool countdown_active;
    bool wait_for_enter_release;
    uint8_t pending_route;
    uint16_t countdown_remain_ms;
} USER_UI_RouteContext_t;

static USER_UI_RouteContext_t route_ctx = {
    false,
    false,
    false,
    false,
    RACE_ROUTE_NONE,
    0u,
};

void USER_UI_Route_Reset(void)
{
    route_ctx.charging_active = false;
    route_ctx.confirmed_wait_release = false;
    route_ctx.countdown_active = false;
    route_ctx.wait_for_enter_release = false;
    route_ctx.pending_route = RACE_ROUTE_NONE;
    route_ctx.countdown_remain_ms = 0u;
}

void USER_UI_Route_StartCharge(uint8_t route)
{
    route_ctx.charging_active = true;
    route_ctx.confirmed_wait_release = false;
    route_ctx.countdown_active = false;
    route_ctx.wait_for_enter_release = false;
    route_ctx.pending_route = route;
    route_ctx.countdown_remain_ms = 0u;
}

void USER_UI_Route_CancelCharge(void)
{
    route_ctx.charging_active = false;
    route_ctx.confirmed_wait_release = false;
    route_ctx.countdown_active = false;
    route_ctx.wait_for_enter_release = true;
    route_ctx.pending_route = RACE_ROUTE_NONE;
    route_ctx.countdown_remain_ms = 0u;
}

bool USER_UI_Route_IsBusy(void)
{
    return route_ctx.charging_active ||
           route_ctx.confirmed_wait_release ||
           route_ctx.countdown_active;
}

bool USER_UI_Route_IsCharging(void)
{
    return route_ctx.charging_active;
}

bool USER_UI_Route_IsWaitingRelease(void)
{
    return route_ctx.confirmed_wait_release;
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
    if (route_ctx.wait_for_enter_release)
    {
        if (!enter_is_pressed)
        {
            route_ctx.wait_for_enter_release = false;
        }
        return;
    }

    if (!USER_UI_Route_IsBusy() && enter_is_pressed)
    {
        USER_UI_Route_StartCharge(RACE_ROUTE_TEMPLATE);
        USER_OLED_CleanScreen();
        USER_UI_Core_MarkStaticDirty();
    }

    if (route_ctx.charging_active)
    {
        if (!enter_is_pressed)
        {
            USER_UI_Route_CancelCharge();
            USER_OLED_CleanScreen();
            USER_UI_Core_MarkStaticDirty();
            return;
        }

        if (enter_press_time_ms >= USER_UI_ROUTE_CHARGE_TIME_MS)
        {
            route_ctx.charging_active = false;
            route_ctx.confirmed_wait_release = true;
            route_ctx.countdown_remain_ms = 0u;
        }
    }

    if (route_ctx.confirmed_wait_release)
    {
        if (!enter_is_pressed)
        {
            route_ctx.confirmed_wait_release = false;
            route_ctx.countdown_active = true;
            route_ctx.countdown_remain_ms = USER_UI_ROUTE_COUNTDOWN_MS;
        }
        return;
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
            route_ctx.charging_active = false;
            route_ctx.confirmed_wait_release = false;
            route_ctx.countdown_active = false;
            route_ctx.wait_for_enter_release = enter_is_pressed;
            route_ctx.pending_route = RACE_ROUTE_NONE;
            USER_OLED_CleanScreen();
            USER_UI_Core_MarkStaticDirty();
        }
    }
}
