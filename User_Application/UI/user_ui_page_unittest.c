#include "user_ui_internal.h"
#include "user_ui_unittest_actions.h"

#define UI_UNITTEST_VISIBLE_ROWS 6u

static uint8_t unittest_selected_index = 0u;
static uint8_t unittest_scroll_top = 0u;

static void USER_UI_UnitTest_ClampScroll(void)
{
    uint8_t item_count = USER_UI_UT_Action_GetItemCount();

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
    uint8_t item_count = USER_UI_UT_Action_GetItemCount();

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
        /* 通过动作层执行硬件操作，UI 页面不直接操作硬件 */
        (void)USER_UI_UT_Action_Execute(unittest_selected_index);
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
    uint8_t item_count = USER_UI_UT_Action_GetItemCount();
    char line_buf[22];

    /* 按键由 core 层通过 on_key() 回调统一分发，动态刷新函数不再直读按键 */

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
                           USER_UI_UT_Action_GetItemName(item_index),
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
    USER_OLED_putString(7u, 6u,
                        USER_UI_UT_Action_GetStatus(unittest_selected_index),
                        15u);
}
