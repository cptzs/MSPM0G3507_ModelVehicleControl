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
#include "userapp_mcm.h"
#include "userapp_race.h"

#define DISPLAY_PAGE_COUNT 12

/**
 * @brief OLED 显示页面枚举。
 *
 * 当前前 11 页仍保持 legacy 页序，避免一次性修改 user_ui.c 的页面导航逻辑。
 * PAGE_UNITTEST 是新 UI 分层后的独立页面入口，后续由页面表或 legacy 分发器接入。
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
    PAGE_TEMPLATE,
    PAGE_UNITTEST
} DisplayPage_t;

/**
 * @brief UI 页面按键事件类型。
 *
 * 新页面优先使用该事件类型，由 UI core 统一消费底层按键事件后分发给当前页面。
 * legacy 页面仍可临时直接调用 USER_LBB_Button_ReadState()/Consume* 接口。
 */
typedef enum
{
    USER_UI_KEY_EVENT_NONE = 0,
    USER_UI_KEY_EVENT_SHORT,
    USER_UI_KEY_EVENT_LONG,
    USER_UI_KEY_EVENT_LONG_REPEAT
} USER_UI_KeyEvent_t;

typedef void (*USER_UI_PageDrawFunc_t)(void);
typedef void (*USER_UI_PageKeyFunc_t)(Button_t key, USER_UI_KeyEvent_t event);

typedef struct
{
    DisplayPage_t page;
    const char *title;
    USER_UI_PageDrawFunc_t show_static;
    USER_UI_PageDrawFunc_t show_dynamic;
    USER_UI_PageKeyFunc_t on_key;
} USER_UI_PageDef_t;

const USER_UI_PageDef_t *USER_UI_GetPageTable(void);
uint8_t USER_UI_GetPageCount(void);
const USER_UI_PageDef_t *USER_UI_FindPage(DisplayPage_t page);
const USER_UI_PageDef_t *USER_UI_GetPageByIndex(uint8_t index);

bool USER_UI_DrawPageStaticFromRegistry(DisplayPage_t page);
bool USER_UI_DrawPageDynamicFromRegistry(DisplayPage_t page);
bool USER_UI_DispatchPageKeyFromRegistry(DisplayPage_t page, Button_t key, USER_UI_KeyEvent_t event);
DisplayPage_t USER_UI_GetAdjacentPageFromRegistry(DisplayPage_t current_page, bool forward);

/* ---- UI input/event adapters ---- */
USER_UI_KeyEvent_t USER_UI_ConvertButtonEvent(USER_LBB_ButtonEvent_t event);
USER_UI_KeyEvent_t USER_UI_ConsumeButtonEvent(Button_t button);
bool USER_UI_ConsumeAndDispatchButton(DisplayPage_t page, Button_t button);

/* ---- UI core state helpers ---- */
DisplayPage_t USER_UI_Core_GetCurrentPage(void);
void USER_UI_Core_SetCurrentPage(DisplayPage_t page);
void USER_UI_Core_GotoAdjacentPage(bool forward);
bool USER_UI_Core_IsStaticDirty(void);
void USER_UI_Core_MarkStaticDirty(void);
void USER_UI_Core_ClearStaticDirty(void);
void USER_UI_Core_RedrawStaticIfNeeded(void);
void USER_UI_Core_DrawCurrentDynamic(void);

/* ---- Route start interaction context ---- */
void USER_UI_Route_Reset(void);
void USER_UI_Route_StartCharge(uint8_t route);
void USER_UI_Route_CancelCharge(void);
bool USER_UI_Route_IsBusy(void);
bool USER_UI_Route_IsCharging(void);
bool USER_UI_Route_IsCountdown(void);
uint8_t USER_UI_Route_GetPendingRoute(void);
uint16_t USER_UI_Route_GetCountdownRemainMs(void);
void USER_UI_Route_Service5ms(bool enter_is_pressed, uint16_t enter_press_time_ms);

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

/* ---- 新分层页面 ---- */
void USER_UI_ShowMotorPageStatic(void);
void USER_UI_ShowMotorPageDynamic(void);
void USER_UI_ShowActuatorPageStatic(void);
void USER_UI_ShowActuatorPageDynamic(void);
void USER_UI_ShowDebugPageStatic(void);
void USER_UI_ShowDebugPageDynamic(void);
void USER_UI_ShowRouteStatic(void);
void USER_UI_ShowRouteDynamic(void);
void USER_UI_RouteOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ShowUnitTestStatic(void);
void USER_UI_ShowUnitTestDynamic(void);
void USER_UI_UnitTestOnKey(Button_t key, USER_UI_KeyEvent_t event);

#endif /* USER_UI_INTERNAL_H */
