#ifndef USER_UI_INTERNAL_H
#define USER_UI_INTERNAL_H

/**
 * @file user_ui_internal.h
 * @brief UI 模块内部接口（注册表驱动架构）。
 *
 * 已全面切换到新 UI 架构，不再包含 legacy 页面声明。
 * 各页面 .c 文件通过本头获得统一的驱动访问入口。
 */

#include "user_ui_public.h"

#include <stdbool.h>
#include <stdint.h>

#include "userlib_lbb.h"

/**
 * @brief OLED 显示页面枚举（注册表驱动，共 13 页）。
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
    PAGE_THREADS,
    PAGE_SYSINFO,
    PAGE_IMU_SUM,
    PAGE_TEMPLATE,
    PAGE_UNITTEST
} DisplayPage_t;

/**
 * @brief UI 页面按键事件类型。
 *
 * 由 UI core 统一消费底层按键事件后分发给当前页面 on_key() 回调。
 */
typedef enum
{
    USER_UI_KEY_EVENT_NONE = 0,
    USER_UI_KEY_EVENT_SHORT,
    USER_UI_KEY_EVENT_LONG,
    USER_UI_KEY_EVENT_LONG_REPEAT
} USER_UI_KeyEvent_t;

typedef enum
{
    UI_VIEW_MAIN_MENU = 0,
    UI_VIEW_SUB_MENU,
    UI_VIEW_PAGE,
    UI_VIEW_PLACEHOLDER
} USER_UI_ViewMode_t;

typedef void (*USER_UI_PageDrawFunc_t)(void);
typedef void (*USER_UI_PageKeyFunc_t)(Button_t key, USER_UI_KeyEvent_t event);
typedef void (*USER_UI_PageLifecycleFunc_t)(void);

typedef struct
{
    DisplayPage_t page;
    const char *title;
    USER_UI_PageDrawFunc_t show_static;
    USER_UI_PageDrawFunc_t show_dynamic;
    USER_UI_PageKeyFunc_t on_key;
    USER_UI_PageLifecycleFunc_t on_enter;
    USER_UI_PageLifecycleFunc_t on_exit;
    uint8_t refresh_divider;
} USER_UI_PageDef_t;

const USER_UI_PageDef_t *USER_UI_GetPageTable(void);
uint8_t USER_UI_GetPageCount(void);
const USER_UI_PageDef_t *USER_UI_FindPage(DisplayPage_t page);
const USER_UI_PageDef_t *USER_UI_GetPageByIndex(uint8_t index);

bool USER_UI_DrawPageStaticFromRegistry(DisplayPage_t page);
bool USER_UI_DrawPageDynamicFromRegistry(DisplayPage_t page);
bool USER_UI_DispatchPageKeyFromRegistry(DisplayPage_t page, Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_EnterPageFromRegistry(DisplayPage_t page);
void USER_UI_ExitPageFromRegistry(DisplayPage_t page);
DisplayPage_t USER_UI_GetAdjacentPageFromRegistry(DisplayPage_t current_page, bool forward);

/* ---- UI input/event adapters ---- */
USER_UI_KeyEvent_t USER_UI_ConvertButtonEvent(USER_LBB_ButtonEvent_t event);
USER_UI_KeyEvent_t USER_UI_ConsumeButtonEvent(Button_t button);
bool USER_UI_ConsumeAndDispatchButton(DisplayPage_t page, Button_t button);

/* ---- UI core state helpers ---- */
DisplayPage_t USER_UI_Core_GetCurrentPage(void);
USER_UI_ViewMode_t USER_UI_Core_GetViewMode(void);
void USER_UI_Core_Reset(void);
void USER_UI_Core_SetCurrentPage(DisplayPage_t page);
void USER_UI_Core_GotoAdjacentPage(bool forward);
void USER_UI_Core_MoveMainSelection(bool forward);
void USER_UI_Core_MoveSubSelection(bool forward);
void USER_UI_Core_OpenSelectedCategory(void);
void USER_UI_Core_OpenSelectedItem(void);
void USER_UI_Core_Back(void);
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
bool USER_UI_Route_IsWaitingRelease(void);
bool USER_UI_Route_IsCountdown(void);
uint8_t USER_UI_Route_GetPendingRoute(void);
uint16_t USER_UI_Route_GetCountdownRemainMs(void);
void USER_UI_Route_Service5ms(bool enter_is_pressed, uint16_t enter_press_time_ms);

/* ---- 页面绘制函数声明 ---- */
void USER_UI_ShowMotorPageStatic(void);
void USER_UI_ShowMotorPageDynamic(void);
void USER_UI_ShowEncoderPageStatic(void);
void USER_UI_ShowEncoderPageDynamic(void);
void USER_UI_EncoderOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ShowPhotoelectricPageStatic(void);
void USER_UI_ShowPhotoelectricPageDynamic(void);
void USER_UI_PhotoelectricOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ShowAdcPageStatic(void);
void USER_UI_ShowAdcPageDynamic(void);
void USER_UI_ShowLidarPageStatic(void);
void USER_UI_ShowLidarPageDynamic(void);
void USER_UI_LidarOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ShowGyroscopePageStatic(void);
void USER_UI_ShowGyroscopePageDynamic(void);
void USER_UI_GyroscopeOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ShowActuatorPageStatic(void);
void USER_UI_ShowActuatorPageDynamic(void);
void USER_UI_ActuatorOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ActuatorOnEnter(void);
void USER_UI_ActuatorOnExit(void);
void USER_UI_ShowThreadsPageStatic(void);
void USER_UI_ShowThreadsPageDynamic(void);
void USER_UI_ThreadsOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ThreadsOnEnter(void);
void USER_UI_ShowSysInfoPageStatic(void);
void USER_UI_ShowSysInfoPageDynamic(void);
void USER_UI_ShowCameraPageStatic(void);
void USER_UI_ShowCameraPageDynamic(void);
void USER_UI_ShowIMUSumPageStatic(void);
void USER_UI_ShowIMUSumPageDynamic(void);
void USER_UI_IMUSumOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_ShowRouteStatic(void);
void USER_UI_ShowRouteDynamic(void);
void USER_UI_RouteOnKey(Button_t key, USER_UI_KeyEvent_t event);
void USER_UI_RouteOnExit(void);
void USER_UI_ShowUnitTestStatic(void);
void USER_UI_ShowUnitTestDynamic(void);
void USER_UI_UnitTestOnKey(Button_t key, USER_UI_KeyEvent_t event);

#endif /* USER_UI_INTERNAL_H */
