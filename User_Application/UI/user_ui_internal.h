#ifndef USER_UI_INTERNAL_H
#define USER_UI_INTERNAL_H

/**
 * @file user_ui_internal.h
 * @brief UI 子模块内部接口。
 *
 * 本文件集中保存 UI 页面枚举、页面绘制函数声明以及 legacy 页面实现
 * 仍需使用的设备/应用依赖。后续逐页迁移时，各页面 .c 文件应只包含
 * 自己需要的驱动头，最终逐步瘦身本文件。
 */

#include "user_ui_public.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "globals.h"
#include "userlib_can.h"
#include "userlib_lbb.h"
#include "userlib_lidar.h"
#include "userlib_motor.h"
#include "userlib_oemt_an.h"
#include "userlib_oled.h"
#include "userlib_servo.h"
#include "userlib_uart.h"
#include "userapp_race.h"

#define DISPLAY_PAGE_COUNT 11

/**
 * @brief OLED 显示页面枚举。
 *
 * 当前仍保持 legacy 页序，避免一次性修改 user_ui.c 的页面导航逻辑。
 * 后续新增 UnitTest 页面时，应显式增加 PAGE_UNITTEST，而不是复用 PAGE_CAMERA。
 */
typedef enum
{
    PAGE_MOTOR = 1,
    PAGE_ENCODER,
    PAGE_PHOTOELECTRIC,
    PAGE_ADC,
    PAGE_LIDAR,
    PAGE_GYROSCOPE,
    PAGE_CAMERA,
    PAGE_SERVO,
    PAGE_DEBUG,
    PAGE_IMU_SUM,
    PAGE_TEMPLATE
} DisplayPage_t;

/* ---- legacy 页面分发接口 ---- */
void USER_UI_ShowStaticContent(DisplayPage_t page);
void USER_UI_ShowDynamicContent(DisplayPage_t page);

/* ---- legacy 页面静态 / 动态内容绘制函数 ---- */
void USER_UI_ShowMotorStatic(void);
void USER_UI_ShowMotorDynamic(void);
void USER_UI_ShowEncoderStatic(void);
void USER_UI_ShowEncoderDynamic(void);
void USER_UI_ShowPhotoelectricStatic(void);
void USER_UI_ShowPhotoelectricDynamic(void);
void USER_UI_ShowAdcStatic(void);
void USER_UI_ShowAdcDynamic(void);
void USER_UI_ShowLidarStatic(void);
void USER_UI_ShowLidarDynamic(void);
void USER_UI_ShowGyroscopeStatic(void);
void USER_UI_ShowGyroscopeDynamic(void);
void USER_UI_ShowCameraStatic(void);
void USER_UI_ShowCameraDynamic(void);
void USER_UI_ShowServoStatic(void);
void USER_UI_ShowServoDynamic(void);
void USER_UI_ShowDebugStatic(void);
void USER_UI_ShowDebugDynamic(void);
void USER_UI_ShowIMUSumStatic(void);
void USER_UI_ShowIMUSumDynamic(void);
void USER_UI_ShowTemplateStatic(void);
void USER_UI_ShowTemplateDynamic(void);

#endif /* USER_UI_INTERNAL_H */
