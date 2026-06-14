#include "user_ui_menu.h"

#include "userlib_oled.h"

#define USER_UI_MENU_VISIBLE_ITEMS 6u
#define USER_UI_MENU_TEXT_WIDTH 21u

static const USER_UI_MenuItem_t ui_menu_run_route_items[] = {
    {"Fixed Route", PAGE_TEMPLATE, USER_UI_MENU_ITEM_ENABLED},
};

static const USER_UI_MenuItem_t ui_menu_auto_pilot_items[] = {
    {"Photo Pilot", 0, USER_UI_MENU_ITEM_PLACEHOLDER},
    {"Vision Pilot", 0, USER_UI_MENU_ITEM_PLACEHOLDER},
    {"Fusion Pilot", 0, USER_UI_MENU_ITEM_PLACEHOLDER},
};

static const USER_UI_MenuItem_t ui_menu_sensors_items[] = {
    {"Encoder Data", PAGE_ENCODER, USER_UI_MENU_ITEM_ENABLED},
    {"Photo Sensors", PAGE_PHOTOELECTRIC, USER_UI_MENU_ITEM_ENABLED},
    {"IMU Data", PAGE_GYROSCOPE, USER_UI_MENU_ITEM_ENABLED},
    {"IMU Sum Data", PAGE_IMU_SUM, USER_UI_MENU_ITEM_ENABLED},
    {"Smart Camera", PAGE_CAMERA, USER_UI_MENU_ITEM_ENABLED},
    {"ADC Data", PAGE_ADC, USER_UI_MENU_ITEM_ENABLED},
    {"LiDAR Sensors", PAGE_LIDAR, USER_UI_MENU_ITEM_ENABLED},
};

static const USER_UI_MenuItem_t ui_menu_service_items[] = {
    {"Hardware Test", PAGE_UNITTEST, USER_UI_MENU_ITEM_ENABLED},
    {"Motor PID Mon", PAGE_MOTOR, USER_UI_MENU_ITEM_ENABLED},
    {"Servo Manual", PAGE_SERVO, USER_UI_MENU_ITEM_ENABLED},
    {"Thread Stats", PAGE_THREADS, USER_UI_MENU_ITEM_ENABLED},
};

static const USER_UI_MenuItem_t ui_menu_setting_items[] = {
    {"Motor PID", 0, USER_UI_MENU_ITEM_PLACEHOLDER},
    {"Motion Params", 0, USER_UI_MENU_ITEM_PLACEHOLDER},
    {"Servo Params", 0, USER_UI_MENU_ITEM_PLACEHOLDER},
    {"System", 0, USER_UI_MENU_ITEM_PLACEHOLDER},
};

static const USER_UI_MenuCategory_t ui_menu_categories[] = {
    {UI_CATEGORY_RUN_ROUTE, "Run Route", ui_menu_run_route_items, (uint8_t)(sizeof(ui_menu_run_route_items) / sizeof(ui_menu_run_route_items[0]))},
    {UI_CATEGORY_AUTO_PILOT, "Auto Pilot", ui_menu_auto_pilot_items, (uint8_t)(sizeof(ui_menu_auto_pilot_items) / sizeof(ui_menu_auto_pilot_items[0]))},
    {UI_CATEGORY_SENSORS, "Sensors", ui_menu_sensors_items, (uint8_t)(sizeof(ui_menu_sensors_items) / sizeof(ui_menu_sensors_items[0]))},
    {UI_CATEGORY_SERVICE, "Service", ui_menu_service_items, (uint8_t)(sizeof(ui_menu_service_items) / sizeof(ui_menu_service_items[0]))},
    {UI_CATEGORY_SETTING, "Setting", ui_menu_setting_items, (uint8_t)(sizeof(ui_menu_setting_items) / sizeof(ui_menu_setting_items[0]))},
};

static void USER_UI_Menu_ClearLine(uint8_t row)
{
    USER_OLED_putString(row, 0u, "                     ", USER_UI_MENU_TEXT_WIDTH);
}

static void USER_UI_Menu_DrawLabel(uint8_t row, const char *label)
{
    USER_UI_Menu_ClearLine(row);
    USER_OLED_putString(row, 2u, label, 17u);
}

const USER_UI_MenuCategory_t *USER_UI_Menu_GetCategory(uint8_t category_index)
{
    if (category_index >= USER_UI_Menu_GetCategoryCount())
    {
        return NULL;
    }

    return &ui_menu_categories[category_index];
}

const USER_UI_MenuItem_t *USER_UI_Menu_GetItem(uint8_t category_index, uint8_t item_index)
{
    const USER_UI_MenuCategory_t *category = USER_UI_Menu_GetCategory(category_index);

    if ((category == NULL) || (item_index >= category->item_count))
    {
        return NULL;
    }

    return &category->items[item_index];
}

uint8_t USER_UI_Menu_GetCategoryCount(void)
{
    return (uint8_t)(sizeof(ui_menu_categories) / sizeof(ui_menu_categories[0]));
}

uint8_t USER_UI_Menu_GetItemCount(uint8_t category_index)
{
    const USER_UI_MenuCategory_t *category = USER_UI_Menu_GetCategory(category_index);

    return (category == NULL) ? 0u : category->item_count;
}

