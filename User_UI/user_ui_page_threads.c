#include "user_ui_internal.h"

#include "user_os.h"
#include "userlib_oled.h"

#define USER_UI_THREADS_VISIBLE_ROWS 6u
#define USER_UI_THREADS_FIRST_DATA_ROW 2u
#define USER_UI_THREADS_LINE_LEN 21u
#define USER_UI_THREADS_COST_LIMIT_US 9999u

static uint8_t threads_scroll_top = 0u;
static bool threads_initial_max_cleared = false;

void USER_UI_ThreadsOnEnter(void)
{
    threads_scroll_top = 0u;

    if (!threads_initial_max_cleared)
    {
        USER_OS_ClearAllTaskMaxCost();
        threads_initial_max_cleared = true;
    }
}

void USER_UI_ThreadsOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    uint8_t total_tasks = USER_OS_GetTaskCount();
    uint8_t max_top;

    if ((key == ENTER) && (event == USER_UI_KEY_EVENT_LONG))
    {
        USER_OS_ClearAllTaskMaxCost();
        return;
    }

    if ((event == USER_UI_KEY_EVENT_NONE) ||
        ((key != UP) && (key != DOWN)))
    {
        return;
    }

    if (total_tasks <= USER_UI_THREADS_VISIBLE_ROWS)
    {
        threads_scroll_top = 0u;
        return;
    }

    max_top = (uint8_t)(total_tasks - USER_UI_THREADS_VISIBLE_ROWS);

    if (key == UP)
    {
        threads_scroll_top = (threads_scroll_top == 0u)
                               ? max_top
                               : (uint8_t)(threads_scroll_top - 1u);
    }
    else
    {
        threads_scroll_top = (threads_scroll_top >= max_top)
                               ? 0u
                               : (uint8_t)(threads_scroll_top + 1u);
    }
}

void USER_UI_ShowThreadsPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "TASK    L/us   M/us   ", USER_UI_THREADS_LINE_LEN);
}

void USER_UI_ShowThreadsPageDynamic(void)
{
    static uint8_t update_row = 0u;
    uint8_t total_tasks;
    uint8_t visible_count;
    uint8_t row;
    USER_OS_TaskStats_t stats;

    total_tasks = USER_OS_GetTaskCount();
    if (total_tasks == 0u)
    {
        USER_OLED_putString(USER_UI_THREADS_FIRST_DATA_ROW,
                            0u,
                            "No scheduler tasks  ",
                            USER_UI_THREADS_LINE_LEN);
        return;
    }

    visible_count = (total_tasks > USER_UI_THREADS_VISIBLE_ROWS)
                        ? USER_UI_THREADS_VISIBLE_ROWS
                        : total_tasks;

    update_row++;
    if (update_row >= visible_count)
    {
        update_row = 0u;
    }

    row = (uint8_t)(USER_UI_THREADS_FIRST_DATA_ROW + update_row);

    if (USER_OS_GetTaskStats((uint8_t)(threads_scroll_top + update_row), &stats))
    {
        if (stats.last_cost_us > USER_UI_THREADS_COST_LIMIT_US)
        {
            stats.last_cost_us = USER_UI_THREADS_COST_LIMIT_US;
        }
        if (stats.max_cost_us > USER_UI_THREADS_COST_LIMIT_US)
        {
            stats.max_cost_us = USER_UI_THREADS_COST_LIMIT_US;
        }

        USER_OLED_putString(row, 0u, "       L:     M:    ", USER_UI_THREADS_LINE_LEN);
        USER_OLED_putString(row,
                            0u,
                            (stats.name != NULL) ? stats.name : "?",
                            7u);
        USER_OLED_putUI16(row, 10u, (uint16_t)stats.last_cost_us, 4u);
        USER_OLED_putUI16(row, 17u, (uint16_t)stats.max_cost_us, 4u);
    }
}
