# 非阻塞运动控制状态机方案

## 1. 目标

将当前阻塞式运动函数彻底替换为全新的非阻塞运动控制模块。该模块每个控制周期运行一次，通常为 `MCM_CONTROL_PERIOD_MS = 10ms`。不保留旧阻塞 API 的兼容包装器。

需要支持的动作：

1. 按给定相对角度原地自旋。
2. 按给定距离直行前进或后退。
3. 沿给定半径向左或向右前进转过给定角度。
4. 沿给定半径向左或向右后退转过给定角度。

每个动作都必须：

1. 非阻塞。
2. 具备稳定的加速、减速、完成判定和超时保护。
3. 可以设置最大线速度或最大角速度。
4. 在各传感器适用的场景下融合 IMU 陀螺仪/航向角、驱动轮编码器和路程计编码器。
5. 继续使用现有左右轮速度 PID 作为内环。

当前代码已经具备核心部件：

1. `encoder_data[0]`：路程计编码器，用作车体距离反馈。
2. `encoder_data[1]`：左驱动轮编码器速度和累计计数。
3. `encoder_data[2]`：右驱动轮编码器速度和累计计数。
4. `imu_data.gyroz`、`imu_data.yaw`、`imu_data.sum_gyroz`：偏航角速度和偏航角反馈。
5. `speed_pid[MOTOR_0_LEFT]`、`speed_pid[MOTOR_1_RIGHT]`：驱动轮速度 PID。
6. 现有阻塞函数中包含有用的梯形规划和外环修正思路，但其公共 API 和阻塞执行模型都将被替换。

## 2. 架构

### 2.0 分层边界

保持 `User_Algorithm` 为纯算法层。它不应依赖具体工程传感器，例如 `encoder_data`、`imu_data`、路程计通道、电机 ID 或 `USER_MCM_*` 类型。

建议职责划分：

1. `User_Algorithm/userlib_kalman.h/.c`
   - 只做通用 Kalman/EKF 数学计算。
   - 输入是数值向量、矩阵、模型回调和协方差参数。
   - 输出是估计状态向量和协方差矩阵。
   - 不直接访问 IMU、编码器、电机、SysTick、OLED 或 MCM 全局变量。

2. `User_Application/userapp_state_estimator.h/.c`
   - 车辆专用状态估测层。
   - 读取 IMU、驱动轮编码器和路程计数据。
   - 将传感器读数转换为统一状态估计。
   - 第一版可以只做 raw passthrough、单位换算和有效性标记。
   - 后续可以将传感器读数转换为通用互补滤波或 Kalman/EKF 输入向量。
   - 处理车辆模型、单位、符号约定、传感器有效性检查和质量标志。
   - 向 MCM 发布车辆状态估计结果。

3. `User_Application/userapp_mcm.h/.c`
   - 运动控制状态机。
   - 只消费状态估测层输出。
   - 不直接读取原始物理传感器。
   - 不直接调用底层 Kalman 矩阵运算。

这样可以保持算法库可复用、可测试。车辆专用融合策略应位于纯算法层之上。

### 2.1 模块布局

用新的非阻塞 API 替换现有阻塞式运动 API。旧的阻塞入口应从 `userapp_mcm.h` 中移除，所有调用方都迁移到新的启动/任务/状态模型。

建议文件：

1. `User_Application/userapp_mcm.h`
2. `User_Application/userapp_mcm.c`

建议新增公共 API：

```c
typedef enum
{
    USER_MCM_ACTION_NONE = 0,
    USER_MCM_ACTION_STRAIGHT,
    USER_MCM_ACTION_SPIN,
    USER_MCM_ACTION_ARC
} USER_MCM_ActionType_t;

typedef enum
{
    USER_MCM_STATE_IDLE = 0,
    USER_MCM_STATE_START,
    USER_MCM_STATE_RUN,
    USER_MCM_STATE_FINISH_HOLD,
    USER_MCM_STATE_DONE,
    USER_MCM_STATE_FAULT
} USER_MCM_State_t;

typedef enum
{
    USER_MCM_TURN_LEFT = 1,
    USER_MCM_TURN_RIGHT = -1
} USER_MCM_TurnDirection_t;

typedef enum
{
    USER_MCM_DRIVE_FORWARD = 1,
    USER_MCM_DRIVE_BACKWARD = -1
} USER_MCM_DriveDirection_t;

bool USER_MCM_IsIdle(void);
bool USER_MCM_IsBusy(void);
USER_MCM_Status_t USER_MCM_GetStatus(void);
USER_MCM_ActionType_t USER_MCM_GetActionType(void);
void USER_MCM_Task(void);
void USER_MCM_Cancel(void);

USER_MCM_Status_t USER_MCM_StartStraight(int32_t distance_mm,
                                          int32_t max_speed_mm_s);

USER_MCM_Status_t USER_MCM_StartSpin(int32_t relative_angle_deg,
                                      int32_t max_gyro_deg_s);

USER_MCM_Status_t USER_MCM_StartArc(int32_t radius_mm,
                                    int32_t arc_angle_deg,
                                    USER_MCM_TurnDirection_t turn_dir,
                                    USER_MCM_DriveDirection_t drive_dir,
                                    int32_t max_speed_mm_s,
                                    int32_t max_gyro_deg_s);
```

