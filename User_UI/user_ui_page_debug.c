#include "user_ui_internal.h"
#include "user_os.h"
#include "userlib_lbb.h"

/**
 * @file user_ui_page_debug.c
 * @brief Threads 调试页 — 显示协作式调度器各任务最近/最大运行耗时 (μs)。
 *
 * 当任务数量超过 OLED 可见行数 (6 行) 时，本模块通过 UP / DOWN 按键
 * 消费事件自行管理滚动窗口。OS 层对外提供纯数据接口（总任务数 + 按绝对
 * ID 查询统计信息），不再耦合任何 UI 滚动状态。
 */

#define USER_UI_DEBUG_VISIBLE_ROWS 6u
#define USER_UI_DEBUG_FIRST_DATA_ROW 2u
#define USER_UI_DEBUG_LINE_LEN 21u
#define USER_UI_DEBUG_COST_LIMIT_US 9999u

/*----------------------------------------------------------------------------
 * 滚动窗口状态（仅本 UI 页使用，与 OS 层无关）
 *----------------------------------------------------------------------------*/
static uint8_t debug_scroll_top = 0u;

/**
 * @brief 处理线程统计页的上下滚动事件。
 *
 * 消费 UP / DOWN 按钮事件，更新 debug_scroll_top 以实现多任务翻页。
 * 滚动循环：到顶再按 UP 跳到底部，到底再按 DOWN 跳到顶部。
 *
 * @note 本函数应在每次 Dynamic 刷新前调用，确保按键事件在 UI 帧边界
 *       被及时消费。
 */
static void USER_UI_DebugUpdateScroll(uint8_t total_tasks)
{
    uint8_t max_top;

    if (total_tasks <= USER_UI_DEBUG_VISIBLE_ROWS)
    {
        debug_scroll_top = 0u;
        return;
    }

    max_top = (uint8_t)(total_tasks - USER_UI_DEBUG_VISIBLE_ROWS);

    /* UP 按键：向上滚动一行，到顶时回绕到底部 */
    {
        USER_LBB_ButtonEvent_t event = USER_LBB_Button_ConsumeEvent(UP);
        if ((event == USER_LBB_BUTTON_EVENT_SHORT) ||
            (event == USER_LBB_BUTTON_EVENT_LONG) ||
            (event == USER_LBB_BUTTON_EVENT_LONG_REPEAT))
        {
            if (debug_scroll_top == 0u)
            {
                debug_scroll_top = max_top;
            }
            else
            {
                debug_scroll_top--;
            }
        }
    }

    /* DOWN 按键：向下滚动一行，到底时回绕到顶部 */
    {
        USER_LBB_ButtonEvent_t event = USER_LBB_Button_ConsumeEvent(DOWN);
        if ((event == USER_LBB_BUTTON_EVENT_SHORT) ||
            (event == USER_LBB_BUTTON_EVENT_LONG) ||
            (event == USER_LBB_BUTTON_EVENT_LONG_REPEAT))
        {
            if (debug_scroll_top >= max_top)
            {
                debug_scroll_top = 0u;
            }
            else
            {
                debug_scroll_top++;
            }
        }
    }
}

/**
 * @brief 显示调试页面的静态内容。
 */
void USER_UI_ShowDebugPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "TASK       L/us  M/us", USER_UI_DEBUG_LINE_LEN);
}

/**
 * @brief 显示调试页面的动态内容。
 *
 * 每次调用更新一行，显示当前滚动窗口内对应任务的名称、最近耗时和最大耗时。
 * 当任务数超过可见行数时，UP / DOWN 按键控制滚动窗口位置。
 *
 * @note 该函数应每 100ms 调用一次，确保界面响应及时且 OLED 刷新压力可控。
 */
void USER_UI_ShowDebugPageDynamic(void)
{
    static uint8_t update_row = 0u;
    uint8_t total_tasks;
    uint8_t visible_count;
    uint8_t row;
    USER_OS_TaskStats_t stats;
    char line_buf[22];

    /* 获取总任务数，处理滚动事件，计算可见窗口 */
    total_tasks = USER_OS_GetTaskCount();
    USER_UI_DebugUpdateScroll(total_tasks);

    if (total_tasks == 0u)
    {
        USER_OLED_putString(USER_UI_DEBUG_FIRST_DATA_ROW, 0u, "No scheduler tasks  ", USER_UI_DEBUG_LINE_LEN);
        return;
    }

    visible_count = (total_tasks > USER_UI_DEBUG_VISIBLE_ROWS)
                        ? USER_UI_DEBUG_VISIBLE_ROWS
                        : total_tasks;

    /* 逐行刷新：每次调用只更新一行，降低 OLED SPI 刷新压力 */
    update_row++;
    if (update_row >= visible_count)
    {
        update_row = 0u;
    }

    row = (uint8_t)(USER_UI_DEBUG_FIRST_DATA_ROW + update_row);

    /* 使用绝对任务 ID（scroll_top + 行内偏移）查询 OS 统计 */
    if (USER_OS_GetTaskStats((uint8_t)(debug_scroll_top + update_row), &stats))
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
