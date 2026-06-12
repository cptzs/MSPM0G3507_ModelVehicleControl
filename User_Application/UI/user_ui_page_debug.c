#include "user_ui_internal.h"
#include "user_os.h"

/**
 * @file user_ui_page_debug.c
 * @brief Threads 调试页：显示协作式调度器任务最近/最大运行耗时。
 *
 * 本页面是从 legacy user_ui.c 中拆出的第一批真实页面实现。
 * 当前仍复用 USER_OS_GetTaskCount()/USER_OS_GetTaskStats() 的显示窗口语义：
 * 当任务数量超过 OLED 可见行数时，OS stats 层负责根据 UP/DOWN 事件做窗口滚动。
 */

#define USER_UI_DEBUG_VISIBLE_ROWS 6u
#define USER_UI_DEBUG_FIRST_DATA_ROW 2u
#define USER_UI_DEBUG_LINE_LEN 21u
#define USER_UI_DEBUG_COST_LIMIT_US 9999u

void USER_UI_ShowDebugPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "TASK       L/us  M/us", USER_UI_DEBUG_LINE_LEN);
}

void USER_UI_ShowDebugPageDynamic(void)
{
    static uint8_t update_row = 0u;
    uint8_t task_count;
    uint8_t row;
    USER_OS_TaskStats_t stats;
    char line_buf[22];

    task_count = USER_OS_GetTaskCount();
    if (task_count == 0u)
    {
        USER_OLED_putString(USER_UI_DEBUG_FIRST_DATA_ROW, 0u, "No scheduler tasks  ", USER_UI_DEBUG_LINE_LEN);
        return;
    }

    if (task_count > USER_UI_DEBUG_VISIBLE_ROWS)
    {
        task_count = USER_UI_DEBUG_VISIBLE_ROWS;
    }

    update_row++;
    if (update_row >= task_count)
    {
        update_row = 0u;
    }

    row = (uint8_t)(USER_UI_DEBUG_FIRST_DATA_ROW + update_row);

    if (USER_OS_GetTaskStats(update_row, &stats))
    {
        if (stats.last_cost_us > USER_UI_DEBUG_COST_LIMIT_US)
        {
            stats.last_cost_us = USER_UI_DEBUG_COST_LIMIT_US;
        }
        if (stats.max_cost_us > USER_UI_DEBUG_COST_LIMIT_US)
        {
            stats.max_cost_us = USER_UI_DEBUG_COST_LIMIT_US;
        }

        (void)snprintf(line_buf,
                       sizeof(line_buf),
                       "%-8s L:%4u M:%4u",
                       (stats.name != NULL) ? stats.name : "?",
                       (uint16_t)stats.last_cost_us,
                       (uint16_t)stats.max_cost_us);
        USER_OLED_putString(row, 0u, line_buf, USER_UI_DEBUG_LINE_LEN);
    }
}