`USER_MCM_Task()` 在 `main.c` 的 10ms 周期块中调用，并且必须替代 `main.c` 中直接写速度 PID 的逻辑。无论处于忙碌状态还是执行空闲策略，MCM 都应拥有电机命令的写入权。

命名规则：

1. 使用 `USER_MCM_Task()` 作为唯一的 10ms MCM 更新函数。
2. 使用 `USER_MCM_Cancel()` 表示用户或应用层取消动作。
3. 保留 `USER_MCM_FreezeAllMotions()` 作为底层立即制动辅助函数。
4. 使用 `USER_MCM_StartStraight()`、`USER_MCM_StartSpin()` 和 `USER_MCM_StartArc()` 作为当前有效的动作启动 API。
5. 不要再引入 `USER_MCM_Update10ms()`、`USER_MCM_Stop()`、`USER_MCM_StartMoveDistance()` 或 `USER_MCM_StartRotateAngle()` 等平行命名，除非文档和代码整体统一改名。

MCM 输入规则：

```text
sensor drivers -> state estimator -> MCM
```

MCM 不得直接读取 `encoder_data[]`、`imu_data` 或其他原始物理传感器全局变量。即使没有启用高级估测算法，状态估测层也应发布 raw-passthrough 估计，包含单位换算、有效性标志和质量字段。

当前工程状态：

1. `User_Application/userapp_state_estimator.h/.c` 已经存在。
2. 估测器已经具备可选方法分发入口，包含 raw/default、weighted、complementary、Kalman 和 EKF 分支。
3. 非阻塞 MCM 改造应完善并消费这个估测层输出，不应在 MCM 内重新建立一套传感器适配层。

### 2.2 为什么使用一个状态机

四类动作本质上是同一个控制问题：

1. 一个标量进度变量从 `0` 走到目标值。
2. 规划器计算平滑的参考速度。
3. 反馈项修正跟踪误差。
4. 运动学映射将车体命令 `(v, omega)` 转换为左右轮速度目标。
5. 现有轮速 PID 驱动电机。

区别只在于：

1. 进度的含义：距离 mm 或角度 deg。
2. 约束条件：最大线速度或最大角速度。
3. `(v, omega)` 的生成方式。
4. 使用哪个传感器作为主反馈。

因此实现上应使用一个状态机和少量按动作区分的计算函数。

## 3. 核心数据结构

建议内部上下文：

```c
typedef struct
{
    USER_MCM_ActionType_t type;
    USER_MCM_State_t state;
    USER_MCM_Status_t status;

    int32_t drive_dir;       /* +1 forward, -1 backward */
    int32_t turn_dir;        /* +1 left, -1 right */

    float target_distance_mm;
    float target_angle_deg;
    float radius_mm;

    float max_speed_mm_s;
    float max_gyro_deg_s;

    float profile_rate;
    float reference_progress;
    float last_progress;

    float start_distance_mm;
    float start_x_mm;
    float start_y_mm;
    float start_yaw_deg;

    float relative_distance_mm;
    float relative_x_mm;
    float relative_y_mm;
    float relative_yaw_deg;
    float current_v_mm_s;
    float current_omega_deg_s;

    float progress;
    float remaining;
    float finish_error;

    uint8_t finish_hold_count;
    uint32_t start_tick;
    uint32_t progress_tick;
    uint32_t timeout_ms;

    uint8_t estimate_quality;
} USER_MCM_Context_t;
```

在 `userapp_mcm.c` 中使用 `static USER_MCM_Context_t mcm;`。

重要规则：MCM 只快照状态估测层输出。它不得快照原始传感器计数，也不得清零全局传感器累计量。

```c
const USER_STATE_Estimate_t *est = USER_STATE_GetEstimate();

mcm.start_distance_mm = est->distance_mm;
mcm.start_x_mm = est->x_mm;
mcm.start_y_mm = est->y_mm;
mcm.start_yaw_deg = est->yaw_deg;
```

动作运行期间，MCM 从同一个统一估计中计算相对运动：

```c
mcm.relative_distance_mm = est->distance_mm - mcm.start_distance_mm;
mcm.relative_x_mm = est->x_mm - mcm.start_x_mm;
mcm.relative_y_mm = est->y_mm - mcm.start_y_mm;
mcm.relative_yaw_deg = USER_MCM_AngleDifference(est->yaw_deg, mcm.start_yaw_deg);
```

原始传感器快照和传感器专用 delta 计算属于状态估测层。这样可以避免某个动作重置传感器累计量，从而影响其他调试视图或模块。

## 4. 周期执行

每 10ms 的调用流程：

