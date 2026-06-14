#include "userapp_mcm.h"

#include "userapp_state_estimator.h"
#include "userlib_systick.h"

/**
 * @file userapp_mcm.c
 * @brief 非阻塞运动控制管理器。
 *
 * 本模块每个控制周期执行一次运动状态机。它只消费状态估测层发布的
 * `USER_STATE_Estimate_t`，不直接读取 IMU 或编码器原始全局变量。
 *
 * 当前实现阶段：
 * 1. 支持非阻塞直行前进/后退。
 * 2. 支持非阻塞原地自旋。
 * 3. 预留圆弧动作启动入口，后续阶段实现。
 * 4. MCM 拥有速度 PID 目标和电机输出写入权。
 */

/* ---------- 内部辅助宏 ---------- */

/** @brief 32 位有符号整数绝对值 */
#define MCM_ABS_I32(value) (((value) < 0) ? -(value) : (value))
/** @brief 单精度浮点绝对值 */
#define MCM_ABS_F(value) (((value) < 0.0f) ? -(value) : (value))
/** @brief 32 位有符号整数符号（-1 / 1） */
#define MCM_SIGN_I32(value) (((value) < 0) ? -1 : 1)
/** @brief 从起始 tick 计算已流逝毫秒数 */
#define MCM_ELAPSED_MS(start_tick) ((uint32_t)(sysTick - (start_tick)))
/** @brief 线速度 → 单周期编码器计数 */
#define MCM_SPEED_TO_ENCODER_COUNT(speed_mm_s) ((int32_t)((speed_mm_s) * VEHICLE_ENCODER_PULSE_PER_CONTROL_PERIOD))
/** @brief 根据行程量和最大速率计算超时阈值（含安全余量系数） */
#define MCM_TIMEOUT_MS(amount, max_rate, base_ms) \
    ((uint32_t)((((uint64_t)MCM_ABS_I32(amount) * 1000u) / (uint32_t)(max_rate)) * MCM_TIMEOUT_SCALE_PERCENT / 100u) + (base_ms))

/** @brief 浮点数限幅到 [min_value, max_value] */
#define MCM_CLAMP_F(value, min_value, max_value) \
    do                                           \
    {                                            \
        if ((value) > (max_value))               \
        {                                        \
            (value) = (max_value);               \
        }                                        \
        else if ((value) < (min_value))          \
        {                                        \
            (value) = (min_value);               \
        }                                        \
    } while (0)

/**
 * @brief MCM 内部非阻塞动作上下文。
 *
 * 所有字段均为 MCM 私有状态。启动动作时快照估测层输出，运行期间通过估测层
 * 最新输出计算相对距离、相对航向、速度和完成条件。
 */
typedef struct
{
    USER_MCM_ActionType_t type;
    USER_MCM_State_t state;
    USER_MCM_Status_t status;

    int32_t drive_dir;
    int32_t spin_dir;

    float target_abs;
    float target_signed;
    float max_rate;

    float start_distance_mm;
    float start_x_mm;
    float start_y_mm;
    float start_yaw_deg;

    float relative_distance_mm;
    float relative_yaw_deg;
    float progress;
    float remaining;

    float profile_rate;
    float reference_progress;
    float last_progress;

    uint8_t heading_hold_enabled;
    uint8_t finish_hold_count;
    uint32_t start_tick;
    uint32_t progress_tick;
    uint32_t timeout_ms;
} USER_MCM_Context_t;

static USER_MCM_Context_t mcm = {
    USER_MCM_ACTION_NONE,
    USER_MCM_STATE_IDLE,
    USER_MCM_STATUS_OK,
    1,
    1,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0u,
    0u,
    0u,
    0u,
    0u};

/**
 * @brief 将车轮行驶距离换算为车轮编码器累计计数。
 * @param distance_mm 距离，单位 mm，可为负数。
 * @return 车轮编码器计数，单位 count。
 */
int32_t USER_MCM_DistanceToEncoderCount(int32_t distance_mm)
{
    return (int32_t)(distance_mm * VEHICLE_ENCODER_PULSE_PER_MM);
}

