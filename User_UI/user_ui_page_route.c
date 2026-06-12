/**
 * @file user_ui_page_route.c
 * @brief 路线页面 (Template Path) — 赛道图形绘制 + 路线动作预览 + 启动交互。
 *
 * 静态内容绘制标准赛道模板图形（双竖线 + 两端半圆 + 方向箭头）。
 * 动态内容刷新充电进度条、当前动作步骤、超时剩余时间。
 * ENTER 长按启动充电→倒计时→比赛流程；ESC 短按取消。
 */

#include "user_ui_internal.h"

/** @brief 赛道图形绘制区域左上角 X */
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

/**
 * @brief 返回 int16_t 的绝对值（uint16_t）。
 */
static uint16_t USER_UI_RouteAbsI16(int16_t value)
{
    if (value < 0)
    {
        return (uint16_t)(-value);
    }

    return (uint16_t)value;
}

/**
 * @brief 绘制标准赛道模板图形（双竖线 + 两端半圆 + 方向箭头 + 外框）。
 */
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

/**
 * @brief 绘制路线启动充电进度条。
 *
 * @param percent 进度百分比（0~100）。
 */
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

/**
 * @brief 格式化路线动作步骤文本（S01 FW 100 / S02 TR 90 等）。
 *
 * @param action_ptr 指向当前动作的指针，NULL 表示 IDLE。
 * @param step_index 步骤索引（-1 表示无动作）。
 * @param out_buf    输出缓冲区。
 * @param out_len    输出缓冲区长度。
 */
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

/**
 * @brief 获取当前应显示的动作信息（优先比赛动作，其次模板预览）。
 *
 * @param[out] action_ptr        当前动作描述。
 * @param[out] step_index_ptr    步骤索引。
 * @param[out] timeout_remain_ptr 超时剩余时间 (ms)。
 * @return true  有可显示的动作。
 * @return false 无动作（IDLE 状态）。
 */
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

/**
 * @brief 绘制路线页面静态内容（赛道图形 + 信息标签）。
 */
void USER_UI_ShowRouteStatic(void)
{
    USER_UI_RouteDrawTemplatePath();

    USER_OLED_putString(2u, UI_ROUTE_INFO_COL, "R00     ", 11u);
    USER_OLED_putString(3u, UI_ROUTE_INFO_COL, "HOLD", 4u);
    USER_UI_RouteDrawProgressBar(0u);
    USER_OLED_putString(5u, UI_ROUTE_INFO_COL, "S-- IDLE", 8u);
    USER_OLED_putString(6u, UI_ROUTE_INFO_COL, "TMO ----", 8u);
}

/**
 * @brief 路线页面动态刷新 — 充电进度条、当前步骤文本、超时倒计时。
 *
 * @note ENTER 按下时启动充电流程，后续由 USER_UI_Route_Service5ms() 驱动。
 *       倒计时阶段触发蜂鸣器提示音。
 */
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

/**
 * @brief 路线页面按键处理 — ESC 短按取消充电/倒计时。
 */
void USER_UI_RouteOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    if ((key == ESC) && (event == USER_UI_KEY_EVENT_SHORT))
    {
        USER_UI_Route_CancelCharge();
        USER_OLED_CleanScreen();
        USER_UI_Core_MarkStaticDirty();
    }
}
