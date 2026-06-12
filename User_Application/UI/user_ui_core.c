#include "user_ui_internal.h"

/**
 * @file user_ui_core.c
 * @brief Registry-driven UI task implementation.
 *
 * This file is the target replacement for the legacy USER_UI_Task() still kept in
 * User_Application/user_ui.c.  When switching the IAR project, compile this file
 * together with user_ui_legacy_pages.c instead of compiling ../user_ui.c directly.
 * The helper script tools/migrate_iar_ui_core.py performs that deterministic
 * project-file switch for basic.ewp.
 */

static bool USER_UI_Core_PageHasKeyHandler(DisplayPage_t page)
{
    const USER_UI_PageDef_t *page_def = USER_UI_FindPage(page);

    return (page_def != NULL) && (page_def->on_key != NULL);
}

static bool USER_UI_Core_IsNavigationEvent(USER_UI_KeyEvent_t event)
{
    return (event == USER_UI_KEY_EVENT_SHORT) ||
           (event == USER_UI_KEY_EVENT_LONG_REPEAT);
}

static void USER_UI_Core_DispatchButtonToCurrentPage(Button_t button)
{
    DisplayPage_t current_page = USER_UI_Core_GetCurrentPage();
    USER_UI_KeyEvent_t event;

    if (!USER_UI_Core_PageHasKeyHandler(current_page))
    {
        return;
    }

    event = USER_UI_ConsumeButtonEvent(button);
    if (event != USER_UI_KEY_EVENT_NONE)
    {
        (void)USER_UI_DispatchPageKeyFromRegistry(current_page, button, event);
    }
}

static void USER_UI_Core_DispatchPageButtons(void)
{
    USER_UI_Core_DispatchButtonToCurrentPage(UP);
    USER_UI_Core_DispatchButtonToCurrentPage(DOWN);
    USER_UI_Core_DispatchButtonToCurrentPage(LEFT);
    USER_UI_Core_DispatchButtonToCurrentPage(RIGHT);
    USER_UI_Core_DispatchButtonToCurrentPage(ENTER);
    USER_UI_Core_DispatchButtonToCurrentPage(ESC);
}

static void USER_UI_Core_ServiceRoutePage(void)
{
    if (USER_UI_Core_GetCurrentPage() != PAGE_TEMPLATE)
    {
        return;
    }

    USER_UI_Route_Service5ms((button_press_time[ENTER] > 0u), button_press_time[ENTER]);
}

void USER_UI_Task(void)
{
    USER_UI_KeyEvent_t prev_event;
    USER_UI_KeyEvent_t next_event;

    USER_UI_Core_ServiceRoutePage();

    if (USER_UI_Route_IsBusy())
    {
        USER_UI_Core_RedrawStaticIfNeeded();
        USER_UI_Core_DrawCurrentDynamic();
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

    USER_UI_Core_DispatchPageButtons();

    USER_UI_Core_RedrawStaticIfNeeded();
    USER_UI_Core_DrawCurrentDynamic();
}