/**
 * @brief 将车轮编码器累计计数换算为车轮行驶距离。
 * @param encoder_count 车轮编码器计数，单位 count。
 * @return 行驶距离，单位 mm。
 */
int32_t USER_MCM_EncoderCountToDistance(int32_t encoder_count)
{
    return (int32_t)(encoder_count / VEHICLE_ENCODER_PULSE_PER_MM);
}

/**
 * @brief 将车轮线速度换算为速度 PID 使用的编码器目标值。
 * @param speed_mm_s 车轮线速度，单位 mm/s，可为负数。
 * @return 一个控制周期内的车轮编码器计数，单位 count/period。
 */
int32_t USER_MCM_SpeedToEncoderCount(int32_t speed_mm_s)
{
    return MCM_SPEED_TO_ENCODER_COUNT((float)speed_mm_s);
}

/**
 * @brief 将速度 PID 的编码器反馈值换算为车轮线速度。
 * @param encoder_count_per_period 一个控制周期内的车轮编码器计数，单位 count/period。
 * @return 车轮线速度，单位 mm/s。
 */
int32_t USER_MCM_EncoderCountToSpeed(int32_t encoder_count_per_period)
{
    return (int32_t)(encoder_count_per_period / VEHICLE_ENCODER_PULSE_PER_MM *
                     (1000.0f / VEHICLE_ENCODER_UPDATE_INTERVAL_MS));
}

/**
 * @brief 将车体直线位移换算为里程计累计计数。
 * @param distance_mm 车体位移，单位 mm，可为负数。
 * @return 里程计编码器计数，单位 count。
 */
int32_t USER_MCM_DistanceToOdometerEncoderCount(int32_t distance_mm)
{
    return (int32_t)(distance_mm * ODOMETER_ENCODER_PULSE_PER_MM);
}

/**
 * @brief 将里程计累计计数换算为车体直线位移。
 * @param odometer_count 里程计编码器计数，单位 count。
 * @return 车体位移，单位 mm。
 */
int32_t USER_MCM_OdometerEncoderCountToDistance(int32_t odometer_count)
{
    return (int32_t)(odometer_count / ODOMETER_ENCODER_PULSE_PER_MM);
}

/**
 * @brief 将车体直线速度换算为里程计每周期计数。
 * @param speed_mm_s 车体直线速度，单位 mm/s，可为负数。
 * @return 一个控制周期内的里程计计数，单位 count/period。
 */
int32_t USER_MCM_SpeedToOdometerEncoderCount(int32_t speed_mm_s)
{
    return (int32_t)(speed_mm_s / (1000.0f / ODOMETER_UPDATE_INTERVAL_MS) * ODOMETER_ENCODER_PULSE_PER_MM);
}

/**
 * @brief 将里程计每周期计数换算为车体直线速度。
 * @param odometer_count_per_period 一个控制周期内的里程计计数，单位 count/period。
 * @return 车体直线速度，单位 mm/s。
 */
int32_t USER_MCM_OdometerEncoderCountToSpeed(int32_t odometer_count_per_period)
{
    return (int32_t)(odometer_count_per_period / ODOMETER_ENCODER_PULSE_PER_MM *
                     (1000.0f / ODOMETER_UPDATE_INTERVAL_MS));
}

/**
 * @brief 将任意角度归一化到 [-180, 180]。
 * @param angle 原始角度，单位 deg。
 * @return 归一化后的角度，单位 deg。
 */
float USER_MCM_NormalizeAngle(float angle)
{
    while (angle > 180.0f)
    {
        angle -= 360.0f;
    }

    while (angle < -180.0f)
    {
        angle += 360.0f;
    }

    return angle;
}

/**
 * @brief 计算从当前角度转到目标角度的最短角度差。
 * @param target 目标角度，单位 deg。
 * @param current 当前角度，单位 deg。
 * @return 最短角度差，单位 deg，范围 [-180, 180]。
 */
