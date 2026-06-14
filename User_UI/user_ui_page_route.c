/**
 * @file user_ui_page_route.c
 * @brief 路线页面 (Template Path) — 赛道图形绘制 + 路线动作预览 + 启动交互。
 *
 * 静态内容绘制标准赛道模板图形（双竖线 + 两端半圆 + 方向箭头）。
 * 动态内容刷新充电进度条、当前动作步骤、超时剩余时间。
 * ENTER 长按启动充电→倒计时→比赛流程；ESC 短按取消。
 */

#include "user_ui_internal.h"
#include "user_ui_route_map.h"

#include "userlib_oled.h"
#include "userapp_race.h"

#define UI_ROUTE_INFO_COL 10u
#define UI_ROUTE_BAR_X 70u
#define UI_ROUTE_BAR_Y 26u
#define UI_ROUTE_BAR_W 42u
#define UI_ROUTE_BAR_H 6u
#define UI_ROUTE_CHARGE_TIME_MS 2000u

static uint8_t s_route_last_progress = 0xFFu;

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

static uint16_t USER_UI_RouteDiv10U16(uint16_t value)
{
    return (uint16_t)(((uint32_t)value * 52429u) >> 19);
}

static uint16_t USER_UI_RouteDiv100U16(uint16_t value)
{
    return USER_UI_RouteDiv10U16(USER_UI_RouteDiv10U16(value));
}

static uint8_t USER_UI_RouteChargePercent(uint16_t press_time)
{
    return (uint8_t)(USER_UI_RouteDiv10U16(press_time) >> 1);
}

static void USER_UI_RouteAppendChar(char **cursor, uint8_t *remaining, char ch)
{
    if (*remaining == 0u)
    {
        return;
    }

    **cursor = ch;
    (*cursor)++;
    (*remaining)--;
}

static void USER_UI_RouteAppendText(char **cursor, uint8_t *remaining, const char *text)
{
    while ((text != NULL) && (*text != '\0') && (*remaining > 0u))
    {
        USER_UI_RouteAppendChar(cursor, remaining, *text);
        text++;
    }
}

static void USER_UI_RouteAppend2Digit(char **cursor, uint8_t *remaining, uint8_t value)
{
    uint8_t hundreds = 0u;
    uint8_t tens = 0u;

    if (value >= 100u)
    {
        while (value >= 100u)
        {
            value = (uint8_t)(value - 100u);
            hundreds++;
        }
        USER_UI_RouteAppendChar(cursor, remaining, (char)('0' + hundreds));
    }

    while (value >= 10u)
    {
        value = (uint8_t)(value - 10u);
        tens++;
    }

    USER_UI_RouteAppendChar(cursor, remaining, (char)('0' + tens));
    USER_UI_RouteAppendChar(cursor, remaining, (char)('0' + value));
}

static void USER_UI_RouteAppendU16(char **cursor, uint8_t *remaining, uint16_t value)
{
    static const uint16_t dec_place[] = {10000u, 1000u, 100u, 10u, 1u};
    uint8_t place = 0u;
    bool started = false;

    while (place < (uint8_t)(sizeof(dec_place) / sizeof(dec_place[0])))
    {
        uint8_t digit = 0u;
        const uint16_t divisor = dec_place[place++];

        while (value >= divisor)
        {
            value = (uint16_t)(value - divisor);
            digit++;
        }

        if ((digit != 0u) || started || (divisor == 1u))
        {
            USER_UI_RouteAppendChar(cursor, remaining, (char)('0' + digit));
            started = true;
        }
    }
}