```c
void USER_MCM_Task(void)
{
    switch (mcm.state)
    {
    case USER_MCM_STATE_IDLE:
    case USER_MCM_STATE_DONE:
    case USER_MCM_STATE_FAULT:
        return;

    case USER_MCM_STATE_START:
        USER_MCM_EnterRun();
        break;

    case USER_MCM_STATE_RUN:
        USER_MCM_UpdateSensors();
        USER_MCM_CheckFaults();
        USER_MCM_UpdateProfile();
        USER_MCM_UpdateBodyCommand();
        USER_MCM_ApplyWheelSpeedCommand();
        USER_MCM_CheckFinish();
        break;

    case USER_MCM_STATE_FINISH_HOLD:
        USER_MCM_ApplyBrakeOrZeroSpeed();
        USER_MCM_CheckFinishHold();
        break;
    }
}
```

比赛应用也必须变成非阻塞。不要再这样写：

```c
USER_MCM_MoveDistance(500, 300);
USER_MCM_RotateAngle(180, 90);
```

改用路线状态机：

```c
switch (route_step)
{
case 0:
    if (USER_MCM_IsIdle())
        USER_MCM_StartStraight(500, 300);
    route_step = 1;
    break;

case 1:
    if (USER_MCM_GetStatus() == USER_MCM_STATUS_OK)
        route_step = 2;
    break;
}
```

## 5. 传感器融合策略

### 5.0 估计器边界

下面描述的融合策略是车辆专用的。如果使用 Kalman/EKF，通用滤波器实现应留在 `User_Algorithm`，传感器到状态量的适配层应放在 `User_Algorithm` 之外。

通用滤波器输入示例：

```c
typedef struct
{
    uint8_t state_dim;
    uint8_t measure_dim;
    float x[KALMAN_MAX_STATE_DIM];
    float P[KALMAN_MAX_STATE_DIM][KALMAN_MAX_STATE_DIM];
    float Q[KALMAN_MAX_STATE_DIM][KALMAN_MAX_STATE_DIM];
    float R[KALMAN_MAX_MEAS_DIM][KALMAN_MAX_MEAS_DIM];
} USER_KALMAN_Context_t;
```

状态估测层传感器输入示例：

```c
typedef struct
{
    float dt_s;
    float odom_delta_mm;
    float left_delta_mm;
    float right_delta_mm;
    float imu_yaw_deg;
    float imu_gyro_z_deg_s;
    uint8_t odom_valid;
    uint8_t wheel_valid;
    uint8_t imu_valid;
} USER_STATE_SensorInput_t;
```

状态估测层输出示例：

```c
typedef struct
{
    float distance_mm;
    float x_mm;
    float y_mm;
    float yaw_deg;
    float v_mm_s;
    float omega_deg_s;
    float gyro_bias_deg_s;
    uint8_t distance_valid;
    uint8_t yaw_valid;
    uint8_t velocity_valid;
    uint8_t degraded;
    uint8_t quality;
} USER_STATE_Estimate_t;
```

MCM 层消费 `USER_STATE_Estimate_t` 或等价的状态估测层输出。Kalman 库永远不直接读取具体工程全局变量。

### 5.1 传感器职责

在每个传感器最擅长的地方使用它：

1. 路程计编码器 `encoder_data[0]`：直线距离和圆弧长度的最佳主反馈，因为它测量车体实际行进距离，而不是驱动轮打滑后的轮转距离。
2. 左右轮编码器 `encoder_data[1]`、`encoder_data[2]`：轮速内环控制和备用里程估计的最佳来源。
3. IMU 航向角/陀螺仪：航向保持、自旋角度和圆弧角度的最佳来源。
4. IMU 线性加速度：适合短时加速度趋势、碰撞检测和打滑诊断，但不作为主距离来源。

不要盲目信任单个传感器。要做一致性检查。传感器并不是越多越好；只有当某个测量的误差来源清楚、时序可接受，并且在当前运动状态下置信度合适时，才应参与融合。

### 5.2 距离估计

状态估测层内部的计数到 mm 换算示例：

```c
odom_distance_mm = (encoder_data[0].sum_distance - start_odom_count) * ODOMETER_MM_PER_PULSE;

left_distance_mm = (encoder_data[1].sum_distance - start_left_count) / VEHICLE_ENCODER_PULSE_PER_MM;
right_distance_mm = (encoder_data[2].sum_distance - start_right_count) / VEHICLE_ENCODER_PULSE_PER_MM;
wheel_distance_mm = (left_distance_mm + right_distance_mm) * 0.5f;
```

这些换算属于 `userapp_state_estimator.c` 内部职责；MCM 只能消费估测层发布后的字段。

距离融合：

1. 如果路程计正常，用它作为主反馈。
2. 如果路程计异常但两个轮编码器都正常，使用左右轮平均值。
3. 如果两者都正常，计算打滑估计：

```c
slip_error_mm = odom_distance_mm - wheel_distance_mm;
```

打滑估计只作为诊断或小幅修正。驱动轮可能打滑；车体位移应以路程计为主。

IMU 线性加速度也可以由状态估测层预积分：

```c
accel_velocity_mm_s += linear_accel_mm_s2 * dt_s;
accel_distance_mm += accel_velocity_mm_s * dt_s;
```

但是该值应视为低置信度。很小的加速度零偏、振动、重力投影误差和安装角误差都会在二次积分后迅速放大。主要用途应是：

