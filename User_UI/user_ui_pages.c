/**
 * @file user_ui_pages.c
 * @brief UI 页面注册表 — 编译期静态定义全部 13 页的描述符数组。
 *
 * 每页包含：页面枚举值、13 字符标题、静态/动态绘制函数指针、按键回调（可为 NULL）。
 * 注册表顺序决定 PREV/NEXT 导航的循环顺序。
 */

#include "user_ui_internal.h"

/** @brief 全局页面注册表（13 页，编译期常量） */
static const USER_UI_PageDef_t ui_page_table[] = {
    {PAGE_MOTOR, "Motor PID Mon", USER_UI_ShowMotorPageStatic, USER_UI_ShowMotorPageDynamic, NULL, NULL, NULL, 1u},
    {PAGE_ENCODER, "Encoder Data ", USER_UI_ShowEncoderPageStatic, USER_UI_ShowEncoderPageDynamic, USER_UI_EncoderOnKey, NULL, NULL, 1u},
    {PAGE_PHOTOELECTRIC, "Photo Sensors", USER_UI_ShowPhotoelectricPageStatic, USER_UI_ShowPhotoelectricPageDynamic, USER_UI_PhotoelectricOnKey, NULL, NULL, 1u},
    {PAGE_ADC, "ADC Data     ", USER_UI_ShowAdcPageStatic, USER_UI_ShowAdcPageDynamic, NULL, NULL, NULL, 1u},
    {PAGE_LIDAR, "LiDAR Sensors", USER_UI_ShowLidarPageStatic, USER_UI_ShowLidarPageDynamic, USER_UI_LidarOnKey, NULL, NULL, 1u},
    {PAGE_GYROSCOPE, "IMU Data     ", USER_UI_ShowGyroscopePageStatic, USER_UI_ShowGyroscopePageDynamic, USER_UI_GyroscopeOnKey, NULL, NULL, 1u},
    {PAGE_IMU_SUM, "IMU Sum Data ", USER_UI_ShowIMUSumPageStatic, USER_UI_ShowIMUSumPageDynamic, USER_UI_IMUSumOnKey, NULL, NULL, 1u},
    {PAGE_CAMERA, "Smart Camera ", USER_UI_ShowCameraPageStatic, USER_UI_ShowCameraPageDynamic, NULL, NULL, NULL, 4u},
    {PAGE_SERVO, "Servo Manual ", USER_UI_ShowActuatorPageStatic, USER_UI_ShowActuatorPageDynamic, USER_UI_ActuatorOnKey, USER_UI_ActuatorOnEnter, USER_UI_ActuatorOnExit, 1u},
    {PAGE_THREADS, "Threads      ", USER_UI_ShowThreadsPageStatic, USER_UI_ShowThreadsPageDynamic, USER_UI_ThreadsOnKey, USER_UI_ThreadsOnEnter, NULL, 4u},
    {PAGE_SYSINFO, "SysInfo      ", USER_UI_ShowSysInfoPageStatic, USER_UI_ShowSysInfoPageDynamic, NULL, NULL, NULL, 4u},
    {PAGE_TEMPLATE, "Template Path", USER_UI_ShowRouteStatic, USER_UI_ShowRouteDynamic, USER_UI_RouteOnKey, NULL, USER_UI_RouteOnExit, 1u},
    {PAGE_ARC_TEST, "Arc Test    ", USER_UI_ShowRouteStatic, USER_UI_ShowRouteDynamic, USER_UI_RouteOnKey, NULL, USER_UI_RouteOnExit, 1u},
    {PAGE_UNITTEST, "UnitTest     ", USER_UI_ShowUnitTestStatic, USER_UI_ShowUnitTestDynamic, USER_UI_UnitTestOnKey, NULL, NULL, 4u},
};

/**
 * @brief 获取页面注册表首指针。
 *
 * @return 指向 ui_page_table[0] 的常量指针。
 */
const USER_UI_PageDef_t *USER_UI_GetPageTable(void)
{
    return ui_page_table;
}

/**
 * @brief 获取注册表中的页面总数。
 *
 * @return 页面数量（编译期确定，当前为 12）。
 */
uint8_t USER_UI_GetPageCount(void)
{
    return (uint8_t)(sizeof(ui_page_table) / sizeof(ui_page_table[0]));
}

/**
 * @brief 按数组索引获取页面描述符。
 *
 * @param index 数组索引（0 ~ page_count-1）。
 * @return 指向页面描述符的指针，越界返回 NULL。
 */
const USER_UI_PageDef_t *USER_UI_GetPageByIndex(uint8_t index)
{
    if (index >= USER_UI_GetPageCount())
    {
        return NULL;
    }

    return &ui_page_table[index];
}

/**
 * @brief 按页面枚举值在注册表中查找页面描述符。
 *
 * @param page 目标页面枚举值。
 * @return 指向页面描述符的指针，未找到返回 NULL。
 */
const USER_UI_PageDef_t *USER_UI_FindPage(DisplayPage_t page)
{
    uint8_t i;

    for (i = 0u; i < USER_UI_GetPageCount(); i++)
    {
        if (ui_page_table[i].page == page)
        {
            return &ui_page_table[i];
        }
    }

    return NULL;
}
