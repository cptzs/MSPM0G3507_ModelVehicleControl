#include "user_ui_internal.h"

#include "userlib_oled.h"
#include "userlib_servo.h"

typedef enum
{
    SERVO_OWNER_NONE = 0,
    SERVO_OWNER_UI
} USER_UI_ServoOwner_t;

static bool s_servo_manual_mode = false;
static Servo_Instance s_servo_selected = SERVO_0;
static USER_UI_ServoOwner_t s_servo_owner = SERVO_OWNER_NONE;

/**
 * @file user_ui_page_actuator.c
 * @brief 执行器页面 — Servo Manual（监视两路舵机，显式进入手动角度调节）。
 *
 * 后续如需增加独立执行器测试或舵机调参页，可继续收拢到本文件或拆成子页面。
 */

/**
 * @brief 绘制 Servo Control 页面静态布局（角度/宽度/误差标签）。
 */
void USER_UI_ShowActuatorPageStatic(void)
{
    USER_OLED_PutString(1u, 0u, "MODE MON    SEL SVO1 ", 21u);
    USER_OLED_PutString(2u, 0u, " ANG   00000    00000", 21u);
    USER_OLED_PutString(3u, 0u, " WDH   00000    00000", 21u);
    USER_OLED_PutString(4u, 0u, "STEP      1deg       ", 21u);
    USER_OLED_PutString(5u, 0u, "fERR       00.000    ", 21u);
    USER_OLED_PutString(6u, 0u, "ENT:MANUAL          ", 21u);
    USER_OLED_PutString(7u, 0u, "ESC:BACK            ", 21u);
}

/**
 * @brief Servo Control 页面动态刷新 — 逐行轮询更新两路舵机数据。
 */
void USER_UI_ShowActuatorPageDynamic(void)
{
    static uint8_t update_line = 1u;

    update_line++;
    if (update_line > 7u)
    {
        update_line = 1u;
    }

    switch (update_line)
    {
    case 1:
        USER_OLED_PutString(1u, 5u, s_servo_manual_mode ? "MAN" : "MON", 3u);
        USER_OLED_PutString(1u, 16u, (s_servo_selected == SERVO_0) ? "SVO1" : "SVO2", 4u);
        USER_OLED_PutString(6u, 0u, s_servo_manual_mode ? "ENT:MON             " : "ENT:MANUAL          ", 21u);
        USER_OLED_PutString(7u, 0u, s_servo_manual_mode ? "L/R:-/+  UP/DN:SEL  " : "ESC:BACK            ", 21u);
        break;

    case 2:
        USER_OLED_PutI16(2u, 7u, USER_Servo_GetAngle(SERVO_0), 5u);
        USER_OLED_PutI16(2u, 16u, USER_Servo_GetAngle(SERVO_1), 5u);
        break;

    case 3:
        USER_OLED_PutUI16(3u, 7u, USER_Servo_GetWidth(SERVO_0), 5u);
        USER_OLED_PutUI16(3u, 16u, USER_Servo_GetWidth(SERVO_1), 5u);
        break;

    case 4:
        break;

    case 5:
        USER_OLED_PutFloat(5u, 10u, USER_Servo_GetFError(), 2u, 4u);
        break;

    default:
        break;
    }
}

void USER_UI_ActuatorOnEnter(void)
{
    s_servo_manual_mode = false;
    s_servo_owner = SERVO_OWNER_NONE;
}

void USER_UI_ActuatorOnExit(void)
{
    s_servo_manual_mode = false;
    s_servo_owner = SERVO_OWNER_NONE;
}

void USER_UI_ActuatorOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    int16_t angle;

    if (event == USER_UI_KEY_EVENT_NONE)
    {
        return;
    }

    if ((key == ENTER) && (event == USER_UI_KEY_EVENT_SHORT))
    {
        if (s_servo_manual_mode)
        {
            s_servo_manual_mode = false;
            s_servo_owner = SERVO_OWNER_NONE;
        }
        else if (!USER_UI_Route_IsBusy())
        {
            s_servo_manual_mode = true;
            s_servo_owner = SERVO_OWNER_UI;
        }
        else
        {
            USER_OLED_PutString(6u, 0u, "BUSY ROUTE          ", 21u);
        }
        return;
    }

    if (!s_servo_manual_mode || (s_servo_owner != SERVO_OWNER_UI))
    {
        return;
    }

    if ((event != USER_UI_KEY_EVENT_SHORT) && (event != USER_UI_KEY_EVENT_LONG_REPEAT))
    {
        return;
    }

    switch (key)
    {
    case UP:
    case DOWN:
        s_servo_selected = (s_servo_selected == SERVO_0) ? SERVO_1 : SERVO_0;
        break;

    case LEFT:
        angle = USER_Servo_GetAngle(s_servo_selected);
        (void)USER_Servo_SetAngle(s_servo_selected, (int16_t)(angle - 1));
        break;

    case RIGHT:
        angle = USER_Servo_GetAngle(s_servo_selected);
        (void)USER_Servo_SetAngle(s_servo_selected, (int16_t)(angle + 1));
        break;

    default:
        break;
    }
}