float USER_MCM_AngleDifference(float target, float current)
{
    return USER_MCM_NormalizeAngle(target - current);
}

/**
 * @brief 查询 MCM 是否处于空闲状态（无动作、未启动）。
 * @return true 表示空闲，可接受新动作。
 */
bool USER_MCM_IsIdle(void)
{
    return mcm.state == USER_MCM_STATE_IDLE;
}

/**
 * @brief 查询 MCM 是否正在执行动作（START 或 RUN 阶段）。
 * @return true 表示忙碌中，不可启动新动作。
 */
bool USER_MCM_IsBusy(void)
{
    return (mcm.state == USER_MCM_STATE_START) || (mcm.state == USER_MCM_STATE_RUN);
}

/**
 * @brief 获取当前动作的完成/错误状态码。
 * @return 状态码，DONE 后仍可读取。
 */
USER_MCM_Status_t USER_MCM_GetStatus(void)
{
    return mcm.status;
}

/**
 * @brief 获取当前状态机的 state 枚举。
 * @return IDLE / START / RUN / DONE / FAULT。
 */
USER_MCM_State_t USER_MCM_GetState(void)
{
    return mcm.state;
}

/**
 * @brief 获取当前正在执行（或刚完成）的动作类型。
 * @return NONE / STRAIGHT / SPIN / ARC。
 */
USER_MCM_ActionType_t USER_MCM_GetActionType(void)
{
    return mcm.type;
}

/**
 * @brief 初始化动作进入 RUN 前的 PID、计时和规划器状态。
 */
static void USER_MCM_EnterRun(void)
{
    /* 步骤1：清空速度内环和外环调试 PID，避免继承上次动作的积分。 */
    USER_PID_ClearAndStop(&speed_pid[MOTOR_0_LEFT]);
    USER_PID_ClearAndStop(&speed_pid[MOTOR_1_RIGHT]);
    USER_PID_ClearAndStop(&distance_pid);
    USER_PID_ClearAndStop(&angle_pid);

    /* 步骤2：初始化调试 PID 字段，便于 OLED 或调试器观察。 */
    if (mcm.type == USER_MCM_ACTION_STRAIGHT)
    {
        distance_pid.target = USER_MCM_DistanceToOdometerEncoderCount((int32_t)mcm.target_signed);
        distance_pid.max_output = USER_MCM_SpeedToEncoderCount((int32_t)mcm.max_rate);
        distance_pid.min_output = -distance_pid.max_output;
        distance_pid.deadzone = USER_MCM_DistanceToOdometerEncoderCount((int32_t)MCM_LINEAR_DONE_DEADBAND_MM);
    }
    else if (mcm.type == USER_MCM_ACTION_SPIN)
    {
        angle_pid.target = (int32_t)mcm.target_signed;
        angle_pid.max_output = (int32_t)mcm.max_rate;
        angle_pid.min_output = -angle_pid.max_output;
        angle_pid.deadzone = (int32_t)MCM_ROTATE_DEADZONE_DEG;
    }

    /* 步骤3：初始化规划器和保护计时。 */
    mcm.profile_rate = 0.0f;
    mcm.reference_progress = 0.0f;
    mcm.last_progress = 0.0f;
    mcm.finish_hold_count = 0u;
    mcm.start_tick = sysTick;
    mcm.progress_tick = sysTick;
    mcm.state = USER_MCM_STATE_RUN;
    mcm.status = USER_MCM_STATUS_BUSY;
}

/**
 * @brief 按加减速参数更新一维梯形规划器。
 * @param accel 加速度，单位为目标进度单位/s^2。
 * @param decel 减速度，单位为目标进度单位/s^2。
 */
static void USER_MCM_UpdateProfile(float accel, float decel)
{
    float stop_distance;

    stop_distance = (mcm.profile_rate * mcm.profile_rate) / (2.0f * decel);
    if (stop_distance >= mcm.remaining)
    {
        mcm.profile_rate -= decel * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f);
        if (mcm.profile_rate < 0.0f)
        {
            mcm.profile_rate = 0.0f;
        }
    }
    else
    {
        mcm.profile_rate += accel * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f);
        if (mcm.profile_rate > mcm.max_rate)
        {
            mcm.profile_rate = mcm.max_rate;
        }
    }

    mcm.reference_progress += mcm.profile_rate * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f);
    if (mcm.reference_progress > mcm.target_abs)
    {
        mcm.reference_progress = mcm.target_abs;
    }
}