1. 检测碰撞、急刹或异常加速度。
2. 在短时间窗口内检查轮速/路程计距离是否合理。
3. 作为高噪声的弱 EKF 观测或过程提示。

第一版直行或圆弧完成判定中，不应使用 IMU 加速度作为距离主反馈。

推荐：

```c
if (odometer_ok)
    fused_distance_mm = odom_distance_mm;
else
    fused_distance_mm = wheel_distance_mm;
```

高级调试时可用：

```c
fused_distance_mm = 0.85f * odom_distance_mm + 0.15f * wheel_distance_mm;
```

但只有在路程计机械安装稳定时才建议这样做。

推荐距离置信度顺序：

```text
路程计距离 > 左右轮平均距离 > IMU 加速度积分
```

### 5.3 航向和角度估计

使用两个 IMU 角度来源：

1. `imu_data.yaw`：来自 IMU 帧解析的绝对展开航向角。
2. `imu_data.sum_gyroz`：局部积分陀螺仪角度。

短动作中，`sum_gyroz` 延迟低。较长动作中，`yaw` 有助于限制积分漂移。

还应将轮速差积分作为辅助航向来源：

```c
wheel_angle_rad = (right_distance_mm - left_distance_mm) / VEHICLE_TRACK_WIDTH_MM;
wheel_angle_deg = wheel_angle_rad * 180.0f / MCM_PI;
```

状态估测层内部的航向融合示例：

```c
gyro_angle_deg = imu_data.sum_gyroz - start_gyro_sum_deg;
yaw_angle_deg = USER_MCM_AngleDifference(imu_data.yaw, start_yaw_deg);
imu_angle_deg = 0.75f * gyro_angle_deg + 0.25f * yaw_angle_deg;
fused_angle_deg = 0.80f * imu_angle_deg + 0.20f * wheel_angle_deg;
```

该计算属于状态估测层。MCM 应使用 `est->yaw_deg`、`est->omega_deg_s`、`est->wheel_yaw_deg` 和诊断标志，而不是直接读取 `imu_data` 或轮编码器全局变量。

如果 IMU 与轮式航向超过阈值，不要盲目平均。应按当前运动状态选择更可信的传感器，并标记诊断标志：

1. IMU 航向估计有效、轮式航向不一致：优先 IMU，并标记可能轮胎打滑或轮距标定错误。
2. IMU 无效、轮编码器正常：可选使用轮式航向作为降级反馈。
3. 两者反复不一致：报故障或降低速度。

如果估测层检测到 IMU 航向/陀螺仪数据无效，应发布 `yaw_valid = 0`。当 `yaw_valid == 0` 时，MCM 默认不启动自旋或圆弧。直行动作在策略允许时可进入降级模式，只做距离控制。

### 5.4 基于轮速的航向备用估计

轮式航向估计：

```c
wheel_angle_rad = (right_distance_mm - left_distance_mm) / VEHICLE_TRACK_WIDTH_MM;
wheel_angle_deg = wheel_angle_rad * 180.0f / MCM_PI;
```

用途：

1. 检测 IMU 不一致。
2. 如果 IMU 不可用且动作安全风险可接受，作为紧急备用。
3. 当它与 IMU 一致时，以中低权重辅助正常航向估计。
4. 在半径转弯中检查实际圆弧曲率。

第一版实现建议：

1. 自旋和圆弧要求 `yaw_valid != 0`。
2. 轮式航向用于诊断、无进展检查和低权重航向辅助。
3. 稳定测试后，再允许圆弧使用轮式航向作为可选备用模式。

## 6. 运动规划

初始使用梯形速度规划。它与现有实现一致，并且 MCU 计算成本低。

通用标量进度：

1. `target_abs`：目标距离 mm 或目标角度 deg。
2. `profile_rate`：规划速度 mm/s 或 deg/s。
3. `max_rate`：最大速度。
4. `accel`、`decel`：加速度/减速度。
5. `remaining = target_abs - progress`。

每个周期：

```c
stop_distance = profile_rate * profile_rate / (2.0f * decel);

if (stop_distance >= remaining)
    profile_rate -= decel * dt;
else
    profile_rate += accel * dt;

clamp(profile_rate, 0, max_rate);

reference_progress += profile_rate * dt;
clamp(reference_progress, 0, target_abs);
```

使用现有常量作为起点：

1. 直行：`MCM_LINEAR_ACCEL_MM_S2`、`MCM_LINEAR_DECEL_MM_S2`。
2. 自旋：`MCM_ROTATE_ACCEL_DEG_S2`、`MCM_ROTATE_DECEL_DEG_S2`。
3. 圆弧线速度规划：与直行相同，但还要受角速度限制。

圆弧：

```c
max_speed_by_gyro = radius_mm * max_gyro_deg_s * MCM_PI / 180.0f;
effective_max_speed = min(max_speed_mm_s, max_speed_by_gyro);
```

这可以保证线速度限制和角速度限制都被满足。

## 7. 运动学

### 7.1 车体命令

所有动作都生成车体命令：

