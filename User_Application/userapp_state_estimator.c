#include "userapp_state_estimator.h"

#include "userapp_vehicle_model.h"
#include "userlib_encoder.h"
#include "userlib_imu.h"

#include <math.h>
#include <stdbool.h>

/**
 * @file userapp_state_estimator.c
 * @brief 可切换策略的车辆状态估测器。
 *
 * 本模块是物理传感器和运动控制逻辑之间的边界。它读取原始编码器/IMU
 * 全局数据，将其换算为物理量，生成单一的 `USER_STATE_Estimate_t`，
 * 并标记降级状态。
 *
 * 当前可用策略：
 * 1. 优先使用路程计作为距离和速度来源。
 * 2. 使用驱动轮平均值作为距离和速度的后备来源。
 * 3. 优先使用 IMU 航向角/陀螺仪作为航向和角速度来源。
 * 4. 使用驱动轮差速作为航向和角速度的后备来源。
 * 5. 发布诊断用差异量，便于后续调参和滤波。
 *
 * 后续加权融合、Kalman 和 EKF 方法应继续消费本文件内部的原始输入结构，
 * 不直接读取传感器全局变量，从而保持“采集”和“估测”两层边界清晰。
 */

extern EncoderData_t_Typedef encoder_data[3];
extern IMU_Data_StructTypeDef imu_data;

#define STATE_DT_S ((float)MCM_CONTROL_PERIOD_MS / 1000.0f)
#define STATE_RAD_PER_DEG (MCM_PI / 180.0f)
#define STATE_DEG_PER_RAD (180.0f / MCM_PI)

/**
 * @brief 单周期采集后的标准化原始输入。
 *
 * 该结构只在估测器内部使用，用来隔离物理传感器全局变量和具体估测算法。
 * 后续高级估测方法应只消费这里的物理量和有效性标志。
 */
typedef struct
{
    bool odom_valid;
    bool wheel_valid;
    bool imu_valid;

    int32_t odom_count;
    int32_t odom_delta_count;

    float odom_distance_mm;
    float odom_delta_mm;
    float odom_speed_mm_s;

    float left_distance_mm;
    float right_distance_mm;
    float left_speed_mm_s;
    float right_speed_mm_s;

    float wheel_distance_mm;
    float wheel_speed_mm_s;
    float wheel_yaw_deg;
    float wheel_omega_deg_s;

    float imu_yaw_deg;
    float imu_gyro_yaw_deg;
    float imu_omega_deg_s;
} USER_STATE_RawInput_t;

static USER_STATE_Estimate_t state_estimate;
static int32_t last_odom_count;
static USER_STATE_Source_t state_method = (USER_STATE_Source_t)USER_STATE_DEFAULT_ESTIMATOR;
static bool state_initialized;

/**
 * @brief 检查指定编码器通道是否可用于状态估测。
 */
static bool USER_STATE_EncoderValid(uint8_t index)
{
    return (encoder_data[index].status != ENCODER_STA_UNINIT) &&
           (encoder_data[index].status != ENCODER_STA_OVERFLOW);
}

/**
 * @brief 检查 IMU 数据是否可用于航向估测。
 */
static bool USER_STATE_ImuValid(void)
{
    return imu_data.status == IMU_STA_OK;
}

/**
 * @brief 将驱动轮累计编码器计数换算为 mm。
 */
static float USER_STATE_WheelDistanceMm(uint8_t index)
{
    return (float)encoder_data[index].sum_distance / VEHICLE_ENCODER_PULSE_PER_MM;
}

/**
 * @brief 将驱动轮编码器速度计数换算为 mm/s。
 */
static float USER_STATE_WheelSpeedMmS(uint8_t index)
{
    return ((float)encoder_data[index].speed / VEHICLE_ENCODER_PULSE_PER_MM) *
           (1000.0f / (float)VEHICLE_ENCODER_UPDATE_INTERVAL_MS);
}