bool USER_UI_Menu_FindPage(DisplayPage_t page, uint8_t *category_index, uint8_t *item_index)
{
    uint8_t category;
    uint8_t item;

    for (category = 0u; category < USER_UI_Menu_GetCategoryCount(); category++)
    {
        const USER_UI_MenuCategory_t *category_def = USER_UI_Menu_GetCategory(category);
        if (category_def == NULL)
        {
            continue;
        }

        for (item = 0u; item < category_def->item_count; item++)
        {
            if ((category_def->items[item].flags & USER_UI_MENU_ITEM_ENABLED) != 0u &&
                category_def->items[item].page == page)
            {
                if (category_index != NULL)
                {
                    *category_index = category;
                }
                if (item_index != NULL)
                {
                    *item_index = item;
                }
                return true;
            }
        }
    }

    return false;
}

DisplayPage_t USER_UI_Menu_GetAdjacentPage(uint8_t category_index, DisplayPage_t page, bool forward)
{
    const USER_UI_MenuCategory_t *category = USER_UI_Menu_GetCategory(category_index);
    uint8_t current_item = 0u;
    uint8_t checked;

    if ((category == NULL) || (category->item_count == 0u))
    {
        return page;
    }

    (void)USER_UI_Menu_FindPage(page, NULL, &current_item);

    for (checked = 0u; checked < category->item_count; checked++)
    {
        if (forward)
        {
            current_item++;
            if (current_item >= category->item_count)
            {
                current_item = 0u;
            }
        }
        else
        {
            if (current_item == 0u)
            {
                current_item = (uint8_t)(category->item_count - 1u);
            }
            else
            {
                current_item--;
            }
        }

        if ((category->items[current_item].flags & USER_UI_MENU_ITEM_ENABLED) != 0u)
        {
            return category->items[current_item].page;
        }
    }

    return page;
}

void USER_UI_Menu_DrawMain(uint8_t selected_category)
{
    uint8_t i;

    USER_OLED_CleanScreen();
    USER_OLED_putString(0u, 0u, "MAIN MENU       ", 16u);
    USER_OLED_putUI16(0u, 16u, (uint16_t)(selected_category + 1u), 1u);
    USER_OLED_putString(0u, 17u, "/", 1u);
    USER_OLED_putUI16(0u, 18u, USER_UI_Menu_GetCategoryCount(), 1u);

    for (i = 0u; i < USER_UI_Menu_GetCategoryCount(); i++)
    {
        const USER_UI_MenuCategory_t *category = USER_UI_Menu_GetCategory(i);
        USER_UI_Menu_DrawLabel((uint8_t)(i + 1u), (category == NULL) ? "" : category->title);
        if (i == selected_category)
        {
            USER_OLED_putString((uint8_t)(i + 1u), 0u, ">", 1u);
        }
    }

    USER_OLED_putString(7u, 0u, "UP/DN   ENT:OPEN", 16u);
}

void USER_UI_Menu_DrawSub(uint8_t selected_category, uint8_t selected_item)
{
    const USER_UI_MenuCategory_t *category = USER_UI_Menu_GetCategory(selected_category);
    uint8_t item_count;
    uint8_t top = 0u;
    uint8_t row;

    USER_OLED_CleanScreen();
    if (category == NULL)
    {
        return;
    }

    item_count = category->item_count;
    if ((item_count > USER_UI_MENU_VISIBLE_ITEMS) &&
        (selected_item >= USER_UI_MENU_VISIBLE_ITEMS))
    {
        top = (uint8_t)(selected_item - USER_UI_MENU_VISIBLE_ITEMS + 1u);
    }

    USER_OLED_putString(0u, 0u, category->title, 13u);
    USER_OLED_putUI16(0u, 14u, (uint16_t)(selected_item + 1u), 1u);
    USER_OLED_putString(0u, 15u, "/", 1u);
    USER_OLED_putUI16(0u, 16u, item_count, 1u);

    for (row = 0u; row < USER_UI_MENU_VISIBLE_ITEMS; row++)
    {
        uint8_t item_index = (uint8_t)(top + row);
        uint8_t oled_row = (uint8_t)(row + 1u);
        const USER_UI_MenuItem_t *item = USER_UI_Menu_GetItem(selected_category, item_index);

        USER_UI_Menu_ClearLine(oled_row);
        if (item == NULL)
        {
            continue;
        }

        USER_OLED_putString(oled_row, 2u, item->label, 15u);
        if ((item->flags & USER_UI_MENU_ITEM_PLACEHOLDER) != 0u)
        {
            USER_OLED_putString(oled_row, 17u, "[--]", 4u);
        }
        if (item_index == selected_item)
        {
            USER_OLED_putString(oled_row, 0u, ">", 1u);
        }
    }

    if (top > 0u)
    {
        USER_OLED_putString(1u, 20u, "^", 1u);
    }
    if ((uint8_t)(top + USER_UI_MENU_VISIBLE_ITEMS) < item_count)
    {
        USER_OLED_putString(6u, 20u, "v", 1u);
    }

    USER_OLED_putString(7u, 0u, "ESC:BACK ENT:OPEN", 17u);
}

void USER_UI_Menu_DrawPlaceholder(uint8_t selected_category, uint8_t selected_item)
{
    const USER_UI_MenuItem_t *item = USER_UI_Menu_GetItem(selected_category, selected_item);

    USER_OLED_CleanScreen();
    if (item != NULL)
    {
        USER_OLED_putString(0u, 0u, item->label, 21u);
    }
    USER_OLED_putString(3u, 3u, "NOT IMPLEMENTED", 15u);
    USER_OLED_putString(7u, 0u, "ESC:BACK", 8u);
}
