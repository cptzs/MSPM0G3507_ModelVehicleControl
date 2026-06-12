#include "user_ui_internal.h"

#define UI_ROUTE_MAP_X 0u
#define UI_ROUTE_MAP_Y 0u
#define UI_ROUTE_MAP_W 60u
#define UI_ROUTE_MAP_H 54u

#define UI_ROUTE_INFO_COL 10u
#define UI_ROUTE_BAR_X 70u
#define UI_ROUTE_BAR_Y 26u
#define UI_ROUTE_BAR_W 42u
#define UI_ROUTE_BAR_H 6u
#define UI_ROUTE_CHARGE_TIME_MS 2000u

static uint16_t USER_UI_RouteAbsI16(int16_t value)
{
    if (value < 0)
    {
        return (uint16_t)(-value);
    }

    return (uint16_t)value;
}

static void USER_UI_RouteDrawTemplatePath(void)
{
    const uint8_t left_x = 10u;
    const uint8_t right_x = 42u;
    const uint8_t top_y = 14u;
    const uint8_t bottom_y = 56u;
    const uint8_t center_x = 26u;
    const uint8_t radius = 16u;

    USER_OLED_DrawRect(UI_ROUTE_MAP_X,
                       UI_ROUTE_MAP_Y,
                       UI_ROUTE_MAP_X + UI_ROUTE_MAP_W - 1u,
                       UI_ROUTE_MAP_Y + UI_ROUTE_MAP_H - 1u,
                       false);

    USER_OLED_DrawVLine(left_x, top_y, bottom_y);
    USER_OLED_DrawVLine(right_x, top_y, bottom_y);

    USER_OLED_DrawArc(center_x, top_y, radius, 0, 180);
    USER_OLED_DrawArc(center_x, bottom_y, radius, 180, 360);

    USER_OLED_DrawLine(left_x, 28u, (uint8_t)(left_x - 3u), 32u);
    USER_OLED_DrawLine(left_x, 28u, (uint8_t)(left_x + 3u), 32u);

    USER_OLED_DrawLine(right_x, 42u, (uint8_t)(right_x - 3u), 38u);
    USER_OLED_DrawLine(right_x, 42u, (uint8_t)(right_x + 3u), 38u);
}

static void USER_UI_RouteDrawProgressBar(uint8_t percent)
{
    uint8_t fill_w;

    if (percent > 100u)
    {
        percent = 100u;
    }

    fill_w = (uint8_t)(((uint16_t)(UI_ROUTE_BAR_W - 2u) * percent) / 100u);

    USER_OLED_DrawRect(UI_ROUTE_BAR_X,
                       UI_ROUTE_BAR_Y,
                       UI_ROUTE_BAR_X + UI_ROUTE_BAR_W - 1u,
                       UI_ROUTE_BAR_Y + UI_ROUTE_BAR_H - 1u,
                       false);

    if (fill_w > 0u)
    {
        USER_OLED_DrawRect(UI_ROUTE_BAR_X + 1u,
                           UI_ROUTE_BAR_Y + 1u,
                           UI_ROUTE_BAR_X + fill_w,
                           UI_ROUTE_BAR_Y + UI_ROUTE_BAR_H - 2u,
                           true);
    }
}

static void USER_UI_RouteFormatStepText(const USER_Race_Action_t *action_ptr,
                                        int16_t step_index,
                                        char *out_buf,
                                        uint8_t out_len)
{
    uint16_t main_value = 0u;
    uint8_t step_no;

    if ((out_buf == NULL) || (out_len == 0u))
    {
        return;
    }

    if ((action_ptr == NULL) || (step_index < 0))
    {
        (void)snprintf(out_buf, out_len, "S-- IDLE");
        return;
    }

    step_no = (uint8_t)(step_index + 1u);

    switch (action_ptr->action_type)
    {
    case USER_Race_ACTION_MOVE_DISTANCE:
        main_value = (uint16_t)(USER_UI_RouteAbsI16((int16_t)action_ptr->param1) / 10u);
        if (action_ptr->param1 >= 0.0f)
        {
            (void)snprintf(out_buf, out_len, "S%02u FW %u", step_no, main_value);
        }
        else
        {
            (void)snprintf(out_buf, out_len, "S%02u BW %u", step_no, main_value);
        }
        break;

    case USER_Race_ACTION_ROTATE_ANGLE:
        main_value = USER_UI_RouteAbsI16((int16_t)action_ptr->param1);
        if (action_ptr->param1 >= 0.0f)
        {
            (void)snprintf(out_buf, out_len, "S%02u TR %u", step_no, main_value);
        }
        else
        {
            (void)snprintf(out_buf, out_len, "S%02u TL %u", step_no, main_value);
        }
        break;

    case USER_Race_ACTION_WAIT_MS:
        main_value = (uint16_t)action_ptr->param1;
        (void)snprintf(out_buf, out_len, "S%02u WT %u", step_no, main_value);
        break;

    case USER_Race_ACTION_STOP:
        (void)snprintf(out_buf, out_len, "S%02u STP", step_no);
        break;

    case USER_Race_ACTION_END:
        (void)snprintf(out_buf, out_len, "S%02u END", step_no);
        break;

    default:
        (void)snprintf(out_buf, out_len, "S%02u N/A", step_no);
        break;
    }
}

