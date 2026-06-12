#ifndef USERAPP_RACE_H
#define USERAPP_RACE_H

/**
 * @file userapp_race.h
 * @brief 比赛路线启动和调度入口。
 *
 * 本模块负责接收路线启动请求、执行启动倒计时，并在周期任务中推进
 * 当前路线动作表。路线具体动作由 `userapp_race_table` 调度，运动控制由 MCM 执行。
 */

#include <stdbool.h>
#include <stdint.h>

#include "userapp_mcm.h"

/**
 * @brief 比赛路线编号。
 */
typedef enum
{
    RACE_ROUTE_TEMPLATE = 0,
    RACE_ROUTE_NONE = 255
} RaceRoute_t;

bool USER_Race_TemplatePath(void);
bool USER_Race_RunSelected(uint8_t race_route);
void USER_Race_RequestStart(uint8_t race_route);
void USER_Race_Task(void);

#endif /* USERAPP_RACE_H */