```c
v_mm_s      /* vehicle center linear speed, forward positive */
omega_deg_s /* yaw angular speed, left/CCW positive */
```

然后将车体命令转换为轮速：

```c
omega_rad_s = omega_deg_s * MCM_PI / 180.0f;
left_speed_mm_s = v_mm_s - omega_rad_s * VEHICLE_TRACK_WIDTH_MM * 0.5f;
right_speed_mm_s = v_mm_s + omega_rad_s * VEHICLE_TRACK_WIDTH_MM * 0.5f;
```

最后转换为每控制周期编码器计数目标，并使用现有轮速 PID。

### 7.2 原地自旋

自旋：

```c
v_mm_s = 0;
omega_deg_s = spin_dir * profile_gyro_deg_s + angle_kp * angle_error_deg;
```

轮速：

```c
left = -omega_rad_s * track / 2;
right = omega_rad_s * track / 2;
```

### 7.3 直线运动

直行：

```c
v_mm_s = drive_dir * profile_speed_mm_s + position_kp * distance_error_mm;
omega_deg_s = heading_kp * heading_error_deg;
```

`heading_error_deg` 是初始航向和当前融合航向之间的差：

```c
heading_error_deg = -fused_angle_deg;
```

`omega_deg_s` 应限制在较小的修正范围，例如 `30 deg/s`，避免距离误差造成激烈转向响应。

### 7.4 圆弧运动

定义约定：

1. `turn_dir = +1`：圆心位于动作开始时车辆左侧。
2. `turn_dir = -1`：圆心位于动作开始时车辆右侧。
3. `drive_dir = +1`：车辆沿圆弧前进。
4. `drive_dir = -1`：车辆沿圆弧后退。

曲率：

```c
kappa = turn_dir / radius_mm;
v_ff = drive_dir * profile_speed_mm_s;
omega_ff_rad_s = v_ff * kappa;
omega_ff_deg_s = omega_ff_rad_s * 180.0f / MCM_PI;
```

这个约定很重要：

1. 前进左转：`v > 0`，`omega > 0`。
2. 前进右转：`v > 0`，`omega < 0`。
3. 后退左转：`v < 0`，`omega < 0`。
4. 后退右转：`v < 0`，`omega > 0`。

这样可以保持几何规则：瞬时圆心位于请求的车辆侧面。

圆弧长度目标：

```c
target_arc_length_mm = radius_mm * abs(arc_angle_deg) * MCM_PI / 180.0f;
```

圆弧进度可由两种方式估计：

1. 距离：`abs(fused_distance_mm)`。
2. 角度：`abs(fused_angle_deg)`。

推荐主完成变量：

```c
progress_angle_deg = abs(fused_angle_deg);
remaining_angle_deg = target_angle_abs_deg - progress_angle_deg;
```

推荐规划变量：

使用距离规划生成 `v`，但受剩余角度约束：

```c
progress_distance_mm = abs(fused_distance_mm);
reference_arc_angle_deg = reference_progress_mm / radius_mm * 180.0f / MCM_PI;
```

圆弧反馈有两个误差：

1. 沿轨迹误差：距离规划值减去测得圆弧长度。
2. 航向/曲率误差：参考圆弧角减去融合偏航角。

控制律：

```c
distance_error_mm = reference_progress_mm - progress_distance_mm;
angle_error_deg = turn_sign_for_motion * reference_arc_angle_deg - fused_angle_deg;

v_cmd = v_ff + drive_dir * arc_distance_kp * distance_error_mm;
omega_cmd_deg_s = omega_ff_deg_s + arc_angle_kp * angle_error_deg;
```

其中：

```c
turn_sign_for_motion = turn_dir * drive_dir;
```

限幅：

```c
abs(v_cmd) <= effective_max_speed
abs(omega_cmd_deg_s) <= max_gyro_deg_s
```

然后将 `(v_cmd, omega_cmd)` 转换为轮速目标。

## 8. 动作细节

### 8.1 直行前进/后退

启动校验：

1. `distance_mm != 0`。
2. `max_speed_mm_s > 0`。
3. 状态估计满足 `distance_valid != 0`。
4. 仅当 `yaw_valid != 0` 时启用航向保持；否则如果策略允许，直行动作可在降级模式下只做距离控制。

启动快照：

1. 从 `USER_STATE_Estimate_t` 保存 `distance_mm`、`x_mm`、`y_mm` 和 `yaw_deg`。
2. 清空速度 PID 状态。
3. 初始化规划器和计时器。

RUN 步骤：

1. 读取最新的 `USER_STATE_Estimate_t`。
2. 由 `est->distance_mm - start_distance_mm` 更新相对距离。
3. 计算进度 `drive_dir * relative_distance_mm`。
3. 更新梯形线速度规划。
4. 计算 `v_cmd`。
5. 如果 `yaw_valid != 0`，由相对航向计算航向修正。
6. 转换为轮速。
7. 运行轮速 PID。

完成条件：

