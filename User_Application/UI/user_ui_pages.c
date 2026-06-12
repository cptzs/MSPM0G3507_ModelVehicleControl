#include "user_ui_internal.h"

static const USER_UI_PageDef_t ui_page_table[] = {
    {PAGE_MOTOR, "Motor Control", USER_UI_ShowMotorPageStatic, USER_UI_ShowMotorPageDynamic, NULL},
    {PAGE_ENCODER, "Encoder Data ", USER_UI_ShowEncoderPageStatic, USER_UI_ShowEncoderPageDynamic, USER_UI_EncoderOnKey},
    {PAGE_PHOTOELECTRIC, "Photo Sensors", USER_UI_ShowPhotoelectricPageStatic, USER_UI_ShowPhotoelectricPageDynamic, USER_UI_PhotoelectricOnKey},
    {PAGE_ADC, "ADC Data     ", USER_UI_ShowAdcPageStatic, USER_UI_ShowAdcPageDynamic, NULL},
    {PAGE_LIDAR, "LiDAR Sensors", USER_UI_ShowLidarPageStatic, USER_UI_ShowLidarPageDynamic, USER_UI_LidarOnKey},
    {PAGE_GYROSCOPE, "IMU Data     ", USER_UI_ShowGyroscopePageStatic, USER_UI_ShowGyroscopePageDynamic, USER_UI_GyroscopeOnKey},
    {PAGE_IMU_SUM, "IMU Sum Data ", USER_UI_ShowIMUSumPageStatic, USER_UI_ShowIMUSumPageDynamic, USER_UI_IMUSumOnKey},
    {PAGE_CAMERA, "Smart Camera ", USER_UI_ShowCameraPageStatic, USER_UI_ShowCameraPageDynamic, NULL},
    {PAGE_SERVO, "Servo Control", USER_UI_ShowActuatorPageStatic, USER_UI_ShowActuatorPageDynamic, NULL},
    {PAGE_DEBUG, "Threads      ", USER_UI_ShowDebugPageStatic, USER_UI_ShowDebugPageDynamic, NULL},
    {PAGE_TEMPLATE, "Template Path", USER_UI_ShowRouteStatic, USER_UI_ShowRouteDynamic, USER_UI_RouteOnKey},
    {PAGE_UNITTEST, "UnitTest     ", USER_UI_ShowUnitTestStatic, USER_UI_ShowUnitTestDynamic, USER_UI_UnitTestOnKey},
};

const USER_UI_PageDef_t *USER_UI_GetPageTable(void)
{
    return ui_page_table;
}

uint8_t USER_UI_GetPageCount(void)
{
    return (uint8_t)(sizeof(ui_page_table) / sizeof(ui_page_table[0]));
}

const USER_UI_PageDef_t *USER_UI_GetPageByIndex(uint8_t index)
{
    if (index >= USER_UI_GetPageCount())
    {
        return NULL;
    }

    return &ui_page_table[index];
}

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
