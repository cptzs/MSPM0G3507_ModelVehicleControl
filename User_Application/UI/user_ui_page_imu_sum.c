#include "user_ui_internal.h"
#include "user_ui_pages.h"
#include "user_ui_input.h"

// Static page variables
static uint8_t imu_display_mode = 0;

void USER_UI_ShowIMUSumPageStatic(void) {
    // Draw static UI elements for IMU Sum page
    OLED_ClearScreen();
    OLED_PrintString(0, 0, "IMU Sum Data");
}

void USER_UI_ShowIMUSumPageDynamic(void) {
    // Draw dynamic IMU Sum values
    int16_t roll, pitch, yaw;
    IMU_GetSumData(&roll, &pitch, &yaw);
    char buf[32];
    snprintf(buf, sizeof(buf), "R:%d P:%d Y:%d", roll, pitch, yaw);
    OLED_PrintString(0, 2, buf);
}

void USER_UI_IMUSumOnKey(Button_t key, USER_UI_KeyEvent_t event) {
    if(event == KEY_EVENT_SHORT_PRESS) {
        if(key == BUTTON_UP) imu_display_mode = (imu_display_mode + 1) % 3;
        else if(key == BUTTON_DOWN) imu_display_mode = (imu_display_mode + 2) % 3;
    }
}