1. `abs(target_distance - relative_distance_mm) <= MCM_LINEAR_DONE_DEADBAND_MM`。
2. `velocity_valid != 0` 且 `abs(est->v_mm_s)` 低于停止阈值。
3. 条件持续满足 `MCM_FINISH_HOLD_CYCLES`。

### 8.2 原地自旋

启动校验：

1. `relative_angle_deg != 0`。
2. `max_gyro_deg_s > 0`。
3. 状态估计满足 `yaw_valid != 0`。
4. 状态估计质量高于配置的最低阈值。

RUN 步骤：

1. 读取最新的 `USER_STATE_Estimate_t`。
2. 由 `est->yaw_deg - start_yaw_deg` 更新相对航向。
3. 进度为 `spin_dir * relative_yaw_deg`。
3. 更新梯形角速度规划。
4. 计算 `omega_cmd`。
5. 转换为相反方向的左右轮速度。
6. 运行轮速 PID。

完成条件：

1. `abs(target_angle - relative_yaw_deg) <= MCM_ROTATE_DEADZONE_DEG`。
2. `abs(est->omega_deg_s) <= MCM_ROTATE_STOP_GYRO_DEG_S`。
3. 可选：命令轮速目标也已衰减到阈值以下。
4. 条件持续满足 `MCM_FINISH_HOLD_CYCLES`。

### 8.3 前进圆弧

参数：

1. `radius_mm > VEHICLE_TRACK_WIDTH_MM / 2`。
2. `arc_angle_deg > 0`。
3. `turn_dir` 为左或右。
4. `drive_dir = USER_MCM_DRIVE_FORWARD`。
5. `max_speed_mm_s > 0`。
6. `max_gyro_deg_s > 0`。

RUN 步骤：

1. 将半径和目标角度转换为目标圆弧长度。
2. 使用状态估计的相对航向作为主角度进度。
3. 使用状态估计的相对距离作为主线性进度。
4. 生成线速度规划，速度同时受 `max_speed_mm_s` 和 `radius * max_gyro` 限制。
5. 前馈：

```c
v_ff = +profile_speed;
omega_ff = turn_dir * v_ff / radius;
```

6. 反馈：

```c
distance_error = reference_arc_length - abs(fused_distance);
angle_error = turn_dir * reference_arc_angle - fused_angle;
```

7. 命令：

```c
v_cmd = v_ff + distance_kp * distance_error;
omega_cmd = omega_ff + angle_kp * angle_error;
```

8. 转换为轮速并运行轮速 PID。

完成条件：

1. `abs(target_angle - abs(relative_yaw_deg)) <= arc_angle_deadband`。
2. `abs(target_arc_length - abs(relative_distance_mm)) <= arc_distance_deadband`。
3. 状态估计的角速度和线速度低于停止阈值。
4. 持续满足 `MCM_FINISH_HOLD_CYCLES`。

### 8.4 后退圆弧

参数与前进圆弧相同，除了：

```c
drive_dir = USER_MCM_DRIVE_BACKWARD;
```

前馈：

```c
v_ff = -profile_speed;
omega_ff = v_ff * turn_dir / radius;
```

示例：

1. 后退左转：`v_ff < 0`，`omega_ff < 0`。
2. 后退右转：`v_ff < 0`，`omega_ff > 0`。

反馈符号必须使用相同约定：

```c
expected_angle = drive_dir * turn_dir * reference_arc_angle;
angle_error = expected_angle - fused_angle;
v_cmd = v_ff + drive_dir * distance_kp * distance_error;
omega_cmd = omega_ff + angle_kp * angle_error;
```

完成条件与前进圆弧相同。

## 9. 稳定性策略

### 9.1 串级控制

使用串级控制：

1. 外层规划器：生成平滑参考进度。
2. 外层反馈：将距离/角度误差转换为车体速度修正。
3. 运动学映射：车体速度转换为轮速。
4. 内层 PID：轮速目标转换为 PWM/电机命令。

这比直接用距离/角度误差控制电机 PWM 更稳定。

### 9.2 限幅顺序

始终按以下顺序限幅：

1. 将规划速度限制到动作最大值。
2. 限制反馈修正后的 `v_cmd` 和 `omega_cmd`。
3. 转换为轮速。
4. 如果任一轮速超过物理最大值，按同一比例缩放两个轮速以保持曲率：

```c
scale = max(abs(left), abs(right)) / max_wheel_speed;
if (scale > 1.0f)
{
    left /= scale;
    right /= scale;
}
```

5. 转换为编码器计数目标。
6. 由速度 PID 输出限幅保护电机输出。

### 9.3 避免突然反向

动作开始时，清空速度 PID 积分和历史误差。

RUN 期间，如果规划速度在目标附近降到零，除非正在完成动作，否则保持命令速度符号与动作方向一致。这样可以避免在零点附近振荡。

### 9.4 完成保持

不要在第一次进入死区时就标记完成。要求稳定保持：

```c
finish_hold_count >= MCM_FINISH_HOLD_CYCLES
```

在 10ms 周期和计数 5 的情况下，表示稳定 50ms。

### 9.5 刹车行为

完成或故障时：

1. 如果需要柔性停车，可以先将目标轮速设为零持续一个或多个周期。
2. 然后使用 `MOTOR_MODE_REGEN_BRAKE`。

