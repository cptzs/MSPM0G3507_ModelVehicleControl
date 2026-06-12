#include "user_ui_internal.h"

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