/**
 * @brief 将角度误差归一化到 [-180, 180] deg。
 */
static float USER_STATE_NormalizeAngle(float angle_deg)
{
    while (angle_deg > 180.0f)
    {
        angle_deg -= 360.0f;
    }

    while (angle_deg < -180.0f)
    {
        angle_deg += 360.0f;
    }

    return angle_deg;
}

/**
 * @brief 采集并换算一周期原始传感器输入。
 *
 * 步骤说明：
 * 1. 检查路程计、左右驱动轮编码器和 IMU 的可用状态。
 * 2. 将累计计数换算为距离，将速度计数换算为线速度。
 * 3. 使用左右驱动轮距离差计算后备航向角和后备角速度。
 * 4. 更新路程计快照，为下个周期的速度估计准备增量基准。
 */
static void USER_STATE_CollectRawInput(USER_STATE_RawInput_t *raw)
{
    bool left_valid;
    bool right_valid;

    raw->odom_valid = USER_STATE_EncoderValid(0);
    left_valid = USER_STATE_EncoderValid(1);
    right_valid = USER_STATE_EncoderValid(2);
    raw->wheel_valid = left_valid && right_valid;
    raw->imu_valid = USER_STATE_ImuValid();

    raw->odom_count = encoder_data[0].sum_distance;
    raw->odom_delta_count = raw->odom_count - last_odom_count;
    last_odom_count = raw->odom_count;

    raw->odom_distance_mm = (float)raw->odom_count * ODOMETER_MM_PER_PULSE;
    raw->odom_delta_mm = (float)raw->odom_delta_count * ODOMETER_MM_PER_PULSE;
    raw->odom_speed_mm_s = raw->odom_delta_mm / STATE_DT_S;

    raw->left_distance_mm = USER_STATE_WheelDistanceMm(1);
    raw->right_distance_mm = USER_STATE_WheelDistanceMm(2);
    raw->left_speed_mm_s = USER_STATE_WheelSpeedMmS(1);
    raw->right_speed_mm_s = USER_STATE_WheelSpeedMmS(2);

    raw->wheel_distance_mm = (raw->left_distance_mm + raw->right_distance_mm) * 0.5f;
    raw->wheel_speed_mm_s = (raw->left_speed_mm_s + raw->right_speed_mm_s) * 0.5f;
    raw->wheel_yaw_deg = ((raw->right_distance_mm - raw->left_distance_mm) /
                          VEHICLE_TRACK_WIDTH_MM) *
                         STATE_DEG_PER_RAD;
    raw->wheel_omega_deg_s = ((raw->right_speed_mm_s - raw->left_speed_mm_s) /
                              VEHICLE_TRACK_WIDTH_MM) *
                             STATE_DEG_PER_RAD;

    raw->imu_yaw_deg = imu_data.yaw;
    raw->imu_gyro_yaw_deg = imu_data.sum_gyroz;
    raw->imu_omega_deg_s = imu_data.gyroz;
}

/**
 * @brief 发布所有估测方法共用的原始诊断字段。
 */
static void USER_STATE_PublishRawDiagnostics(const USER_STATE_RawInput_t *raw)
{
    state_estimate.odom_distance_mm = raw->odom_distance_mm;
    state_estimate.wheel_distance_mm = raw->wheel_distance_mm;
    state_estimate.left_wheel_speed_mm_s = raw->left_speed_mm_s;
    state_estimate.right_wheel_speed_mm_s = raw->right_speed_mm_s;
    state_estimate.wheel_yaw_deg = raw->wheel_yaw_deg;
    state_estimate.imu_yaw_deg = raw->imu_yaw_deg;
    state_estimate.imu_gyro_yaw_deg = raw->imu_gyro_yaw_deg;
    state_estimate.slip_error_mm = raw->odom_distance_mm - raw->wheel_distance_mm;
    state_estimate.yaw_disagree_deg = USER_STATE_NormalizeAngle(raw->imu_yaw_deg -
                                                                raw->wheel_yaw_deg);
}