/**
 * @brief 使用估测层左右轮速度作为速度 PID 反馈并输出电机命令。
 * @param left_speed_mm_s 左轮目标线速度，单位 mm/s。
 * @param right_speed_mm_s 右轮目标线速度，单位 mm/s。
 */
static void USER_MCM_ApplyWheelSpeedCommand(float left_speed_mm_s, float right_speed_mm_s)
{
    const USER_STATE_Estimate_t *est;

    est = USER_State_GetEstimate();

    speed_pid[MOTOR_0_LEFT].target = MCM_SPEED_TO_ENCODER_COUNT(left_speed_mm_s);
    speed_pid[MOTOR_1_RIGHT].target = MCM_SPEED_TO_ENCODER_COUNT(right_speed_mm_s);
    speed_pid[MOTOR_0_LEFT].current = MCM_SPEED_TO_ENCODER_COUNT(est->left_wheel_speed_mm_s);
    speed_pid[MOTOR_1_RIGHT].current = MCM_SPEED_TO_ENCODER_COUNT(est->right_wheel_speed_mm_s);

    USER_Positional_PID_Control(&speed_pid[MOTOR_0_LEFT]);
    USER_Positional_PID_Control(&speed_pid[MOTOR_1_RIGHT]);
    USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_NORMAL_RUN, (int16_t)speed_pid[MOTOR_0_LEFT].output);
    USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_NORMAL_RUN, (int16_t)speed_pid[MOTOR_1_RIGHT].output);
}

/**
 * @brief 进入故障状态并立即制动。
 * @param status 故障状态码。
 */
static void USER_MCM_Fault(USER_MCM_Status_t status)
{
    mcm.status = status;
    mcm.state = USER_MCM_STATE_FAULT;
    USER_MCM_FreezeAllMotions();
}

/**
 * @brief 完成动作并立即制动。
 */
static void USER_MCM_Done(void)
{
    mcm.status = USER_MCM_STATUS_OK;
    mcm.state = USER_MCM_STATE_DONE;
    mcm.type = USER_MCM_ACTION_NONE;
    USER_MCM_FreezeAllMotions();
}

/**
 * @brief 更新直行动作。
 * @param est 最新状态估计。
 */
