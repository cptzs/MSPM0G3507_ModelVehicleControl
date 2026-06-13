/**
 * @file user_ui_core_state.c
 * @brief UI 核心状态管理 — 当前页面 + 静态脏标志维护。
 *
 * 提供当前激活页面的读写接口及静态内容重绘控制。
 * 所有页面切换统一经过本模块，确保 OLED 清屏和脏标志同步更新。
 */

#include "user_ui_internal.h"
#include "userlib_oled.h"

/** @brief 当前激活的 OLED 显示页面 */
static DisplayPage_t ui_current_page = PAGE_MOTOR;
/** @brief 静态内容脏标志（页面切换或显式标记时置位） */
static bool ui_static_dirty = true;
static uint8_t ui_dynamic_tick = 0u;

/**
 * @brief 获取当前激活页面。
 *
 * @return 当前页面的 DisplayPage_t 枚举值。
 */
DisplayPage_t USER_UI_Core_GetCurrentPage(void)
{
    return ui_current_page;
}

void USER_UI_Core_Reset(void)
{
    ui_current_page = PAGE_MOTOR;
    ui_static_dirty = true;
    ui_dynamic_tick = 0u;
    USER_OLED_CleanScreen();
    USER_UI_EnterPageFromRegistry(ui_current_page);
}

/**
 * @brief 切换到指定页面。
 *
 * @param page 目标页面枚举值，若不在注册表中则回退到 PAGE_MOTOR。
 *
 * @note 页面切换时自动清屏并置位静态脏标志，触发下一帧重绘标题和静态内容。
 */
void USER_UI_Core_SetCurrentPage(DisplayPage_t page)
{
    if (USER_UI_FindPage(page) == NULL)
    {
        page = PAGE_MOTOR;
    }

    if (ui_current_page != page)
    {
        USER_UI_ExitPageFromRegistry(ui_current_page);
        ui_current_page = page;
        ui_static_dirty = true;
        ui_dynamic_tick = 0u;
        USER_OLED_CleanScreen();
        USER_UI_EnterPageFromRegistry(ui_current_page);
    }
}

/**
 * @brief 切换到注册表中当前页的前一页或后一页（循环）。
 *
 * @param forward true = 下一页，false = 上一页。
 */
void USER_UI_Core_GotoAdjacentPage(bool forward)
{
    USER_UI_Core_SetCurrentPage(USER_UI_GetAdjacentPageFromRegistry(ui_current_page, forward));
}

/**
 * @brief 查询静态内容是否需要重绘。
 *
 * @return true  静态内容已脏，需重绘。
 * @return false 静态内容干净。
 */
bool USER_UI_Core_IsStaticDirty(void)
{
    return ui_static_dirty;
}

/**
 * @brief 标记静态内容为脏（下次 RedrawStaticIfNeeded 将触发重绘）。
 */
void USER_UI_Core_MarkStaticDirty(void)
{
    ui_static_dirty = true;
}

/**
 * @brief 清除静态脏标志（重绘完成后调用）。
 */
void USER_UI_Core_ClearStaticDirty(void)
{
    ui_static_dirty = false;
}

/**
 * @brief 若静态内容脏则执行重绘并清除标志。
 *
 * @note 通过注册表查找当前页面的 show_static 回调执行绘制，
 *       绘制内容包括页面标题行（[XX] Title）和静态布局。
 */
void USER_UI_Core_RedrawStaticIfNeeded(void)
{
    if (ui_static_dirty)
    {
        (void)USER_UI_DrawPageStaticFromRegistry(ui_current_page);
        ui_static_dirty = false;
    }
}

/**
 * @brief 绘制当前页面的动态内容（逐行轮询刷新）。
 *
 * @note 通过注册表查找当前页面的 show_dynamic 回调，
 *       各页面自行实现逐行轮询策略以降低 OLED SPI 瞬时负载。
 */
void USER_UI_Core_DrawCurrentDynamic(void)
{
    const USER_UI_PageDef_t *page_def = USER_UI_FindPage(ui_current_page);
    uint8_t divider = 1u;

    if ((page_def != NULL) && (page_def->refresh_divider > 0u))
    {
        divider = page_def->refresh_divider;
    }

    if (ui_dynamic_tick == 0u)
    {
        (void)USER_UI_DrawPageDynamicFromRegistry(ui_current_page);
    }

    ui_dynamic_tick++;
    if (ui_dynamic_tick >= divider)
    {
        ui_dynamic_tick = 0u;
    }
}
