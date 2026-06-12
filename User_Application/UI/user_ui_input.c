#include "user_ui_internal.h"

USER_UI_KeyEvent_t USER_UI_ConvertButtonEvent(USER_LBB_ButtonEvent_t event)
{
    switch (event)
    {
    case USER_LBB_BUTTON_EVENT_SHORT:
        return USER_UI_KEY_EVENT_SHORT;
    case USER_LBB_BUTTON_EVENT_LONG:
        return USER_UI_KEY_EVENT_LONG;
    case USER_LBB_BUTTON_EVENT_LONG_REPEAT:
        return USER_UI_KEY_EVENT_LONG_REPEAT;
    case USER_LBB_BUTTON_EVENT_NONE:
    default:
        return USER_UI_KEY_EVENT_NONE;
    }
}

USER_UI_KeyEvent_t USER_UI_ConsumeButtonEvent(Button_t button)
{
    return USER_UI_ConvertButtonEvent(USER_LBB_Button_ConsumeEvent(button));
}

bool USER_UI_ConsumeAndDispatchButton(DisplayPage_t page, Button_t button)
{
    USER_UI_KeyEvent_t event = USER_UI_ConsumeButtonEvent(button);

    if (event == USER_UI_KEY_EVENT_NONE)
    {
        return false;
    }

    return USER_UI_DispatchPageKeyFromRegistry(page, button, event);
}