static void USER_MCM_UpdateStraight(const USER_STATE_Estimate_t *est)
{
    float reference_distance_mm;
    float position_error_mm;
    float base_speed_mm_s;
    float heading_error_deg;
    float heading_correction_mm_s;
    float left_speed_mm_s;
    float right_speed_mm_s;

    /* 步骤1：检查直行动作必须具备的距离和速度估计。 */
    if ((est->distance_valid == 0u) || (est->velocity_valid == 0u))
    {
        USER_MCM_Fault(USER_MCM_STATUS_ENCODER_ERROR);
        return;
    }

    /* 步骤2：计算相对距离、进度和剩余距离。 */
    mcm.relative_distance_mm = est->distance_mm - mcm.start_distance_mm;
    mcm.relative_yaw_deg = USER_MCM_AngleDifference(est->yaw_deg, mcm.start_yaw_deg);
    mcm.progress = (float)mcm.drive_dir * mcm.relative_distance_mm;
    mcm.remaining = mcm.target_abs - mcm.progress;
    if (mcm.remaining < 0.0f)
    {
        mcm.remaining = 0.0f;
    }

    /* 步骤3：检查超时和无进展保护。 */
    if (MCM_ELAPSED_MS(mcm.start_tick) > mcm.timeout_ms)
    {
        USER_MCM_Fault(USER_MCM_STATUS_TIMEOUT);
        return;
    }
    if ((mcm.remaining > MCM_LINEAR_DONE_DEADBAND_MM) &&
        (mcm.progress > (mcm.last_progress + MCM_DISTANCE_PROGRESS_DEADBAND_MM)))
    {
        mcm.last_progress = mcm.progress;
        mcm.progress_tick = sysTick;
    }
    else if ((mcm.remaining > MCM_LINEAR_DONE_DEADBAND_MM) &&
             (MCM_ELAPSED_MS(mcm.progress_tick) > MCM_NO_PROGRESS_TIMEOUT_MS))
    {
        USER_MCM_Fault(USER_MCM_STATUS_NO_PROGRESS);
        return;
    }

    /* 步骤4：更新直线速度规划和位置误差修正。 */
    USER_MCM_UpdateProfile(MCM_LINEAR_ACCEL_MM_S2, MCM_LINEAR_DECEL_MM_S2);
    reference_distance_mm = (float)mcm.drive_dir * mcm.reference_progress;
    position_error_mm = reference_distance_mm - mcm.relative_distance_mm;
    base_speed_mm_s = (float)mcm.drive_dir * mcm.profile_rate +
                      MCM_LINEAR_POSITION_KP * position_error_mm;
    MCM_CLAMP_F(base_speed_mm_s, -mcm.max_rate, mcm.max_rate);

    /* 步骤5：可用航向估计时叠加差速航向修正。 */
    heading_error_deg = (mcm.heading_hold_enabled != 0u) ? -mcm.relative_yaw_deg : 0.0f;
    heading_correction_mm_s = MCM_STRAIGHT_HEADING_KP_MM_S_PER_DEG * heading_error_deg;
    left_speed_mm_s = base_speed_mm_s - heading_correction_mm_s;
    right_speed_mm_s = base_speed_mm_s + heading_correction_mm_s;
    MCM_CLAMP_F(left_speed_mm_s, -mcm.max_rate, mcm.max_rate);
    MCM_CLAMP_F(right_speed_mm_s, -mcm.max_rate, mcm.max_rate);

    /* 步骤6：更新调试字段并执行轮速内环。 */
    distance_pid.current = USER_MCM_DistanceToOdometerEncoderCount((int32_t)mcm.relative_distance_mm);
    distance_pid.error = distance_pid.target - distance_pid.current;
    distance_pid.output = MCM_SPEED_TO_ENCODER_COUNT(base_speed_mm_s);
    USER_MCM_ApplyWheelSpeedCommand(left_speed_mm_s, right_speed_mm_s);

    /* 步骤7：完成条件需要距离进死区且速度足够低，并连续保持若干周期。 */
    if ((MCM_ABS_F(mcm.target_signed - mcm.relative_distance_mm) <= MCM_LINEAR_DONE_DEADBAND_MM) &&
        (MCM_ABS_F(est->v_mm_s) <= MCM_LINEAR_STOP_SPEED_MM_S))
    {
        mcm.finish_hold_count++;
        if (mcm.finish_hold_count >= MCM_FINISH_HOLD_CYCLES)
        {
            USER_MCM_Done();
        }
    }
    else
    {
        mcm.finish_hold_count = 0u;
    }
}

/**
 * @brief 更新原地自旋动作。
 * @param est 最新状态估计。
 */
