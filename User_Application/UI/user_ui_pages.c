#include "user_ui_internal.h"

static const USER_UI_PageDef_t ui_page_table[] = {
    {PAGE_MOTOR, "Motor Control", USER_UI_ShowMotorStatic, USER_UI_ShowMotorDynamic, NULL},
    {PAGE_ENCODER, "Encoder Data ", USER_UI_ShowEncoderStatic, USER_UI_ShowEncoderDynamic, NULL},
    {PAGE_PHOTOELECTRIC, "Photo Sensors", USER_UI_ShowPhotoelectricStatic, USER_UI_ShowPhotoelectricDynamic, NULL},
    {PAGE_ADC, "ADC Data     ", USER_UI_ShowAdcStatic, USER_UI_ShowAdcDynamic, NULL},
    {PAGE_LIDAR, "LiDAR Sensors", USER_UI_ShowLidarStatic, USER_UI_ShowLidarDynamic, NULL},
    {PAGE_GYROSCOPE, "IMU Data     ", USER_UI_ShowGyroscopeStatic, USER_UI_ShowGyroscopeDynamic, NULL},
    {PAGE_CAMERA, "Smart Camera ", USER_UI_ShowCameraStatic, USER_UI_ShowCameraDynamic, NULL},
    {PAGE_SERVO, "Servo Control", USER_UI_ShowServoStatic, USER_UI_ShowServoDynamic, NULL},
    {PAGE_DEBUG, "Threads      ", USER_UI_ShowDebugStatic, USER_UI_ShowDebugDynamic, NULL},
    {PAGE_IMU_SUM, "IMU Sum Data ", USER_UI_ShowIMUSumStatic, USER_UI_ShowIMUSumDynamic, NULL},
    {PAGE_TEMPLATE, "Template Path", USER_UI_ShowTemplateStatic, USER_UI_ShowTemplateDynamic, NULL},
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
