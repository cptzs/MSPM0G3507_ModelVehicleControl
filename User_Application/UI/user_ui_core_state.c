#include "user_ui_internal.h"

static DisplayPage_t ui_current_page = PAGE_MOTOR;
static bool ui_static_dirty = true;

DisplayPage_t USER_UI_Core_GetCurrentPage(void)
{
    return ui_current_page;
}

void USER_UI_Core_SetCurrentPage(DisplayPage_t page)
{
    if (USER_UI_FindPage(page) == NULL)
    {
        page = PAGE_MOTOR;
    }

    if (ui_current_page != page)
    {
        ui_current_page = page;
        ui_static_dirty = true;
        USER_OLED_CleanScreen();
    }
}

void USER_UI_Core_GotoAdjacentPage(bool forward)
{
    USER_UI_Core_SetCurrentPage(USER_UI_GetAdjacentPageFromRegistry(ui_current_page, forward));
}

bool USER_UI_Core_IsStaticDirty(void)
{
    return ui_static_dirty;
}

void USER_UI_Core_MarkStaticDirty(void)
{
    ui_static_dirty = true;
}

void USER_UI_Core_ClearStaticDirty(void)
{
    ui_static_dirty = false;
}

void USER_UI_Core_RedrawStaticIfNeeded(void)
{
    if (ui_static_dirty)
    {
        (void)USER_UI_DrawPageStaticFromRegistry(ui_current_page);
        ui_static_dirty = false;
    }
}

void USER_UI_Core_DrawCurrentDynamic(void)
{
    (void)USER_UI_DrawPageDynamicFromRegistry(ui_current_page);
}
