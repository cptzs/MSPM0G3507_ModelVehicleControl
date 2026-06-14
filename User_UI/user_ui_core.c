#include "user_ui_internal.h"
#include "user_ui_unittest_actions.h"
#include "userlib_oled.h"

#define USER_UI_HEARTBEAT_PERIOD_TICKS 100u
#define USER_UI_HEARTBEAT_ON_TIME_MS 250u

static uint8_t ui_heartbeat_tick = 0u;

/**
 * @file user_ui_core.c
 * @brief 注册表驱动的 UI 主循环 — 唯一提供 USER_UI_Task() 的编译单元。
 *
 * 每 5ms 由协作式调度器调用，负责路线启动交互优先级处理、
 * PREV/NEXT 页面导航、按键事件消费与分发、静态重绘和动态刷新。
 */

/**
 * @brief 判断指定页面是否注册了按键处理回调。
 *
 * @param page 目标页面枚举值。
 * @return true  页面存在且 on_key 非空。
 * @return false 页面不存在或无按键处理。
 */
static bool USER_UI_Core_PageHasKeyHandler(DisplayPage_t page)
{
    const USER_UI_PageDef_t *page_def = USER_UI_FindPage(page);

    return (page_def != NULL) && (page_def->on_key != NULL);
}

/**
 * @brief 判断按键事件是否属于页面导航事件。
 *
 * @param event UI 按键事件类型。
 * @return true  SHORT 或 LONG_REPEAT（可用于 PREV/NEXT 导航）。
 * @return false NONE 或 LONG（不触发导航）。
 *
 * @note LONG 事件仅用于页面内部确认/长按操作，不触发翻页。
 */
static bool USER_UI_Core_IsNavigationEvent(USER_UI_KeyEvent_t event)
{
    return (event == USER_UI_KEY_EVENT_SHORT) ||
           (event == USER_UI_KEY_EVENT_LONG_REPEAT);
}

static bool USER_UI_Core_IsMenuConfirmEvent(USER_UI_KeyEvent_t event)
{
    return (event == USER_UI_KEY_EVENT_SHORT) ||
           (event == USER_UI_KEY_EVENT_LONG);
}

/**
 * @brief 消费指定按钮事件并分发到当前页面的 on_key() 回调。
 *
 * @param button 按钮枚举值。
 *
 * @note 若当前页面未注册 on_key 回调或无事件，则静默返回。
 */
static void USER_UI_Core_DispatchButtonToCurrentPage(Button_t button)
{
    DisplayPage_t current_page = USER_UI_Core_GetCurrentPage();
    USER_UI_KeyEvent_t event;

    event = USER_UI_ConsumeButtonEvent(button);
    if ((event != USER_UI_KEY_EVENT_NONE) &&
        USER_UI_Core_PageHasKeyHandler(current_page))
    {
        (void)USER_UI_DispatchPageKeyFromRegistry(current_page, button, event);
    }
}

/**
 * @brief 将全部 6 个物理按钮事件分发到当前页面。
 *
 * @note UP/DOWN/LEFT/RIGHT/ENTER/ESC 按固定顺序依次消费并分发，
 *       避免按键事件在页面间残留。
 */
static void USER_UI_Core_DispatchPageButtons(void)
{
    USER_UI_Core_DispatchButtonToCurrentPage(UP);
    USER_UI_Core_DispatchButtonToCurrentPage(DOWN);
    USER_UI_Core_DispatchButtonToCurrentPage(LEFT);
    USER_UI_Core_DispatchButtonToCurrentPage(RIGHT);
    USER_UI_Core_DispatchButtonToCurrentPage(ENTER);
}

static void USER_UI_Core_DispatchMenuButtons(void)
{
    USER_UI_ViewMode_t view_mode = USER_UI_Core_GetViewMode();
    USER_UI_KeyEvent_t up_event = USER_UI_ConsumeButtonEvent(UP);
    USER_UI_KeyEvent_t down_event = USER_UI_ConsumeButtonEvent(DOWN);
    USER_UI_KeyEvent_t prev_event = USER_UI_ConsumeButtonEvent(PREV);
    USER_UI_KeyEvent_t next_event = USER_UI_ConsumeButtonEvent(NEXT);
    USER_UI_KeyEvent_t enter_event = USER_UI_ConsumeButtonEvent(ENTER);
    USER_UI_KeyEvent_t esc_event = USER_UI_ConsumeButtonEvent(ESC);

    (void)USER_UI_ConsumeButtonEvent(LEFT);
    (void)USER_UI_ConsumeButtonEvent(RIGHT);

    if (view_mode == UI_VIEW_MAIN_MENU)
    {
        if (USER_UI_Core_IsNavigationEvent(up_event) || USER_UI_Core_IsNavigationEvent(prev_event))
        {
            USER_UI_Core_MoveMainSelection(false);
        }
        if (USER_UI_Core_IsNavigationEvent(down_event) || USER_UI_Core_IsNavigationEvent(next_event))
        {
            USER_UI_Core_MoveMainSelection(true);
        }
        if (USER_UI_Core_IsMenuConfirmEvent(enter_event))
        {
            USER_UI_Core_OpenSelectedCategory();
        }
    }
    else if (view_mode == UI_VIEW_SUB_MENU)
    {
        if (USER_UI_Core_IsNavigationEvent(up_event) || USER_UI_Core_IsNavigationEvent(prev_event))
        {
            USER_UI_Core_MoveSubSelection(false);
        }
        if (USER_UI_Core_IsNavigationEvent(down_event) || USER_UI_Core_IsNavigationEvent(next_event))
        {
            USER_UI_Core_MoveSubSelection(true);
        }
        if (USER_UI_Core_IsMenuConfirmEvent(enter_event))
        {
            USER_UI_Core_OpenSelectedItem();
        }
        if (esc_event != USER_UI_KEY_EVENT_NONE)
        {
            USER_UI_Core_Back();
        }
    }
    else if (view_mode == UI_VIEW_PLACEHOLDER)
    {
        if (esc_event != USER_UI_KEY_EVENT_NONE)
        {
            USER_UI_Core_Back();
        }
    }
}

