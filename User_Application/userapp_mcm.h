#ifndef USERAPP_MCM_H
#define USERAPP_MCM_H

#include <stdbool.h>
#include <stdint.h>

#include "userlib_motor.h"
#include "userlib_pid.h"
#include "userapp_vehicle_model.h"

/* 运动控制标定参数，所有名称都带单位，便于后续按实车重新标定。 */
/* 直行动作轨迹规划参数。速度曲线采用梯形规划，单位均为物理量。 */
#define MCM_LINEAR_ACCEL_MM_S2 800.0f
#define MCM_LINEAR_DECEL_MM_S2 1000.0f
#define MCM_LINEAR_ACCEL_STEP_MM_S (MCM_LINEAR_ACCEL_MM_S2 * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f))
#define MCM_LINEAR_DECEL_STEP_MM_S (MCM_LINEAR_DECEL_MM_S2 * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f))
#define MCM_LINEAR_POSITION_KP 2.0f
#define MCM_LINEAR_DONE_DEADBAND_MM 5.0f
#define MCM_LINEAR_STOP_SPEED_MM_S 30.0f

/* 直线航向修正参数。若实车修正方向相反，将该增益改为负值即可。 */
#define MCM_STRAIGHT_HEADING_KP_MM_S_PER_DEG 2.0f

/* 旋转动作轨迹规划参数。角速度曲线采用梯形规划。 */
#define MCM_ROTATE_ACCEL_DEG_S2 360.0f
#define MCM_ROTATE_DECEL_DEG_S2 420.0f
#define MCM_ROTATE_ACCEL_STEP_DEG_S (MCM_ROTATE_ACCEL_DEG_S2 * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f))
#define MCM_ROTATE_DECEL_STEP_DEG_S (MCM_ROTATE_DECEL_DEG_S2 * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f))
#define MCM_ROTATE_ANGLE_KP 3.0f
#define MCM_ROTATE_DEADZONE_DEG 1.0f
#define MCM_ROTATE_STOP_GYRO_DEG_S 5.0f

/* 完成判据和安全保护参数。 */
#define MCM_FINISH_HOLD_CYCLES 5
#define MCM_MOVE_TIMEOUT_BASE_MS 2000u
#define MCM_ROTATE_TIMEOUT_BASE_MS 1500u
#define MCM_TIMEOUT_SCALE_PERCENT 250u
#define MCM_NO_PROGRESS_TIMEOUT_MS 500u
#define MCM_DISTANCE_PROGRESS_DEADBAND_MM 1.0f
#define MCM_ROTATE_PROGRESS_DEADBAND_DEG 0.2f

typedef enum
{
    USER_MCM_STATUS_OK = 0,        /* 动作正常完成 */
    USER_MCM_STATUS_BUSY,          /* 动作正在运行或 MCM 正忙 */
    USER_MCM_STATUS_PARAM_ERROR,   /* 输入参数非法 */
    USER_MCM_STATUS_TIMEOUT,       /* 动作总时长超时 */
    USER_MCM_STATUS_NO_PROGRESS,   /* 反馈量长时间没有有效进展 */
    USER_MCM_STATUS_ENCODER_ERROR, /* 编码器未初始化或溢出 */
    USER_MCM_STATUS_IMU_ERROR,     /* IMU 通信或数据状态异常 */
    USER_MCM_STATUS_CANCELED       /* 动作被上层取消 */
} USER_MCM_Status_t;

/**
 * @brief 当前 MCM 动作类型。
 */
typedef enum
{
    USER_MCM_ACTION_NONE = 0,
    USER_MCM_ACTION_STRAIGHT,
    USER_MCM_ACTION_SPIN,
    USER_MCM_ACTION_ARC
} USER_MCM_ActionType_t;

/**
 * @brief MCM 非阻塞状态机状态。
 */
typedef enum
{
    USER_MCM_STATE_IDLE = 0,
    USER_MCM_STATE_START,
    USER_MCM_STATE_RUN,
    USER_MCM_STATE_DONE,
    USER_MCM_STATE_FAULT
} USER_MCM_State_t;

/**
 * @brief 圆弧转向方向，后续圆弧动作使用。
 */
typedef enum
{
    USER_MCM_TURN_LEFT = 1,
    USER_MCM_TURN_RIGHT = -1
} USER_MCM_TurnDirection_t;

/**
 * @brief 圆弧行驶方向，后续圆弧动作使用。
 */
typedef enum
{
    USER_MCM_DRIVE_FORWARD = 1,
    USER_MCM_DRIVE_BACKWARD = -1
} USER_MCM_DriveDirection_t;

extern PID_Control_Struct_TypeDef speed_pid[2];
extern PID_Control_Struct_TypeDef distance_pid;
extern PID_Control_Struct_TypeDef angle_pid;

int32_t USER_MCM_DistanceToEncoderCount(int32_t distance_mm);
int32_t USER_MCM_EncoderCountToDistance(int32_t encoder_count);
int32_t USER_MCM_SpeedToEncoderCount(int32_t speed_mm_s);
int32_t USER_MCM_EncoderCountToSpeed(int32_t encoder_count_per_period);

int32_t USER_MCM_DistanceToOdometerEncoderCount(int32_t distance_mm);
int32_t USER_MCM_OdometerEncoderCountToDistance(int32_t odometer_count);
int32_t USER_MCM_SpeedToOdometerEncoderCount(int32_t speed_mm_s);
int32_t USER_MCM_OdometerEncoderCountToSpeed(int32_t odometer_count_per_period);

float USER_MCM_NormalizeAngle(float angle);
float USER_MCM_AngleDifference(float target, float current);

bool USER_MCM_IsIdle(void);
bool USER_MCM_IsBusy(void);
USER_MCM_Status_t USER_MCM_GetStatus(void);
USER_MCM_State_t USER_MCM_GetState(void);
USER_MCM_ActionType_t USER_MCM_GetActionType(void);

void USER_MCM_Task(void);
void USER_MCM_Cancel(void);

USER_MCM_Status_t USER_MCM_StartStraight(int32_t distance_mm, int32_t max_speed_mm_s);
USER_MCM_Status_t USER_MCM_StartSpin(int32_t relative_angle_deg, int32_t max_gyro_deg_s);
USER_MCM_Status_t USER_MCM_StartArc(int32_t radius_mm,
                                    int32_t arc_angle_deg,
                                    USER_MCM_TurnDirection_t turn_dir,
                                    USER_MCM_DriveDirection_t drive_dir,
                                    int32_t max_speed_mm_s,
                                    int32_t max_gyro_deg_s);
void USER_MCM_FreezeAllMotions(void);

#endif /* USERAPP_MCM_H */
