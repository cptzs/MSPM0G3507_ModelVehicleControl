#include "user_ui_internal.h"

#include "userlib_oled.h"
#include "userlib_servo.h"

/**
 * @file user_ui_page_actuator.c
 * @brief 执行器页面 — 当前包含 Servo Control（两路舵机角度/PWM 宽度/误差）。
 *
 * 后续如需增加独立执行器测试或舵机调参页，可继续收拢到本文件或拆成子页面。
 */

/**
 * @brief 绘制 Servo Control 页面静态布局（角度/宽度/误差标签）。
 */
void USER_UI_ShowActuatorPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "        SVO1     SVO2", 21u);
    USER_OLED_putString(2u, 0u, " ANG   00000    00000", 21u);
    USER_OLED_putString(3u, 0u, " WDH   00000    00000", 21u);
    USER_OLED_putString(4u, 0u, "fERR       00.000    ", 21u);
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
    case 2:
        USER_OLED_putI16(2u, 7u, USER_SERVO_GetAngle(SERVO_0), 5u);
        USER_OLED_putI16(2u, 16u, USER_SERVO_GetAngle(SERVO_1), 5u);
        break;

    case 3:
        USER_OLED_putUI16(3u, 7u, USER_SERVO_GetWidth(SERVO_0), 5u);
        USER_OLED_putUI16(3u, 16u, USER_SERVO_GetWidth(SERVO_1), 5u);
        break;

    case 4:
        USER_OLED_putFloat(4u, 10u, USER_SERVO_GetFError(), 2u, 4u);
        break;

    default:
        break;
    }
}
