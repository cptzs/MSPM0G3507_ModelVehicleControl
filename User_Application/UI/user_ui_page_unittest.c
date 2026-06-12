#include "user_ui_internal.h"

#define UI_UNITTEST_VISIBLE_ROWS 6u

static uint8_t unittest_selected_index = 0u;
static uint8_t unittest_scroll_top = 0u;
static const char *unittest_status = "Ready";

typedef struct
{
    const char *name;
    const char *status;
} USER_UI_UnitTestItem_t;

static const USER_UI_UnitTestItem_t unittest_items[] = {
    {"MTR FWD", "pending"},
    {"MTR REV", "pending"},
    {"MTR SPD0", "pending"},
    {"MTR STOP", "pending"},
    {"MTR COAST", "pending"},
    {"MTR BRAKE", "pending"},
    {"IMU HELLO", "pending"},
    {"USB HELLO", "pending"},
    {"CAM HELLO", "N/A"},
    {"CAN FRAME", "pending"},
    {"BUZZ 1MS", "pending"},
    {"LED 300MS", "pending"},
};

static uint8_t USER_UI_UnitTest_GetItemCount(void)
{
    return (uint8_t)(sizeof(unittest_items) / sizeof(unittest_items[0]));
}

static void USER_UI_UnitTest_ClampScroll(void)
{
    uint8_t item_count = USER_UI_UnitTest_GetItemCount();

    if (item_count <= UI_UNITTEST_VISIBLE_ROWS)
    {
        unittest_scroll_top = 0u;
        return;
    }

    if (unittest_selected_index < unittest_scroll_top)
    {
        unittest_scroll_top = unittest_selected_index;
    }
    else if (unittest_selected_index >= (uint8_t)(unittest_scroll_top + UI_UNITTEST_VISIBLE_ROWS))
    {
        unittest_scroll_top = (uint8_t)(unittest_selected_index - UI_UNITTEST_VISIBLE_ROWS + 1u);
    }
}

void USER_UI_UnitTestOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    uint8_t item_count = USER_UI_UnitTest_GetItemCount();

    if ((event == USER_UI_KEY_EVENT_NONE) || (item_count == 0u))
    {
        return;
    }

    if (key == UP)
    {
        if (unittest_selected_index == 0u)
        {
            unittest_selected_index = (uint8_t)(item_count - 1u);
        }
        else
        {
            unittest_selected_index--;
        }
        USER_UI_UnitTest_ClampScroll();
    }
    else if (key == DOWN)
    {
        unittest_selected_index++;
        if (unittest_selected_index >= item_count)
        {
            unittest_selected_index = 0u;
        }
        USER_UI_UnitTest_ClampScroll();
    }
    else if (key == ENTER)
    {
        unittest_status = unittest_items[unittest_selected_index].status;
    }
}

void USER_UI_ShowUnitTestStatic(void)
{
    USER_OLED_putString(1u, 0u, "UP/DN SEL ENTER RUN", 21u);
    USER_OLED_putString(2u, 0u, "                     ", 21u);
    USER_OLED_putString(3u, 0u, "                     ", 21u);
    USER_OLED_putString(4u, 0u, "                     ", 21u);
    USER_OLED_putString(5u, 0u, "                     ", 21u);
    USER_OLED_putString(6u, 0u, "                     ", 21u);
    USER_OLED_putString(7u, 0u, "Ready                ", 21u);
}

void USER_UI_ShowUnitTestDynamic(void)
{
    uint8_t row;
    uint8_t item_count = USER_UI_UnitTest_GetItemCount();
    char line_buf[22];

    if (USER_LBB_Button_ConsumeShort(UP) || USER_LBB_Button_ConsumeLongRepeat(UP))
    {
        USER_UI_UnitTestOnKey(UP, USER_UI_KEY_EVENT_SHORT);
    }

    if (USER_LBB_Button_ConsumeShort(DOWN) || USER_LBB_Button_ConsumeLongRepeat(DOWN))
    {
        USER_UI_UnitTestOnKey(DOWN, USER_UI_KEY_EVENT_SHORT);
    }

    if (USER_LBB_Button_ConsumeShort(ENTER))
    {
        USER_UI_UnitTestOnKey(ENTER, USER_UI_KEY_EVENT_SHORT);
    }

    USER_UI_UnitTest_ClampScroll();

    for (row = 0u; row < UI_UNITTEST_VISIBLE_ROWS; row++)
    {
        uint8_t item_index = (uint8_t)(unittest_scroll_top + row);
        uint8_t oled_row = (uint8_t)(row + 1u);

        if (item_index < item_count)
        {
            (void)snprintf(line_buf,
                           sizeof(line_buf),
                           "%c%-12s %02u/%02u",
                           (item_index == unittest_selected_index) ? '>' : ' ',
                           unittest_items[item_index].name,
                           (unsigned)(item_index + 1u),
                           (unsigned)item_count);
        }
        else
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%-21s", "");
        }

        USER_OLED_putString(oled_row, 0u, line_buf, 21u);
    }

    USER_OLED_putString(7u, 0u, "STAT:               ", 21u);
    USER_OLED_putString(7u, 6u, unittest_status, 15u);
}
