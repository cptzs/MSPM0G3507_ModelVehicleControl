#ifndef USER_UI_H
#define USER_UI_H

/**
 * @file user_ui.h
 * @brief 本地操作界面模块。
 *
 * 本模块负责 OLED 多页面显示、按键导航、比赛路线选择与启动倒计时、
 * LED / 蜂鸣器提示等全部人机交互逻辑。
 *
 * 设计边界：
 * - 本模块是唯一直接操作 OLED、LED、蜂鸣器的应用层模块。
 * - 比赛路线模块（userapp_race）不包含任何 UI 代码。
 * - 倒计时结束前不调用 race 启动接口，确保 UI 层完全拥有启动时序。
 */

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
 */
typedef enum
{
    PAGE_MOTOR = 1,
    PAGE_ENCODER,
    PAGE_PHOTOELECTRIC,
    PAGE_ADC,
    PAGE_LIDAR,
    PAGE_GYROSCOPE,
    PAGE_CAMERA,   /* 复用原相机空白页作为 UnitTest 页面 */
    PAGE_SERVO,
    PAGE_DEBUG,
    PAGE_IMU_SUM,
    PAGE_TEMPLATE
} DisplayPage_t;

/**
 * @brief UI 模块周期任务。
 *
 * 由主循环每 5 ms 调用一次，负责页面导航、数据显示刷新、
 * 比赛启动倒计时等全部界面逻辑。
 */
void USER_UI_Task(void);

/* ---- 各页面静态 / 动态内容绘制函数 ---- */
void USER_UI_ShowStaticContent(DisplayPage_t page);
void USER_UI_ShowDynamicContent(DisplayPage_t page);

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

/* ---- UnitTest 页面实现 ----
 * user_ui.c 中 PAGE_CAMERA 原本是空白页。这里用宏把该页的调用转发到
 * UnitTest 实现，同时把 user_ui.c 末尾两个空函数改名为 dummy，避免
 * 大文件重写。宏定义必须放在原函数原型之后。
 */
void USER_UI_ShowUnitTestStatic(void);
void USER_UI_ShowUnitTestDynamic(void);
#define USER_UI_ShowCameraStatic(...)  USER_UI_ShowUnitTestStatic(__VA_ARGS__);  void USER_UI_ShowCameraStatic_Dummy(__VA_ARGS__)
#define USER_UI_ShowCameraDynamic(...) USER_UI_ShowUnitTestDynamic(__VA_ARGS__); void USER_UI_ShowCameraDynamic_Dummy(__VA_ARGS__)

#endif /* USER_UI_H */
