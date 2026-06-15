#ifndef USERAPP_RACE_TABLE_H
#define USERAPP_RACE_TABLE_H

/**
 * @file userapp_race_table.h
 * @brief 表格驱动的比赛路线调度器接口。
 *
 * 本模块位于比赛路线层和 MCM 层之间。路线表只描述“做什么”，
 * 调度器负责按行启动 MCM 非阻塞动作、等待动作完成、处理超时和跳转。
 *
 * 设计边界：
 * 1. 不直接写电机 PWM。
 * 2. 不直接写左右轮速度 PID 目标。
 * 3. 不读取或清零编码器、IMU 等原始传感器数据。
 * 4. 只通过 `USER_MCM_Start*()`、`USER_MCM_IsBusy()` 和 `USER_MCM_GetStatus()` 消费 MCM。
 */

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 路线表动作类型。
 */
typedef enum
{
    USER_Race_ACTION_NONE = 0,
    USER_Race_ACTION_MOVE_DISTANCE,
    USER_Race_ACTION_ROTATE_ANGLE,
    USER_Race_ACTION_ARC,
    USER_Race_ACTION_WAIT_MS,
    USER_Race_ACTION_STOP,
    USER_Race_ACTION_END
} USER_Race_ActionType_t;

#define USER_RACE_ACTION_FLAG_TURN_RIGHT 0x0001u
#define USER_RACE_ACTION_FLAG_DRIVE_BACKWARD 0x0002u

/**
 * @brief 路线表单行动作定义。
 */
typedef struct
{
    USER_Race_ActionType_t action_type; /* 动作类型。 */
    float param1;                       /* 主参数：距离 mm、角度 deg 或等待时间 ms。 */
    float param2;                       /* 次参数：最大速度 mm/s 或最大角速度 deg/s。 */
    uint32_t timeout_ms;                /* 动作超时时间，单位 ms；0 表示不启用表层超时。 */
    int16_t next_index;                 /* 成功后的下一行动作索引；-1 表示路线结束。 */
    uint16_t flags;                     /* 预留扩展标志，当前填 0。 */
} USER_Race_Action_t;

/**
 * @brief 路线表执行器状态。
 */
typedef enum
{
    USER_Race_TABLE_STATE_IDLE = 0,
    USER_Race_TABLE_STATE_RUNNING,
    USER_Race_TABLE_STATE_WAIT_ACTION_DONE,
    USER_Race_TABLE_STATE_DONE,
    USER_Race_TABLE_STATE_ERROR
} USER_Race_TableState_t;

/**
 * @brief 路线表执行结果。
 */
typedef enum
{
    USER_Race_TABLE_RESULT_NONE = 0,
    USER_Race_TABLE_RESULT_OK,
    USER_Race_TABLE_RESULT_ACTION_FAILED,
    USER_Race_TABLE_RESULT_TIMEOUT,
    USER_Race_TABLE_RESULT_INVALID_INDEX,
    USER_Race_TABLE_RESULT_INVALID_ACTION,
    USER_Race_TABLE_RESULT_ABORTED
} USER_Race_TableResult_t;

/**
 * @brief 路线表执行器上下文。
 */
typedef struct
{
    const USER_Race_Action_t *table;
    uint16_t table_size;
    int16_t current_index;
    bool action_started;
    USER_Race_TableState_t state;
    USER_Race_TableResult_t result;
    uint32_t action_elapsed_ms;
    uint32_t scheduler_period_ms;
} USER_Race_TableExecutor_t;

void USER_Race_TableExecutor_Init(USER_Race_TableExecutor_t *executor,
                                  uint32_t scheduler_period_ms);

bool USER_Race_TableExecutor_Start(USER_Race_TableExecutor_t *executor,
                                   const USER_Race_Action_t *table,
                                   uint16_t table_size);

void USER_Race_TableExecutor_Update(USER_Race_TableExecutor_t *executor);
void USER_Race_TableExecutor_Stop(USER_Race_TableExecutor_t *executor);

bool USER_Race_TableExecutor_IsBusy(const USER_Race_TableExecutor_t *executor);
bool USER_Race_TableExecutor_IsDone(const USER_Race_TableExecutor_t *executor);

USER_Race_TableState_t USER_Race_TableExecutor_GetState(const USER_Race_TableExecutor_t *executor);
USER_Race_TableResult_t USER_Race_TableExecutor_GetResult(const USER_Race_TableExecutor_t *executor);
int16_t USER_Race_TableExecutor_GetCurrentIndex(const USER_Race_TableExecutor_t *executor);

#endif /* USERAPP_RACE_TABLE_H */