/**
 * @brief 根据有效性标志刷新通用质量分数。
 */
static void USER_STATE_UpdateQuality(void)
{
    uint8_t quality;

    quality = 0;
    if (state_estimate.distance_valid)
    {
        quality += 40u;
    }
    if (state_estimate.yaw_valid)
    {
        quality += 40u;
    }
    if (state_estimate.velocity_valid)
    {
        quality += 20u;
    }
    state_estimate.quality = quality;
}

/**
 * @brief 默认直通估测方法。
 *
 * 步骤说明：
 * 1. 距离和线速度优先采用路程计，路程计不可用时回落到驱动轮平均值。
 * 2. 航向和角速度优先采用 IMU，IMU 不可用时回落到左右轮差速积分。
 * 3. 使用选定航向和本周期距离增量更新平面位置。
 * 4. 发布有效性、降级标志和质量分数。
 */
static void USER_STATE_RunDefaultEstimator(const USER_STATE_RawInput_t *raw)
{
    float yaw_rad;

    USER_STATE_PublishRawDiagnostics(raw);

    if (raw->odom_valid)
    {
        state_estimate.distance_mm = raw->odom_distance_mm;
        state_estimate.v_mm_s = raw->odom_speed_mm_s;
    }
    else
    {
        state_estimate.distance_mm = raw->wheel_distance_mm;
        state_estimate.v_mm_s = raw->wheel_speed_mm_s;
    }

    if (raw->imu_valid)
    {
        state_estimate.yaw_deg = raw->imu_yaw_deg;
        state_estimate.omega_deg_s = raw->imu_omega_deg_s;
    }
    else
    {
        state_estimate.yaw_deg = raw->wheel_yaw_deg;
        state_estimate.omega_deg_s = raw->wheel_omega_deg_s;
    }

    if (raw->odom_valid)
    {
        yaw_rad = state_estimate.yaw_deg * STATE_RAD_PER_DEG;
        state_estimate.x_mm += raw->odom_delta_mm * cosf(yaw_rad);
        state_estimate.y_mm += raw->odom_delta_mm * sinf(yaw_rad);
    }

    state_estimate.distance_valid = raw->odom_valid || raw->wheel_valid;
    state_estimate.yaw_valid = raw->imu_valid || raw->wheel_valid;
    state_estimate.velocity_valid = raw->odom_valid || raw->wheel_valid;
    state_estimate.degraded = (!raw->odom_valid || !raw->imu_valid) ? 1u : 0u;
    state_estimate.source = USER_STATE_SOURCE_RAW;
    USER_STATE_UpdateQuality();
}

/**
 * @brief 加权融合估测方法预留入口。
 *
 * 当前尚未实现权重配置、可信度门限和异常剔除逻辑，因此先回落到默认直通估测器。
 */
static void USER_STATE_RunWeightedEstimator(const USER_STATE_RawInput_t *raw)
{
    USER_STATE_RunDefaultEstimator(raw);
}

/**
 * @brief 互补滤波估测方法预留入口。
 *
 * 当前尚未接入 Algorithm 层互补滤波库，因此先回落到默认直通估测器。
 */
static void USER_STATE_RunComplementaryEstimator(const USER_STATE_RawInput_t *raw)
{
    USER_STATE_RunDefaultEstimator(raw);
}

/**
 * @brief Kalman 估测方法预留入口。
 *
 * 当前尚未接入 Algorithm 层 Kalman 库，因此先回落到默认直通估测器。
 */
static void USER_STATE_RunKalmanEstimator(const USER_STATE_RawInput_t *raw)
{
    USER_STATE_RunDefaultEstimator(raw);
}

/**
 * @brief EKF 估测方法预留入口。
 *
 * 当前尚未接入 Algorithm 层 EKF 库，因此先回落到默认直通估测器。
 */