第一版实现保持现有行为：

```c
USER_MCM_FreezeAllMotions();
```

## 10. 故障处理

建议每个周期检查：

1. 总超时。
2. 无进展超时。
3. 编码器溢出或未初始化。
4. 对需要 IMU 的动作检查 IMU 故障。
5. 传感器差异过大。
6. 请求半径过小。
7. 轮速命令长时间超出可行范围。

### 10.1 超时

直行：

```c
timeout_ms = base + abs(distance_mm) / max_speed_mm_s * 1000 * scale;
```

自旋：

```c
timeout_ms = base + abs(angle_deg) / max_gyro_deg_s * 1000 * scale;
```

圆弧：

```c
arc_length_mm = radius_mm * abs(angle_deg) * pi / 180;
timeout_by_speed = arc_length_mm / effective_max_speed * 1000;
timeout_by_gyro = abs(angle_deg) / max_gyro_deg_s * 1000;
timeout_ms = base + max(timeout_by_speed, timeout_by_gyro) * scale;
```

### 10.2 无进展

直行：

```c
progress = drive_dir * fused_distance_mm;
```

自旋：

```c
progress = spin_dir * fused_angle_deg;
```

圆弧：

```c
progress = abs(fused_angle_deg);
```

如果进度在 `MCM_NO_PROGRESS_TIMEOUT_MS` 内没有增加一个小死区量，则以 `USER_MCM_STATUS_NO_PROGRESS` 报故障。

### 10.3 传感器不一致

直行：

```c
abs(odom_distance_mm - wheel_distance_mm) > straight_slip_limit_mm
```

圆弧/自旋：

```c
abs(fused_angle_deg - wheel_angle_deg) > angle_disagreement_limit_deg
```

推荐第一版行为：

1. 不要因为单个异常采样立即报故障。
2. 统计连续异常周期数。
3. 只有连续异常 5 到 10 个周期后才报故障。

## 11. 路线层非阻塞执行

MCM 模块非阻塞后，路线代码应使用表格驱动的动作调度器，而不是继续手写路线状态机。路线调度器是位于 MCM 之上的第二层状态机：

```text
race request -> start countdown -> route table scheduler -> MCM start/status API
```

当前比赛启动请求入口是：

```c
void USER_RACE_RequestStart(uint8_t race_route, uint8_t run_mode);
```

不要重新引入 `selected_route` 或 `selected_mode` 这类共享全局变量。

推荐文件：

1. `User_Application/userapp_race_table.h`
2. `User_Application/userapp_race_table.c`

推荐模板路线表：

```c
static const USER_RACE_Action_t race_template_actions[] =
{
    {USER_RACE_ACTION_MOVE_DISTANCE, 500.0f, 300.0f, 3000, 1, 0},
    {USER_RACE_ACTION_ROTATE_ANGLE,  180.0f,  90.0f, 2500, 2, 0},
    {USER_RACE_ACTION_MOVE_DISTANCE, 500.0f, 300.0f, 3000, 3, 0},
    {USER_RACE_ACTION_ROTATE_ANGLE,  180.0f,  90.0f, 2500, 4, 0},
    {USER_RACE_ACTION_END,             0.0f,   0.0f,    0, -1, 0},
};
```

调度器将表格动作映射到统一的 MCM API：

1. `USER_RACE_ACTION_MOVE_DISTANCE` -> `USER_MCM_StartStraight()`
2. `USER_RACE_ACTION_ROTATE_ANGLE` -> `USER_MCM_StartSpin()`
3. 后续圆弧动作 -> `USER_MCM_StartArc()`

路线调度器不得写电机 PWM、轮速目标、原始编码器字段或 IMU 字段。它只负责启动动作、检查 MCM 状态/结果、处理路线超时，并跳转到下一行动作。

## 12. 实施阶段

### Phase 1：非阻塞 MCM 核心、直行和自旋

1. 添加 `USER_MCM_Context_t`。
2. 使用现有 `userapp_state_estimator.h/.c` 输出作为 MCM 唯一反馈来源。
3. 添加 `USER_MCM_Task()`。
4. 添加 `USER_MCM_StartStraight()` 和 `USER_MCM_StartSpin()`。
5. 让 MCM 只消费 `USER_STATE_Estimate_t`。
6. 移除 `main.c` 10ms 周期中直接写速度 PID 目标和电机输出的逻辑，改为先调用 `USER_STATE_Update10ms()`，再调用 `USER_MCM_Task()`。
7. 所有调用方迁移后，从活动 API 中移除旧的阻塞直行/自旋声明。
8. 使用最小测试路径验证直行 500mm 和自旋 180deg。

### Phase 2：表格驱动比赛路线调度器

1. 新增 `User_Application/userapp_race_table.h/.c`。
2. 实现动作表执行器和结果码。
3. 将表格动作映射到 `USER_MCM_StartStraight()`、`USER_MCM_StartSpin()` 和后续 `USER_MCM_StartArc()`。
4. 保留 `USER_RACE_RequestStart()` 作为比赛启动请求入口。
5. 将模板路线转换为动作表。
6. 倒计时结束后周期性调用动作表执行器。
7. 验证路线运行时 OLED、LiDAR、按键和通信任务仍保持响应。

