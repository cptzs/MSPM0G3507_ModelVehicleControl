/**
 * @file user_ui_input.c
 * @brief UI 按键输入适配层。
 *
 * 将底层 userlib_lbb 的计数型按钮事件转换为 UI 内部的 USER_UI_KeyEvent_t，
 * 并提供消费 + 分发的组合调用。
 */

#include "user_ui_internal.h"

/**
 * @brief 将底层按钮事件转换为 UI 按键事件。
 * @param event 底层事件类型。
 * @return 对应的 UI 内部事件类型。
 */
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

/**
 * @brief 消费指定按钮事件并转换为 UI 事件。
 * @details 调用底层消费接口读取并清除事件，再转换为 UI 内部类型。
 * @param button 按钮枚举值。
 * @return 对应的 UI 事件类型（NONE 表示无事件）。
 */
USER_UI_KeyEvent_t USER_UI_ConsumeButtonEvent(Button_t button)
{
    return USER_UI_ConvertButtonEvent(USER_BoardIO_Button_ConsumeEvent(button));
}

/**
 * @brief 消费按钮事件并分发到指定页面的 on_key() 回调。
 * @param page 目标页面枚举值。
 * @param button 按钮枚举值。
 * @retval true 事件被成功消费并分发。
 * @retval false 无事件或分发失败。
 */
bool USER_UI_ConsumeAndDispatchButton(DisplayPage_t page, Button_t button)
{
    USER_UI_KeyEvent_t event = USER_UI_ConsumeButtonEvent(button);

    if (event == USER_UI_KEY_EVENT_NONE)
    {
        return false;
    }

    return USER_UI_DispatchPageKeyFromRegistry(page, button, event);
}
