/**
 * @file user_ui_dispatch.c
 * @brief 注册表驱动的页面分发器。
 *
 * 提供基于 ui_page_table[] 的页面查找、静态/动态绘制委托、
 * 按键事件分发和相邻页面导航的统一入口。
 */

#include "user_ui_internal.h"

/**
 * @brief 在注册表中查找指定页面的数组索引。
 *
 * @param page 目标页面枚举值。
 * @return 数组索引（0-based），未找到时返回 0（回退到首页）。
 */
static uint8_t USER_UI_FindPageIndex(DisplayPage_t page)
{
    uint8_t i;

    for (i = 0u; i < USER_UI_GetPageCount(); i++)
    {
        const USER_UI_PageDef_t *page_def = USER_UI_GetPageByIndex(i);
        if ((page_def != NULL) && (page_def->page == page))
        {
            return i;
        }
    }

    return 0u;
}

/**
 * @brief 通过注册表绘制指定页面的静态内容。
 *
 * @param page 目标页面枚举值。
 * @retval true  绘制成功。
 * @retval false 页面未注册。
 *
 * @note 自动绘制标题行（[XX] Title），然后调用页面的 show_static 回调。
 */
bool USER_UI_DrawPageStaticFromRegistry(DisplayPage_t page)
{
    const USER_UI_PageDef_t *page_def = USER_UI_FindPage(page);

    if (page_def == NULL)
    {
        return false;
    }

    USER_OLED_putString(0u, 0u, "[", 1u);
    USER_OLED_putUI16(0u, 1u, page_def->page, 2u);
    USER_OLED_putString(0u, 3u, "] ", 2u);
    USER_OLED_putString(0u, 5u, page_def->title, 13u);

    if (page_def->show_static != NULL)
    {
        page_def->show_static();
    }

    return true;
}

/**
 * @brief 通过注册表绘制指定页面的动态内容。
 *
 * @param page 目标页面枚举值。
 * @retval true  绘制成功。
 * @retval false 页面未注册。
 *
 * @note 调用页面的 show_dynamic 回调，各页面自行实现逐行轮询刷新。
 */
bool USER_UI_DrawPageDynamicFromRegistry(DisplayPage_t page)
{
    const USER_UI_PageDef_t *page_def = USER_UI_FindPage(page);

    if (page_def == NULL)
    {
        return false;
    }

    if (page_def->show_dynamic != NULL)
    {
        page_def->show_dynamic();
    }

    return true;
}

/**
 * @brief 通过注册表将按键事件分发到指定页面的 on_key() 回调。
 *
 * @param page  目标页面枚举值。
 * @param key   物理按钮枚举值。
 * @param event UI 按键事件类型。
 * @retval true  分发成功。
 * @retval false 页面未注册、无回调或事件为 NONE。
 */
bool USER_UI_DispatchPageKeyFromRegistry(DisplayPage_t page, Button_t key, USER_UI_KeyEvent_t event)
{
    const USER_UI_PageDef_t *page_def = USER_UI_FindPage(page);

    if ((page_def == NULL) || (page_def->on_key == NULL) || (event == USER_UI_KEY_EVENT_NONE))
    {
        return false;
    }

    page_def->on_key(key, event);
    return true;
}

/**
 * @brief 获取注册表中当前页的前一页或后一页（循环）。
 *
 * @param current_page 当前页面枚举值。
 * @param forward      true = 下一页，false = 上一页。
 * @return 相邻页面枚举值，注册表为空时返回 PAGE_MOTOR。
 */
DisplayPage_t USER_UI_GetAdjacentPageFromRegistry(DisplayPage_t current_page, bool forward)
{
    uint8_t page_count = USER_UI_GetPageCount();
    uint8_t index;

    if (page_count == 0u)
    {
        return PAGE_MOTOR;
    }

    index = USER_UI_FindPageIndex(current_page);

    if (forward)
    {
        index++;
        if (index >= page_count)
        {
            index = 0u;
        }
    }
    else
    {
        if (index == 0u)
        {
            index = (uint8_t)(page_count - 1u);
        }
        else
        {
            index--;
        }
    }

    return USER_UI_GetPageByIndex(index)->page;
}