static void USER_MCM_UpdateSpin(const USER_STATE_Estimate_t *est)
{
    float reference_angle_deg;
    float angle_error_deg;
    float omega_cmd_deg_s;
    float wheel_speed_mm_s;

    /* 步骤1：原地自旋依赖有效航向和角速度估计。 */
    if ((est->yaw_valid == 0u) || (est->velocity_valid == 0u))
    {
        USER_MCM_Fault(USER_MCM_STATUS_IMU_ERROR);
        return;
    }

    /* 步骤2：计算相对航向、角度进度和剩余角度。 */
    mcm.relative_yaw_deg = USER_MCM_AngleDifference(est->yaw_deg, mcm.start_yaw_deg);
    mcm.progress = (float)mcm.spin_dir * mcm.relative_yaw_deg;
    mcm.remaining = mcm.target_abs - mcm.progress;
    if (mcm.remaining < 0.0f)
    {
        mcm.remaining = 0.0f;
    }

    /* 步骤3：检查超时和无进展保护。 */
    if (MCM_ELAPSED_MS(mcm.start_tick) > mcm.timeout_ms)
    {
        USER_MCM_Fault(USER_MCM_STATUS_TIMEOUT);
        return;
    }
    if ((mcm.remaining > MCM_ROTATE_DEADZONE_DEG) &&
        (mcm.progress > (mcm.last_progress + MCM_ROTATE_PROGRESS_DEADBAND_DEG)))
    {
        mcm.last_progress = mcm.progress;
        mcm.progress_tick = sysTick;
    }
    else if ((mcm.remaining > MCM_ROTATE_DEADZONE_DEG) &&
             (MCM_ELAPSED_MS(mcm.progress_tick) > MCM_NO_PROGRESS_TIMEOUT_MS))
    {
        USER_MCM_Fault(USER_MCM_STATUS_NO_PROGRESS);
        return;
    }

    /* 步骤4：更新角速度规划和角度误差修正。 */
    USER_MCM_UpdateProfile(MCM_ROTATE_ACCEL_DEG_S2, MCM_ROTATE_DECEL_DEG_S2);
    reference_angle_deg = (float)mcm.spin_dir * mcm.reference_progress;
    angle_error_deg = reference_angle_deg - mcm.relative_yaw_deg;
    omega_cmd_deg_s = (float)mcm.spin_dir * mcm.profile_rate +
                      MCM_ROTATE_ANGLE_KP * angle_error_deg;
    MCM_CLAMP_F(omega_cmd_deg_s, -mcm.max_rate, mcm.max_rate);

    /* 步骤5：将车体角速度映射为左右轮反向线速度。 */
    wheel_speed_mm_s = omega_cmd_deg_s * VEHICLE_TRACK_WIDTH_MM * 0.5f * MCM_PI / 180.0f;
    angle_pid.current = (int32_t)mcm.relative_yaw_deg;
    angle_pid.error = angle_pid.target - angle_pid.current;
    angle_pid.output = (int32_t)omega_cmd_deg_s;
    USER_MCM_ApplyWheelSpeedCommand(-wheel_speed_mm_s, wheel_speed_mm_s);

    /* 步骤6：完成条件需要角度进死区且角速度足够低，并连续保持若干周期。 */
    if ((MCM_ABS_F(mcm.target_signed - mcm.relative_yaw_deg) <= MCM_ROTATE_DEADZONE_DEG) &&
        (MCM_ABS_F(est->omega_deg_s) <= MCM_ROTATE_STOP_GYRO_DEG_S))
    {
        mcm.finish_hold_count++;
        if (mcm.finish_hold_count >= MCM_FINISH_HOLD_CYCLES)
        {
            USER_MCM_Done();
        }
    }
    else
    {
        mcm.finish_hold_count = 0u;
    }
}

/**
 * @brief MCM 主调度任务，每个控制周期由上层调用一次。
 *
 * 根据当前状态机状态分发：
 * - IDLE / DONE / FAULT：不执行任何操作，直接返回。
 * - START：调用 EnterRun 完成 PID 初始化与规划器复位，切到 RUN。
 * - RUN：根据动作类型调用 UpdateStraight 或 UpdateSpin 推进一帧。
 */
void USER_MCM_Task(void)
{
    const USER_STATE_Estimate_t *est;

    /* 步骤1：终态直接返回，避免无效调度。 */
    if ((mcm.state == USER_MCM_STATE_IDLE) ||
        (mcm.state == USER_MCM_STATE_DONE) ||
        (mcm.state == USER_MCM_STATE_FAULT))
    {
        return;
    }

    /* 步骤2：START 状态触发一次性进入 RUN 的初始化。 */
    if (mcm.state == USER_MCM_STATE_START)
    {
        USER_MCM_EnterRun();
        return;
    }

    /* 步骤3：RUN 状态获取最新估测并派发到对应动作更新函数。 */
    est = USER_State_GetEstimate();
    if (mcm.type == USER_MCM_ACTION_STRAIGHT)
    {
        USER_MCM_UpdateStraight(est);
    }
    else if (mcm.type == USER_MCM_ACTION_SPIN)
    {
        USER_MCM_UpdateSpin(est);
    }
    else
    {
        USER_MCM_Fault(USER_MCM_STATUS_PARAM_ERROR);
    }
}

