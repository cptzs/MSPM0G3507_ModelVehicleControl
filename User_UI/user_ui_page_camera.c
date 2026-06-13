#include "user_ui_internal.h"
#include "userlib_oled.h"

/**
 * @file user_ui_page_camera.c
 * @brief 智能相机页面（占位）。
 *
 * 当前相机模组尚未接入，本页面作为预留占位。
 * 后续接入相机后在此实现图像预览、目标识别状态等显示逻辑。
 */

/**
 * @brief 绘制 Camera 页面静态占位内容（相机未接入提示）。
 */
void USER_UI_ShowCameraPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "Camera not connected ", 21u);
    USER_OLED_putString(3u, 0u, "Reserved for future  ", 21u);
    USER_OLED_putString(5u, 0u, "smart camera module  ", 21u);
}

/**
 * @brief Camera 页面动态刷新（当前无数据，预留）。
 */
void USER_UI_ShowCameraPageDynamic(void)
{
    /* 相机未接入，无动态数据刷新 */
}
