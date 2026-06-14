#ifndef USER_UI_MENU_H
#define USER_UI_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "user_ui_internal.h"


typedef enum
{
    UI_CATEGORY_RUN_ROUTE = 0,
    UI_CATEGORY_AUTO_PILOT,
    UI_CATEGORY_SENSORS,
    UI_CATEGORY_SERVICE,
    UI_CATEGORY_SETTING,
    UI_CATEGORY_COUNT
} USER_UI_CategoryId_t;

#define USER_UI_MENU_ITEM_ENABLED (0x01u)
#define USER_UI_MENU_ITEM_PLACEHOLDER (0x02u)

typedef struct
{
    const char *label;
    DisplayPage_t page;
    uint8_t flags;
} USER_UI_MenuItem_t;

typedef struct
{
    USER_UI_CategoryId_t id;
    const char *title;
    const USER_UI_MenuItem_t *items;
    uint8_t item_count;
} USER_UI_MenuCategory_t;

const USER_UI_MenuCategory_t *USER_UI_Menu_GetCategory(uint8_t category_index);
const USER_UI_MenuItem_t *USER_UI_Menu_GetItem(uint8_t category_index, uint8_t item_index);
uint8_t USER_UI_Menu_GetCategoryCount(void);
uint8_t USER_UI_Menu_GetItemCount(uint8_t category_index);
DisplayPage_t USER_UI_Menu_GetAdjacentPage(uint8_t category_index, DisplayPage_t page, bool forward);
bool USER_UI_Menu_FindPage(DisplayPage_t page, uint8_t *category_index, uint8_t *item_index);

void USER_UI_Menu_DrawMain(uint8_t selected_category);
void USER_UI_Menu_DrawSub(uint8_t selected_category, uint8_t selected_item);
void USER_UI_Menu_DrawPlaceholder(uint8_t selected_category, uint8_t selected_item);

#endif /* USER_UI_MENU_H */
