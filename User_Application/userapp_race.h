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
#include "userapp_race_table.h"

/**
 * @brief 比赛路线编号。
 */
typedef enum
{
    RACE_ROUTE_TEMPLATE = 0,
    RACE_ROUTE_NONE = 255
} RaceRoute_t;

/**
 * @brief 获取路线显示名称。
 *
 * @param race_route `RaceRoute_t` 路线编号。
 *
 * @return 路线名称字符串；未知路线返回 `Unknown`。
 */
const char *USER_Race_GetRouteName(uint8_t race_route);

/**
 * @brief 获取当前正在执行的路线动作信息。
 *
 * @param action_ptr 输出当前动作内容；允许传入 NULL。
 * @param step_index_ptr 输出当前步骤索引，从 0 开始；允许传入 NULL。
 * @param remain_timeout_ms_ptr 输出当前动作表层超时剩余时间，单位 ms；允许传入 NULL。
 *
 * @return true 表示当前存在有效动作；false 表示路线未运行、已结束或当前索引无效。
 *
 * @note 本接口只读，不会改变路线执行器状态，主要供 UI 页面显示当前步骤和倒计时。
 */
bool USER_Race_GetCurrentAction(USER_Race_Action_t *action_ptr,
                                int16_t *step_index_ptr,
                                uint32_t *remain_timeout_ms_ptr);

/**
 * @brief 获取模板路线第一步动作，供路线页面在未启动时显示预览。
 *
 * @param action_ptr 输出第一步动作内容；允许传入 NULL。
 * @param remain_timeout_ms_ptr 输出第一步超时时间，单位 ms；允许传入 NULL。
 *
 * @return true 表示预览动作有效。
 */
bool USER_Race_GetTemplatePreviewAction(USER_Race_Action_t *action_ptr,
                                        uint32_t *remain_timeout_ms_ptr);

bool USER_Race_TemplatePath(void);
bool USER_Race_RunSelected(uint8_t race_route);
void USER_Race_RequestStart(uint8_t race_route);
void USER_Race_Task(void);

#endif /* USERAPP_RACE_H */