static void USER_STATE_RunEkfEstimator(const USER_STATE_RawInput_t *raw)
{
    USER_STATE_RunDefaultEstimator(raw);
}

/**
 * @brief 按当前选择的策略执行估测。
 */
static void USER_STATE_RunSelectedEstimator(const USER_STATE_RawInput_t *raw)
{
    switch (state_method)
    {
    case USER_STATE_SOURCE_WEIGHTED:
        USER_STATE_RunWeightedEstimator(raw);
        break;

    case USER_STATE_SOURCE_COMPLEMENTARY:
        USER_STATE_RunComplementaryEstimator(raw);
        break;

    case USER_STATE_SOURCE_KALMAN:
        USER_STATE_RunKalmanEstimator(raw);
        break;

    case USER_STATE_SOURCE_EKF:
        USER_STATE_RunEkfEstimator(raw);
        break;

    case USER_STATE_SOURCE_RAW:
    default:
        USER_STATE_RunDefaultEstimator(raw);
        break;
    }
}

void USER_STATE_Init(void)
{
    /* 步骤1：清零所有发布的估计量和诊断量。 */
    state_estimate.distance_mm = 0.0f;
    state_estimate.x_mm = 0.0f;
    state_estimate.y_mm = 0.0f;
    state_estimate.yaw_deg = 0.0f;
    state_estimate.v_mm_s = 0.0f;
    state_estimate.omega_deg_s = 0.0f;
    state_estimate.gyro_bias_deg_s = 0.0f;
    state_estimate.odom_distance_mm = 0.0f;
    state_estimate.wheel_distance_mm = 0.0f;
    state_estimate.left_wheel_speed_mm_s = 0.0f;
    state_estimate.right_wheel_speed_mm_s = 0.0f;
    state_estimate.wheel_yaw_deg = 0.0f;
    state_estimate.imu_yaw_deg = 0.0f;
    state_estimate.imu_gyro_yaw_deg = 0.0f;
    state_estimate.slip_error_mm = 0.0f;
    state_estimate.yaw_disagree_deg = 0.0f;
    state_estimate.distance_valid = 0;
    state_estimate.yaw_valid = 0;
    state_estimate.velocity_valid = 0;
    state_estimate.degraded = 1;
    state_estimate.quality = 0;
    state_estimate.source = USER_STATE_SOURCE_RAW;
    state_method = (USER_STATE_Source_t)USER_STATE_DEFAULT_ESTIMATOR;

    /* 步骤2：快照路程计计数，使速度估计从零增量开始。 */
    last_odom_count = encoder_data[0].sum_distance;
    state_initialized = true;
}

void USER_STATE_SetEstimatorMethod(USER_STATE_Source_t method)
{
    switch (method)
    {
    case USER_STATE_SOURCE_RAW:
    case USER_STATE_SOURCE_WEIGHTED:
    case USER_STATE_SOURCE_COMPLEMENTARY:
    case USER_STATE_SOURCE_KALMAN:
    case USER_STATE_SOURCE_EKF:
        state_method = method;
        break;

    default:
        state_method = USER_STATE_SOURCE_RAW;
        break;
    }
}

USER_STATE_Source_t USER_STATE_GetEstimatorMethod(void)
{
    return state_method;
}

void USER_State_Task(void)
{
    USER_STATE_RawInput_t raw;

    if (!state_initialized)
    {
        USER_STATE_Init();
    }

    /* 步骤1：统一采集和换算原始输入，具体估测方法不直接读取传感器。 */
    USER_STATE_CollectRawInput(&raw);

    /* 步骤2：按当前选择的方法发布统一状态估计。 */
    USER_STATE_RunSelectedEstimator(&raw);
}

const USER_STATE_Estimate_t *USER_STATE_GetEstimate(void)
{
    return &state_estimate;
}