/**
 * @brief 启动一个非阻塞直行动作（前进/后退）。
 * @param distance_mm 目标直线位移，正值为前进，负值为后退，单位 mm。
 * @param max_speed_mm_s 最大线速度，单位 mm/s。
 * @return 状态码：OK 表示已成功排队到 START 状态，下一周期自动进入 RUN。
 */
USER_MCM_Status_t USER_MCM_StartStraight(int32_t distance_mm, int32_t max_speed_mm_s)
{
    const USER_STATE_Estimate_t *est;

    /* 步骤1：前置校验——忙状态、零行程、无效参数。 */
    if (USER_MCM_IsBusy())
    {
        return USER_MCM_STATUS_BUSY;
    }
    if ((distance_mm == 0) || (max_speed_mm_s <= 0))
    {
        return USER_MCM_STATUS_PARAM_ERROR;
    }

    /* 步骤2：编码器可用性检查。 */
    est = USER_State_GetEstimate();
    if (est->distance_valid == 0u)
    {
        return USER_MCM_STATUS_ENCODER_ERROR;
    }

    /* 步骤3：快照统一估测状态，后续动作进度只用相对估测量。 */
    mcm.type = USER_MCM_ACTION_STRAIGHT;
    mcm.state = USER_MCM_STATE_START;
    mcm.status = USER_MCM_STATUS_BUSY;
    mcm.drive_dir = MCM_SIGN_I32(distance_mm);
    mcm.spin_dir = 1;
    mcm.target_abs = (float)MCM_ABS_I32(distance_mm);
    mcm.target_signed = (float)distance_mm;
    mcm.max_rate = (float)max_speed_mm_s;
    mcm.start_distance_mm = est->distance_mm;
    mcm.start_x_mm = est->x_mm;
    mcm.start_y_mm = est->y_mm;
    mcm.start_yaw_deg = est->yaw_deg;
    mcm.relative_distance_mm = 0.0f;
    mcm.relative_yaw_deg = 0.0f;
    mcm.progress = 0.0f;
    mcm.remaining = mcm.target_abs;
    mcm.heading_hold_enabled = est->yaw_valid;
    mcm.timeout_ms = MCM_TIMEOUT_MS(distance_mm, max_speed_mm_s, MCM_MOVE_TIMEOUT_BASE_MS);

    return USER_MCM_STATUS_OK;
}

/**
 * @brief 启动一个非阻塞原地自旋动作。
 * @param relative_angle_deg 目标相对角度，正值为逆时针，负值为顺时针，单位 deg。
 * @param max_gyro_deg_s 最大角速度，单位 deg/s。
 * @return 状态码：OK 表示已成功排队到 START 状态，下一周期自动进入 RUN。
 */