static void USER_UI_RouteFormatFixedText(const char *text, char *out_buf, uint8_t out_len)
{
    char *cursor;
    uint8_t remaining;

    if ((out_buf == NULL) || (out_len == 0u))
    {
        return;
    }

    for (remaining = 0u; remaining < (uint8_t)(out_len - 1u); remaining++)
    {
        out_buf[remaining] = ' ';
    }
    out_buf[out_len - 1u] = '\0';

    cursor = out_buf;
    remaining = (uint8_t)(out_len - 1u);
    USER_UI_RouteAppendText(&cursor, &remaining, text);
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

    fill_w = (uint8_t)USER_UI_RouteDiv100U16((uint16_t)((uint16_t)(UI_ROUTE_BAR_W - 2u) * percent));

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
    char *cursor;
    uint8_t remaining;

    if ((out_buf == NULL) || (out_len == 0u))
    {
        return;
    }

    if ((action_ptr == NULL) || (step_index < 0))
    {
        USER_UI_RouteFormatFixedText("S-- IDLE", out_buf, out_len);
        return;
    }

    USER_UI_RouteFormatFixedText("", out_buf, out_len);
    cursor = out_buf;
    remaining = (uint8_t)(out_len - 1u);

    step_no = (uint8_t)(step_index + 1u);

    USER_UI_RouteAppendChar(&cursor, &remaining, 'S');
    USER_UI_RouteAppend2Digit(&cursor, &remaining, step_no);
    USER_UI_RouteAppendChar(&cursor, &remaining, ' ');

    switch (action_ptr->action_type)
    {
    case USER_Race_ACTION_MOVE_DISTANCE:
        main_value = USER_UI_RouteDiv10U16(USER_UI_RouteAbsI16((int16_t)action_ptr->param1));
        if (action_ptr->param1 >= 0.0f)
        {
            USER_UI_RouteAppendText(&cursor, &remaining, "FW ");
        }
        else
        {
            USER_UI_RouteAppendText(&cursor, &remaining, "BW ");
        }
        USER_UI_RouteAppendU16(&cursor, &remaining, main_value);
        break;

    case USER_Race_ACTION_ROTATE_ANGLE:
        main_value = USER_UI_RouteAbsI16((int16_t)action_ptr->param1);
        if (action_ptr->param1 >= 0.0f)
        {
            USER_UI_RouteAppendText(&cursor, &remaining, "TR ");
        }
        else
        {
            USER_UI_RouteAppendText(&cursor, &remaining, "TL ");
        }
        USER_UI_RouteAppendU16(&cursor, &remaining, main_value);
        break;

    case USER_Race_ACTION_WAIT_MS:
        main_value = (uint16_t)action_ptr->param1;
        USER_UI_RouteAppendText(&cursor, &remaining, "WT ");
        USER_UI_RouteAppendU16(&cursor, &remaining, main_value);
        break;

    case USER_Race_ACTION_STOP:
        USER_UI_RouteAppendText(&cursor, &remaining, "STP");
        break;

    case USER_Race_ACTION_END:
        USER_UI_RouteAppendText(&cursor, &remaining, "END");
        break;

    default:
        USER_UI_RouteAppendText(&cursor, &remaining, "N/A");
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
    USER_UI_RouteMap_Draw(RACE_ROUTE_TEMPLATE);

    USER_OLED_putString(2u, UI_ROUTE_INFO_COL, "R00 ", 4u);
    USER_OLED_putString(2u,
                        UI_ROUTE_INFO_COL + 4u,
                        USER_Race_GetRouteName(RACE_ROUTE_TEMPLATE),
                        7u);
    USER_OLED_putString(3u, UI_ROUTE_INFO_COL, "HOLD", 4u);
    USER_UI_RouteDrawProgressBar(0u);
    s_route_last_progress = 0u;
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

    if (USER_UI_Route_IsWaitingRelease())
    {
        progress = 100u;
    }
    else if (USER_UI_Route_IsCharging())
    {
        if (press_time >= UI_ROUTE_CHARGE_TIME_MS)
        {
            progress = 100u;
        }
        else
        {
            progress = USER_UI_RouteChargePercent(press_time);
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

    USER_OLED_putString(3u,
                        UI_ROUTE_INFO_COL,
                        USER_UI_Route_IsWaitingRelease() ? "REL " : "HOLD",
                        4u);

    if (progress != s_route_last_progress)
    {
        USER_UI_RouteDrawProgressBar(progress);
        s_route_last_progress = progress;
    }

    if (USER_UI_Route_IsCountdown())
    {
        USER_UI_RouteFormatFixedText("S-- START", step_text, sizeof(step_text));
        USER_OLED_putString(5u, UI_ROUTE_INFO_COL, step_text, 12u);
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
    if ((key == ESC) && (event != USER_UI_KEY_EVENT_NONE))
    {
        USER_UI_Route_CancelCharge();
        USER_OLED_CleanScreen();
        USER_UI_Core_MarkStaticDirty();
    }
}

void USER_UI_RouteOnExit(void)
{
    USER_UI_Route_CancelCharge();
}
