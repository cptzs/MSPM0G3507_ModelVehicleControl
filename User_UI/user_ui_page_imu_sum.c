#include "user_ui_internal.h"

#include "globals.h"
#include "userlib_oled.h"

/**
 * @file user_ui_page_imu_sum.c
 * @brief IMU 累计和数据页面。
 *
 * 显示加速度计 (aSum) 和陀螺仪 (wSum) 三轴累计和。
 * 按键交互通过注册表 on_key() 回调处理，动态刷新函数只负责显示数据。
 * ENTER 短按清零所有累计和。
 */

static uint8_t s_imu_sum_update_line = 1u;

/**
 * @brief 显示 IMU 累计和页面的静态内容。
 */
void USER_UI_ShowIMUSumPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "      X      Y      Z", 21u);
    USER_OLED_putString(2u, 0u, "aSum:                ", 21u);
    USER_OLED_putString(3u, 0u, "  00000  00000  00000", 21u);
    USER_OLED_putString(4u, 0u, "wSum:                ", 21u);
    USER_OLED_putString(5u, 0u, "  00000  00000  00000", 21u);
    USER_OLED_putString(6u, 0u, "Reset: ENTER         ", 21u);
}

/**
 * @brief 显示 IMU 累计和页面的动态内容。
 */
void USER_UI_ShowIMUSumPageDynamic(void)
{
    s_imu_sum_update_line++;
    if (s_imu_sum_update_line > 7u)
    {
        s_imu_sum_update_line = 1u;
    }

    switch (s_imu_sum_update_line)
    {
    case 3u:
        USER_OLED_putFloat(3u, 2u, imu_data.sum_accx, 4u, 1u);
        USER_OLED_putFloat(3u, 8u, imu_data.sum_accy, 4u, 1u);
        USER_OLED_putFloat(3u, 15u, imu_data.sum_accz, 4u, 1u);
        break;
    case 5u:
        USER_OLED_putFloat(5u, 2u, imu_data.sum_gyrox, 4u, 1u);
        USER_OLED_putFloat(5u, 8u, imu_data.sum_gyroy, 4u, 1u);
        USER_OLED_putFloat(5u, 15u, imu_data.sum_gyroz, 4u, 1u);
        break;
    default:
        break;
    }
}

/**
 * @brief 处理 IMU 累计和页面的按键事件。
 *
 * @param key 按键类型。
 * @param event 按键事件。
 */
void USER_UI_IMUSumOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    if (event != USER_UI_KEY_EVENT_SHORT)
    {
        return;
    }

    if (key == ENTER)
    {
        /* 清零所有累计和 */
        imu_data.sum_accx = 0.0f;
        imu_data.sum_accy = 0.0f;
        imu_data.sum_accz = 0.0f;
        imu_data.sum_gyrox = 0.0f;
        imu_data.sum_gyroy = 0.0f;
        imu_data.sum_gyroz = 0.0f;
    }
}
