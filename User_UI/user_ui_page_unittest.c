/**
 * @file user_ui_page_unittest.c
 * @brief UnitTest 页面 — 测试项列表选择与状态显示。
 *
 * 通过注册表 on_key() 回调处理 UP/DOWN 选择和 ENTER 执行。
 * 硬件操作委托给 user_ui_unittest_actions 层，UI 页面仅负责列表渲染。
 */

#include "user_ui_internal.h"
#include "user_ui_unittest_actions.h"

/** @brief OLED 可见行数（第 1~6 行，第 7 行为状态栏） */
#define UI_UNITTEST_VISIBLE_ROWS 6u

/** @brief 当前高亮选中项索引 */
static uint8_t unittest_selected_index = 0u;
/** @brief 滚动窗口顶部索引 */
static uint8_t unittest_scroll_top = 0u;

/**
 * @brief 根据选中项位置自动调整滚动窗口，确保选中项始终可见。
 */
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

/**
 * @brief UnitTest 页面按键处理 — UP/DOWN 移动选择，ENTER 执行当前测试项。
 */
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

/**
 * @brief 绘制 UnitTest 页面静态布局（操作提示 + 空列表占位 + 状态栏）。
 */
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

/**
 * @brief UnitTest 页面动态刷新 — 重绘全部可见行（列表项 + 状态栏）。
 *
 * @note 与逐行轮询页面不同，本页每帧重绘全部 6 行以保证选中高亮即时更新。
 */
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