USER_MCM_Status_t USER_MCM_StartSpin(int32_t relative_angle_deg, int32_t max_gyro_deg_s)
{
    const USER_STATE_Estimate_t *est;

    /* 步骤1：前置校验——忙状态、零角度、无效参数。 */
    if (USER_MCM_IsBusy())
    {
        return USER_MCM_STATUS_BUSY;
    }
    if ((relative_angle_deg == 0) || (max_gyro_deg_s <= 0))
    {
        return USER_MCM_STATUS_PARAM_ERROR;
    }

    /* 步骤2：IMU 航向可用性检查。 */
    est = USER_State_GetEstimate();
    if (est->yaw_valid == 0u)
    {
        return USER_MCM_STATUS_IMU_ERROR;
    }

    /* 步骤3：快照统一估测状态，后续角度进度只用相对航向。 */
    mcm.type = USER_MCM_ACTION_SPIN;
    mcm.state = USER_MCM_STATE_START;
    mcm.status = USER_MCM_STATUS_BUSY;
    mcm.drive_dir = 1;
    mcm.spin_dir = MCM_SIGN_I32(relative_angle_deg);
    mcm.target_abs = (float)MCM_ABS_I32(relative_angle_deg);
    mcm.target_signed = (float)relative_angle_deg;
    mcm.max_rate = (float)max_gyro_deg_s;
    mcm.start_distance_mm = est->distance_mm;
    mcm.start_x_mm = est->x_mm;
    mcm.start_y_mm = est->y_mm;
    mcm.start_yaw_deg = est->yaw_deg;
    mcm.relative_distance_mm = 0.0f;
    mcm.relative_yaw_deg = 0.0f;
    mcm.progress = 0.0f;
    mcm.remaining = mcm.target_abs;
    mcm.heading_hold_enabled = 0u;
    mcm.timeout_ms = MCM_TIMEOUT_MS(relative_angle_deg, max_gyro_deg_s, MCM_ROTATE_TIMEOUT_BASE_MS);

    return USER_MCM_STATUS_OK;
}

/**
 * @brief 启动一个非阻塞圆弧动作（预留接口，当前未实现）。
 * @param radius_mm 圆弧半径，单位 mm。
 * @param arc_angle_deg 圆弧扫过的角度，单位 deg。
 * @param turn_dir 转弯方向（左/右）。
 * @param drive_dir 行驶方向（前进/后退）。
 * @param max_speed_mm_s 最大线速度，单位 mm/s。
 * @param max_gyro_deg_s 最大角速度，单位 deg/s。
 * @return 当前始终返回 PARAM_ERROR，等待后续阶段实现。
 */
USER_MCM_Status_t USER_MCM_StartArc(int32_t radius_mm,
                                    int32_t arc_angle_deg,
                                    USER_MCM_TurnDirection_t turn_dir,
                                    USER_MCM_DriveDirection_t drive_dir,
                                    int32_t max_speed_mm_s,
                                    int32_t max_gyro_deg_s)
{
    /* 步骤1：预留接口，参数暂不消费，直接返回未实现。 */
    (void)radius_mm;
    (void)arc_angle_deg;
    (void)turn_dir;
    (void)drive_dir;
    (void)max_speed_mm_s;
    (void)max_gyro_deg_s;

    return USER_MCM_STATUS_PARAM_ERROR;
}

/**
 * @brief 取消当前动作，回到 IDLE 并制动停车。
 *
 * 无论 MCM 当前处于什么状态，调用后立即复位状态机并刹停电机。
 */
void USER_MCM_Cancel(void)
{
    /* 步骤1：复位状态机到 IDLE，标记取消原因。 */
    mcm.type = USER_MCM_ACTION_NONE;
    mcm.state = USER_MCM_STATE_IDLE;
    mcm.status = USER_MCM_STATUS_CANCELED;
    /* 步骤2：执行统一制动流程。 */
    USER_MCM_FreezeAllMotions();
}

/**
 * @brief 立即停止所有电机并清空 PID 状态（不改变 MCM 状态机）。
 *
 * 由 DONE / FAULT / Cancel 等路径复用，确保停车行为一致。
 */
void USER_MCM_FreezeAllMotions(void)
{
    /* 步骤1：清空左右轮速度 PID 和外环调试 PID，避免下次动作继承旧积分或旧输出。 */
    USER_PID_ClearAndStop(&speed_pid[MOTOR_0_LEFT]);
    USER_PID_ClearAndStop(&speed_pid[MOTOR_1_RIGHT]);
    USER_PID_ClearAndStop(&distance_pid);
    USER_PID_ClearAndStop(&angle_pid);

    /* 步骤2：切到再生制动，给车辆一个确定的停车状态。 */
    USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_REGEN_BRAKE, 0);
    USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_REGEN_BRAKE, 0);
}