static bool USER_UI_RouteGetDisplayAction(USER_Race_Action_t *action_ptr,
                                          int16_t *step_index_ptr,
                                          uint32_t *timeout_remain_ptr)
{
    bool has_action;

    has_action = USER_Race_GetCurrentAction(action_ptr, step_index_ptr, timeout_remain_ptr);
    if (has_action)
    {
        return true;
    }

    has_action = USER_Race_GetTemplatePreviewAction(action_ptr, timeout_remain_ptr);
    if (has_action)
    {
        if (step_index_ptr != NULL)
        {
            *step_index_ptr = 0;
        }
        return true;
    }

    return false;
}

void USER_UI_ShowRouteStatic(void)
{
    USER_UI_RouteDrawTemplatePath();

    USER_OLED_putString(2u, UI_ROUTE_INFO_COL, "R00     ", 11u);
    USER_OLED_putString(3u, UI_ROUTE_INFO_COL, "HOLD", 4u);
    USER_UI_RouteDrawProgressBar(0u);
    USER_OLED_putString(5u, UI_ROUTE_INFO_COL, "S-- IDLE", 8u);
    USER_OLED_putString(6u, UI_ROUTE_INFO_COL, "TMO ----", 8u);
}

void USER_UI_ShowRouteDynamic(void)
{
    static bool buzzer_triggered = false;
    uint16_t press_time = button_press_time[ENTER];
    uint8_t progress = 0u;
    USER_Race_Action_t action;
    int16_t step_index = -1;
    uint32_t timeout_remain_ms = 0u;
    char step_text[13];
    bool has_action = false;

    if (!USER_UI_Route_IsBusy())
    {
        if (press_time > 0u)
        {
            USER_UI_Route_StartCharge(RACE_ROUTE_TEMPLATE);
            USER_OLED_CleanScreen();
            USER_UI_Core_MarkStaticDirty();
            return;
        }
    }

    if (USER_UI_Route_IsCharging())
    {
        if (press_time >= UI_ROUTE_CHARGE_TIME_MS)
        {
            progress = 100u;
        }
        else
        {
            progress = (uint8_t)(((uint32_t)press_time * 100u) / UI_ROUTE_CHARGE_TIME_MS);
        }
    }
    else if (USER_UI_Route_IsCountdown())
    {
        progress = 100u;
    }
    else
    {
        progress = 0u;
    }

    USER_OLED_putString(2u, UI_ROUTE_INFO_COL, "           ", 11u);
    USER_OLED_putString(2u, UI_ROUTE_INFO_COL, "R00 ", 4u);
    USER_OLED_putString(2u,
                        UI_ROUTE_INFO_COL + 4u,
                        USER_Race_GetRouteName(RACE_ROUTE_TEMPLATE),
                        7u);

    USER_OLED_putString(3u, UI_ROUTE_INFO_COL, "    ", 4u);
    USER_OLED_putString(3u, UI_ROUTE_INFO_COL, "HOLD", 4u);

    USER_UI_RouteDrawProgressBar(progress);

    if (USER_UI_Route_IsCountdown())
    {
        USER_OLED_putString(5u, UI_ROUTE_INFO_COL, "             ", 13u);
        USER_OLED_putString(5u, UI_ROUTE_INFO_COL, "S-- START", 9u);
    }
    else
    {
        has_action = USER_UI_RouteGetDisplayAction(&action, &step_index, &timeout_remain_ms);
        if (has_action)
        {
            USER_UI_RouteFormatStepText(&action, step_index, step_text, sizeof(step_text));
        }
        else
        {
            USER_UI_RouteFormatStepText(NULL, -1, step_text, sizeof(step_text));
        }

        USER_OLED_putString(5u, UI_ROUTE_INFO_COL, "             ", 13u);
        USER_OLED_putString(5u, UI_ROUTE_INFO_COL, step_text, 12u);
    }

    USER_OLED_putString(6u, UI_ROUTE_INFO_COL, "TMO ", 4u);
    if (USER_UI_Route_IsCountdown())
    {
        USER_OLED_putUI16(6u, UI_ROUTE_INFO_COL + 4u, USER_UI_Route_GetCountdownRemainMs(), 4u);
    }
    else if (has_action)
    {
        if (timeout_remain_ms > 9999u)
        {
            timeout_remain_ms = 9999u;
        }
        USER_OLED_putUI16(6u, UI_ROUTE_INFO_COL + 4u, (uint16_t)timeout_remain_ms, 4u);
    }
    else
    {
        USER_OLED_putString(6u, UI_ROUTE_INFO_COL + 4u, "----", 4u);
    }

    if (USER_UI_Route_IsCountdown() && !buzzer_triggered)
    {
        USER_LBB_Buzzer_On(100u);
        buzzer_triggered = true;
    }
    else if (!USER_UI_Route_IsCountdown())
    {
        buzzer_triggered = false;
    }
}

void USER_UI_RouteOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    if ((key == ESC) && (event == USER_UI_KEY_EVENT_SHORT))
    {
        USER_UI_Route_CancelCharge();
        USER_OLED_CleanScreen();
        USER_UI_Core_MarkStaticDirty();
    }
}
