/**
 * @file user_ui_page_unittest.c
 * @brief UnitTest 页面 — 测试项列表选择与状态显示。
 *
 * 通过注册表 on_key() 回调处理 UP/DOWN 选择和 ENTER 执行。
 * 硬件操作委托给 user_ui_unittest_actions 层，UI 页面仅负责列表渲染。
 */

#include "user_ui_internal.h"
#include "user_ui_unittest_actions.h"

#include "userlib_oled.h"

/** @brief OLED 可见行数（第 1~6 行，第 7 行为状态栏） */
#define UI_UNITTEST_VISIBLE_ROWS 6u

/** @brief 当前高亮选中项索引 */
static uint8_t unittest_selected_index = 0u;
/** @brief 滚动窗口顶部索引 */
static uint8_t unittest_scroll_top = 0u;

static void USER_UI_UnitTest_Write2Digits(char *dst, uint8_t value)
{
    uint8_t tens = 0u;

    if (value > 99u)
    {
        value = 99u;
    }

    while (value >= 10u)
    {
        value = (uint8_t)(value - 10u);
        tens++;
    }

    dst[0] = (char)('0' + tens);
    dst[1] = (char)('0' + value);
}

static void USER_UI_UnitTest_BuildLine(char *line_buf,
                                       uint8_t item_index,
                                       uint8_t item_count,
                                       bool selected)
{
    const char *name;
    uint8_t i;

    for (i = 0u; i < 21u; i++)
    {
        line_buf[i] = ' ';
    }
    line_buf[21] = '\0';

    line_buf[0] = selected ? '>' : ' ';
    name = USER_UI_UnitTestAction_GetItemName(item_index);
    for (i = 0u; (i < 12u) && (name != NULL) && (name[i] != '\0'); i++)
    {
        line_buf[(uint8_t)(1u + i)] = name[i];
    }

    USER_UI_UnitTest_Write2Digits(&line_buf[14], (uint8_t)(item_index + 1u));
    line_buf[16] = '/';
    USER_UI_UnitTest_Write2Digits(&line_buf[17], item_count);
}

/**
 * @brief 根据选中项位置自动调整滚动窗口，确保选中项始终可见。
 */
static void USER_UI_UnitTest_ClampScroll(void)
{
    uint8_t item_count = USER_UI_UnitTestAction_GetItemCount();

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
    uint8_t item_count = USER_UI_UnitTestAction_GetItemCount();

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
        (void)USER_UI_UnitTest_ExecuteAction(unittest_selected_index);
    }
}

/**
 * @brief 绘制 UnitTest 页面静态布局（操作提示 + 空列表占位 + 状态栏）。
 */
void USER_UI_ShowUnitTestStatic(void)
{
    USER_OLED_PutString(1u, 0u, "UP/DN SEL ENTER RUN", 21u);
    USER_OLED_PutString(2u, 0u, "                     ", 21u);
    USER_OLED_PutString(3u, 0u, "                     ", 21u);
    USER_OLED_PutString(4u, 0u, "                     ", 21u);
    USER_OLED_PutString(5u, 0u, "                     ", 21u);
    USER_OLED_PutString(6u, 0u, "                     ", 21u);
    USER_OLED_PutString(7u, 0u, "Ready                ", 21u);
}

/**
 * @brief UnitTest 页面动态刷新 — 重绘全部可见行（列表项 + 状态栏）。
 *
 * @note 与逐行轮询页面不同，本页每帧重绘全部 6 行以保证选中高亮即时更新。
 */
void USER_UI_ShowUnitTestDynamic(void)
{
    uint8_t row;
    uint8_t item_count = USER_UI_UnitTestAction_GetItemCount();
    char line_buf[22];

    /* 按键由 core 层通过 on_key() 回调统一分发，动态刷新函数不再直读按键 */

    USER_UI_UnitTest_ClampScroll();

    for (row = 0u; row < UI_UNITTEST_VISIBLE_ROWS; row++)
    {
        uint8_t item_index = (uint8_t)(unittest_scroll_top + row);
        uint8_t oled_row = (uint8_t)(row + 1u);

        if (item_index < item_count)
        {
            USER_UI_UnitTest_BuildLine(line_buf,
                                       item_index,
                                       item_count,
                                       (item_index == unittest_selected_index));
        }
        else
        {
            uint8_t i;
            for (i = 0u; i < 21u; i++)
            {
                line_buf[i] = ' ';
            }
            line_buf[21] = '\0';
        }

        USER_OLED_PutString(oled_row, 0u, line_buf, 21u);
    }

    USER_OLED_PutString(7u, 0u, "STAT:               ", 21u);
    USER_OLED_PutString(7u, 6u,
                        USER_UI_UnitTestAction_GetStatus(unittest_selected_index),
                        15u);
}
