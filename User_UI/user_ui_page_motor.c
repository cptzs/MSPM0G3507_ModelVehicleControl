#include "user_ui_internal.h"

/**
 * @file user_ui_page_motor.c
 * @brief Motor Control 页面 — 显示左右电机 PID 状态（模式/目标/实际/误差/积分/微分/PWM）。
 *
 * 采用逐行轮询刷新策略，每 5ms 刷新一行，降低 OLED SPI 瞬时负载。
 */

/**
 * @brief 绘制 Motor Control 页面静态布局（7 行标签）。
 */
void USER_UI_ShowMotorPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "MODE    00000   00000", 21u); /* 左右电机模式 */
    USER_OLED_putString(2u, 0u, "REQU    00000   00000", 21u); /* 左右目标速度 */
    USER_OLED_putString(3u, 0u, "REAL    00000   00000", 21u); /* 左右实际速度 */
    USER_OLED_putString(4u, 0u, "ERR     00000   00000", 21u); /* 左右误差 */
    USER_OLED_putString(5u, 0u, "ESUM    00000   00000", 21u); /* 左右积分 */
    USER_OLED_putString(6u, 0u, "EDIV    00000   00000", 21u); /* 左右微分 */
    USER_OLED_putString(7u, 0u, "PWM     00000   00000", 21u); /* 左右PWM占空比 */
}

/**
 * @brief Motor Control 页面动态刷新 — 逐行轮询更新左右电机 PID 参数。
 *
 * @note 每 5ms 刷新 1 行（共 7 行），周期约 35ms 完成全屏刷新。
 */
void USER_UI_ShowMotorPageDynamic(void)
{
    static uint8_t update_line = 1u;

    update_line++;
    if (update_line > 7u)
    {
        update_line = 1u;
    }

    switch (update_line)
    {
    case 1u:
        USER_OLED_putUI16(1u, 8u, USER_Motor_GetMode(MOTOR_0_LEFT), 5u);
        USER_OLED_putUI16(1u, 16u, USER_Motor_GetMode(MOTOR_1_RIGHT), 5u);
        break;

    case 2u:
        USER_OLED_putI16(2u, 8u, (int16_t)speed_pid[MOTOR_0_LEFT].target, 5u);
        USER_OLED_putI16(2u, 16u, (int16_t)speed_pid[MOTOR_1_RIGHT].target, 5u);
        break;

    case 3u:
        USER_OLED_putI16(3u, 8u, (int16_t)speed_pid[MOTOR_0_LEFT].current, 5u);
        USER_OLED_putI16(3u, 16u, (int16_t)speed_pid[MOTOR_1_RIGHT].current, 5u);
        break;

    case 4u:
        USER_OLED_putI16(4u, 8u, (int16_t)speed_pid[MOTOR_0_LEFT].error, 5u);
        USER_OLED_putI16(4u, 16u, (int16_t)speed_pid[MOTOR_1_RIGHT].error, 5u);
        break;

    case 5u:
        USER_OLED_putI16(5u, 8u, (int16_t)speed_pid[MOTOR_0_LEFT].integral, 5u);
        USER_OLED_putI16(5u, 16u, (int16_t)speed_pid[MOTOR_1_RIGHT].integral, 5u);
        break;

    case 6u:
        USER_OLED_putI16(6u, 8u, (int16_t)speed_pid[MOTOR_0_LEFT].derivative, 5u);
        USER_OLED_putI16(6u, 16u, (int16_t)speed_pid[MOTOR_1_RIGHT].derivative, 5u);
        break;

    case 7u:
        USER_OLED_putI16(7u, 8u, (int16_t)speed_pid[MOTOR_0_LEFT].output, 5u);
        USER_OLED_putI16(7u, 16u, (int16_t)speed_pid[MOTOR_1_RIGHT].output, 5u);
        break;

    default:
        break;
    }
}