/**
 * @brief 路线页面专用服务：仅在页面视图且当前页为 PAGE_TEMPLATE 时推进路线交互状态机。
 *
 * @note 每 5ms 调用一次，传入 ENTER 按键按压时间和状态以驱动充电→倒计时流程。
 */
static bool USER_UI_Core_ServiceRoutePage(void)
{
    bool was_busy;

    if ((USER_UI_Core_GetViewMode() != UI_VIEW_PAGE) ||
        (USER_UI_Core_GetCurrentPage() != PAGE_TEMPLATE))
    {
        return false;
    }

    was_busy = USER_UI_Route_IsBusy();
    USER_UI_Route_Service5ms((button_press_time[ENTER] > 0u), button_press_time[ENTER]);
    return was_busy || USER_UI_Route_IsBusy();
}

/**
 * @brief 主任务函数，负责处理用户界面逻辑。
 */
static void USER_UI_Core_DispatchRouteBusyButtons(void)
{
    USER_UI_KeyEvent_t esc_event = USER_UI_ConsumeButtonEvent(ESC);

    (void)USER_UI_ConsumeButtonEvent(UP);
    (void)USER_UI_ConsumeButtonEvent(DOWN);
    (void)USER_UI_ConsumeButtonEvent(LEFT);
    (void)USER_UI_ConsumeButtonEvent(RIGHT);
    (void)USER_UI_ConsumeButtonEvent(PREV);
    (void)USER_UI_ConsumeButtonEvent(NEXT);
    (void)USER_UI_ConsumeButtonEvent(ENTER);

    if (esc_event != USER_UI_KEY_EVENT_NONE)
    {
        (void)USER_UI_DispatchPageKeyFromRegistry(PAGE_TEMPLATE, ESC, esc_event);
    }
}

void USER_UI_Init(void)
{
    ui_heartbeat_tick = 0u;
    USER_UI_Route_Reset();
    USER_UI_UnitTestActions_Init();
    USER_UI_Core_Reset();
}

void USER_UI_Task(void)
{
    bool route_interaction_active;
    USER_UI_KeyEvent_t prev_event;
    USER_UI_KeyEvent_t next_event;
    USER_UI_KeyEvent_t esc_event;

    ui_heartbeat_tick++;
    if (ui_heartbeat_tick >= USER_UI_HEARTBEAT_PERIOD_TICKS)
    {
        ui_heartbeat_tick = 0u;
        USER_BoardIO_LED_On(LED0, USER_UI_HEARTBEAT_ON_TIME_MS);
    }

    route_interaction_active = USER_UI_Core_ServiceRoutePage();

    if (route_interaction_active)
    {
        USER_UI_Core_DispatchRouteBusyButtons();
        USER_UI_Core_RedrawStaticIfNeeded();
        USER_UI_Core_DrawCurrentDynamic();
        USER_OLED_Service();
        return;
    }

    if (USER_UI_Core_GetViewMode() != UI_VIEW_PAGE)
    {
        USER_UI_Core_DispatchMenuButtons();
        USER_UI_Core_RedrawStaticIfNeeded();
        USER_UI_Core_DrawCurrentDynamic();
        USER_OLED_Service();
        return;
    }

    prev_event = USER_UI_ConsumeButtonEvent(PREV);
    if (USER_UI_Core_IsNavigationEvent(prev_event))
    {
        USER_UI_Core_GotoAdjacentPage(false);
    }

    next_event = USER_UI_ConsumeButtonEvent(NEXT);
    if (USER_UI_Core_IsNavigationEvent(next_event))
    {
        USER_UI_Core_GotoAdjacentPage(true);
    }

    esc_event = USER_UI_ConsumeButtonEvent(ESC);
    if (esc_event != USER_UI_KEY_EVENT_NONE)
    {
        USER_UI_Core_Back();
        USER_UI_Core_RedrawStaticIfNeeded();
        USER_UI_Core_DrawCurrentDynamic();
        USER_OLED_Service();
        return;
    }

    USER_UI_Core_DispatchPageButtons();

    USER_UI_Core_RedrawStaticIfNeeded();
    USER_UI_Core_DrawCurrentDynamic();
    USER_OLED_Service();
}
