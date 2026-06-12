#ifndef USERAPP_STATE_ESTIMATOR_H
#define USERAPP_STATE_ESTIMATOR_H

/**
 * @file userapp_state_estimator.h
 * @brief 向应用层发布统一车辆状态估计。
 *
 * 状态估测层是应用层中唯一读取编码器、IMU 等原始物理传感器全局数据的模块。
 * 运动控制代码应消费这里发布的统一状态估计，而不是直接读取传感器。
 *
 * 当前支持可选择的估测方法。第一版可用方法是 raw-passthrough 默认估测器：
 * 1. 将编码器计数和 IMU 字段换算为物理量。
 * 2. 发布距离、速度、航向角和诊断用差异量。
 * 3. 附带有效性标志和质量分数。
 *
 * 后续可以将部分字段接入加权融合、Kalman 或 EKF 算法，
 * 同时保持面向 MCM 的接口不变。
 */

#include <stdint.h>

#include "user_config.h"

/**
 * @brief 当前估计结果使用的来源算法。
 */
typedef enum
{
    USER_STATE_SOURCE_RAW = 0,
    USER_STATE_SOURCE_WEIGHTED,
    USER_STATE_SOURCE_COMPLEMENTARY,
    USER_STATE_SOURCE_KALMAN,
    USER_STATE_SOURCE_EKF
} USER_STATE_Source_t;

/**
 * @brief 默认估测方法。
 *
 * 如果工程没有在 `user_config.h` 中覆盖本宏，则状态估测层使用基础直通估测器。
 */
#ifndef USER_STATE_DEFAULT_ESTIMATOR
#define USER_STATE_DEFAULT_ESTIMATOR USER_STATE_SOURCE_RAW
#endif

/**
 * @brief 统一车辆状态估计。
 *
 * 主要控制字段：
 * - `distance_mm`：优先使用的前向位移估计。
 * - `x_mm`, `y_mm`：估测坐标系下的平面航位推算位置。
 * - `yaw_deg`：优先使用的航向角估计。
 * - `v_mm_s`：优先使用的车体中心线速度估计。
 * - `omega_deg_s`：优先使用的偏航角速度估计。
 *
 * 诊断字段保留 raw-passthrough 的中间估计，便于后续 MCM 和显示代码
 * 观察打滑、航向差异和降级状态。
 */
typedef struct
{
    float distance_mm;
    float x_mm;
    float y_mm;
    float yaw_deg;
    float v_mm_s;
    float omega_deg_s;
    float gyro_bias_deg_s;

    float odom_distance_mm;
    float wheel_distance_mm;
    float left_wheel_speed_mm_s;
    float right_wheel_speed_mm_s;
    float wheel_yaw_deg;
    float imu_yaw_deg;
    float imu_gyro_yaw_deg;
    float slip_error_mm;
    float yaw_disagree_deg;

    uint8_t distance_valid;
    uint8_t yaw_valid;
    uint8_t velocity_valid;
    uint8_t degraded;
    uint8_t quality;
    USER_STATE_Source_t source;
} USER_STATE_Estimate_t;

/**
 * @brief 初始化状态估测器，并快照用于速度计算的计数器。
 */
void USER_STATE_Init(void);

/**
 * @brief 设置状态估测器使用的估测方法。
 *
 * 尚未实现的高级方法当前会自动回落到默认直通估测器，但保留函数入口，
 * 便于后续接入加权融合、Kalman 或 EKF 时不修改 MCM 调用方式。
 */
void USER_STATE_SetEstimatorMethod(USER_STATE_Source_t method);

/**
 * @brief 返回当前请求使用的估测方法。
 */
USER_STATE_Source_t USER_STATE_GetEstimatorMethod(void);

/**
 * @brief 刷新统一状态估计。
 *
 * 每 `MCM_CONTROL_PERIOD_MS` 调用一次，并且应在 MCM 消费估计结果之前调用。
 */
void USER_State_Task(void);

/**
 * @brief 返回最新估计结果的只读指针。
 */
const USER_STATE_Estimate_t *USER_STATE_GetEstimate(void);

#endif /* USERAPP_STATE_ESTIMATOR_H */