### Phase 3：前进/后退圆弧

1. 添加 `USER_MCM_StartArc()`。
2. 实现半径校验和线速度/角速度限制。
3. 实现圆弧进度和完成判定。
4. 将圆弧动作行接入路线表执行器。
5. 先测试大半径，例如 500mm 和 90deg。
6. 再测试较小半径，但绝不能小于 `track_width / 2 + margin`。

### Phase 4：融合和诊断

1. 将传感器快照保留在状态估测层中，MCM 不再清零累计量。
2. 在状态估测层中添加路程计/轮速/航向差异计数器。
3. 完善估测方法分发，使 weighted/complementary/Kalman/EKF 可以替换 raw/default 方法而不影响 MCM。
4. 将调试字段导出到 OLED 或 Modbus：
   - action type
   - state
   - progress
   - remaining
   - fused distance
   - fused angle
   - left/right target speed
   - last fault

### Phase 5：移除旧阻塞流程

1. 直行、自旋和圆弧动作验证完成后，删除旧的阻塞实现。
2. 移除比赛/应用代码中所有基于 `delay_ms()` 的运动循环。
3. 路线执行只使用表格执行器、动作启动调用、`USER_MCM_Task()` 和状态检查。
4. 打开警告编译，并搜索已移除的旧符号，确认没有遗留调用方。

## 13. 调参顺序

按以下顺序调参：

1. 轮子悬空时调轮速 PID，然后落地调轮速 PID。
2. 不带航向修正的直线距离。
3. 带 IMU 的直线航向修正。
4. 自旋角度控制。
5. 大半径前进圆弧。
6. 大半径后退圆弧。
7. 较小半径圆弧。
8. 超时和无进展阈值。

不要在直行和自旋稳定之前调圆弧控制，因为圆弧控制同时依赖两者。

## 14. 初始常量

推荐起始常量：

```c
#define MCM_HEADING_CORRECTION_MAX_DEG_S       30.0f
#define MCM_ARC_DISTANCE_KP_MM_S_PER_MM        1.0f
#define MCM_ARC_ANGLE_KP_DEG_S_PER_DEG         2.0f
#define MCM_ARC_DONE_DEADBAND_DEG              1.5f
#define MCM_ARC_DONE_DEADBAND_MM               8.0f
#define MCM_SENSOR_DISAGREE_HOLD_CYCLES        8
#define MCM_STRAIGHT_SLIP_LIMIT_MM             40.0f
#define MCM_ANGLE_DISAGREE_LIMIT_DEG           8.0f
#define MCM_MIN_TURN_RADIUS_MARGIN_MM          10.0f
```

使用现有常量：

1. `MCM_CONTROL_PERIOD_MS`
2. `MCM_LINEAR_ACCEL_MM_S2`
3. `MCM_LINEAR_DECEL_MM_S2`
4. `MCM_ROTATE_ACCEL_DEG_S2`
5. `MCM_ROTATE_DECEL_DEG_S2`
6. `MCM_FINISH_HOLD_CYCLES`
7. `MCM_NO_PROGRESS_TIMEOUT_MS`

## 15. 需要确认的关键设计决策

实现前确认这些决策：

1. 半径表示车辆中心线半径，而不是内轮或外轮半径。
2. 正自旋角度沿用新自旋动作中验证过的正偏航电机方向。
3. 左圆弧表示圆心位于动作开始时车辆左侧。
4. 后退左转在上述约定下使用负线速度和负偏航角速度。
5. 第一版实现中，自旋和圆弧要求 `yaw_valid != 0`。
6. 路程计是主距离反馈，除非它报告故障。

这些约定应写入 `userapp_mcm.h` 注释，使调用者不需要从电机行为推断符号。

## 16. 主循环集成

当前 `main.c` 的 10ms 块会直接将速度目标和电机输出覆盖为零目标。这会与非阻塞 MCM 任务冲突。

当前行为：

```c
speed_pid[MOTOR_0_LEFT].target = 0;
speed_pid[MOTOR_1_RIGHT].target = 0;
...
USER_Motor_SetMode(...);
```

推荐行为：

```c
if (USER_MCM_IsBusy())
{
    USER_MCM_Task();
}
else
{
    USER_MCM_IdleTask(); /* optional: zero target or brake */
}
```

或者简单地：

```c
USER_MCM_Task();
```

并让 `USER_MCM_Task()` 在空闲时什么都不做。

路线任务应以 20ms 或 50ms 运行。它只负责启动动作和检查状态；不得直接命令电机。

## 17. 预期收益

1. 车辆运动时主循环仍保持响应。
2. OLED、LiDAR、Modbus、按键和安全检查继续更新。
3. 比赛路线可以被暂停、取消或干净地进入故障状态。
4. 所有动作类型共享同一套完成、超时和电机命令路径。
5. 前进/后退圆弧成为一等动作，而不是单独的特例。
