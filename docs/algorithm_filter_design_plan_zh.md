# 纯算法滤波器设计计划

> 状态：📋 设计阶段，尚未实现
> 目标目录：`User_Algorithm/`

## 1. 目的

为运动控制系统设计可复用的纯算法滤波模块。模块需同时支持简单互补滤波和 Kalman 系列滤波，同时保持与物理传感器、电机、MCM 动作和工程全局变量隔离。

## 2. 当前状态

- ✅ **PID 控制器**：已实现（`userlib_pid.c`），含位置式/增量式/高级 PID + 斜坡控制
- 🔄 **状态估测器**：已实现 raw-passthrough 估测（`userapp_state_estimator.c`），预留了 Weighted/Complementary/Kalman/EKF 入口
- ❌ **互补滤波器**：未实现 — 待加入 `userlib_complementary_filter.c/h`
- ❌ **Kalman/EKF**：未实现 — 待加入 `userlib_kalman.c/h`

## 3. 分层

推荐划分：

```text
User_Algorithm/
  userlib_complementary_filter.h/.c   ← 1D/2D 互补滤波器 + 航位推算辅助器
  userlib_kalman.h/.c                 ← 通用 Kalman/EKF 矩阵运算

User_Application/
  userapp_state_estimator.c/h         ← 车辆专用适配层（传感器→物理量→滤波器输入）
  userapp_mcm.c/h                     ← 运动控制器（只消费状态估计输出）
```

### 严格规则（User_Algorithm）

1. 不依赖 `encoder_data`、`imu_data`
2. 不依赖电机 ID 或电机模式
3. 不依赖 `sysTick`、OLED、Modbus、LiDAR
4. 不假设物理单位（单位由调用方在上层适配）
5. 不做动态内存分配

### 适配层职责（userapp_state_estimator.c）

1. 读取物理传感器
2. 将计数转换为物理量
3. 应用符号约定
4. 选择运行哪个滤波器
5. 将通用滤波输出映射为 MCM 友好的状态
6. 即使没有启用高级滤波器，也发布 raw-passthrough 估计

## 4. 互补滤波设计

### 4.1 通用 1D 互补滤波器

状态：

```c
typedef struct {
    float value;
    float rate;
    float bias;
    float alpha;          // 信任速率积分的权重（0~1，越大越信任）
    float bias_alpha;     // 偏置估计速率
    float min_value;
    float max_value;
    uint8_t wrap_enable;  // 角度回绕使能
} USER_CF1D_Context_t;
```

API：

```c
void USER_CF1D_Init(ctx, initial_value, alpha, bias_alpha);
void USER_CF1D_SetLimits(ctx, min_value, max_value, wrap_enable);
float USER_CF1D_Update(ctx, rate_input, absolute_input, absolute_valid, dt_s);
float USER_CF1D_GetValue(ctx);
float USER_CF1D_GetRate(ctx);
float USER_CF1D_GetBias(ctx);
```

更新逻辑：

```text
predicted = value + (rate_input - bias) * dt
if absolute_valid:
    error = absolute_input - predicted
    if wrap_enable: error = normalized_shortest_error()
    value = predicted + (1 - alpha) * error
    bias = bias + bias_alpha * error / dt
else:
    value = predicted
rate = rate_input - bias
```

推荐起始值：`alpha = 0.96~0.995`, `bias_alpha = 0.001~0.02`

### 4.2 通用 2D 航位推算辅助器

```c
typedef struct {
    float x, y, heading;
} USER_PLANAR_DR_Context_t;

void USER_PLANAR_DR_Init(ctx, x0, y0, heading0);
void USER_PLANAR_DR_Update(ctx, linear_velocity, angular_velocity, dt_s);
void USER_PLANAR_DR_CorrectPosition(ctx, x_meas, y_meas, correction_gain);
```

### 4.3 通用加权融合辅助器

```c
typedef struct {
    float value;
    float weight;
    uint8_t valid;
} USER_BLEND_Input_t;

float USER_BLEND_WeightedAverage(const inputs[], count, fallback_value);
```

## 5. Kalman 滤波设计

### 5.1 固定最大维度

```c
#define USER_KALMAN_MAX_STATE_DIM 6
#define USER_KALMAN_MAX_INPUT_DIM 4
#define USER_KALMAN_MAX_MEAS_DIM 6
```

### 5.2 线性 Kalman 上下文

```c
typedef struct {
    uint8_t state_dim, input_dim, measure_dim;
    float x[KALMAN_MAX_STATE_DIM];
    float P[KALMAN_MAX_STATE_DIM][KALMAN_MAX_STATE_DIM];
    float F[KALMAN_MAX_STATE_DIM][KALMAN_MAX_STATE_DIM];
    float B[KALMAN_MAX_STATE_DIM][KALMAN_MAX_INPUT_DIM];
    float Q[KALMAN_MAX_STATE_DIM][KALMAN_MAX_STATE_DIM];
    float H[KALMAN_MAX_MEAS_DIM][KALMAN_MAX_STATE_DIM];
    float R[KALMAN_MAX_MEAS_DIM][KALMAN_MAX_MEAS_DIM];
    // 临时工作区
    float y[KALMAN_MAX_MEAS_DIM];
    float S[KALMAN_MAX_MEAS_DIM][KALMAN_MAX_MEAS_DIM];
    float K[KALMAN_MAX_STATE_DIM][KALMAN_MAX_MEAS_DIM];
} USER_KALMAN_Context_t;
```

API：

```c
uint8_t USER_KALMAN_Init(ctx, state_dim, input_dim, measure_dim);
uint8_t USER_KALMAN_Predict(ctx, u);
uint8_t USER_KALMAN_Update(ctx, z);
void USER_KALMAN_SetState(ctx, x, P_diag);
```

### 5.3 EKF 上下文和回调

```c
typedef void (*USER_EKF_StateFunc)(x, u, dt_s, x_pred, user);
typedef void (*USER_EKF_StateJacobianFunc)(x, u, dt_s, F, user);
typedef void (*USER_EKF_MeasureFunc)(x, z_pred, user);
typedef void (*USER_EKF_MeasureJacobianFunc)(x, H, user);

typedef struct {
    USER_KALMAN_Context_t kf;
    USER_EKF_StateFunc f;
    USER_EKF_StateJacobianFunc jac_f;
    USER_EKF_MeasureFunc h;
    USER_EKF_MeasureJacobianFunc jac_h;
    void *user;
} USER_EKF_Context_t;
```

### 5.4 矩阵运算需求

1. 矩阵置零/单位阵
2. 矩阵复制
3. 矩阵加/减
4. 矩阵乘法
5. 矩阵转置
6. 矩阵-向量乘法
7. 小矩阵求逆（Gauss-Jordan, 1×1~6×6）

## 6. 在状态估测层中的融合策略

### 6.1 距离估计

优先级：路程计 > 左右轮平均 > IMU 加速度积分

```c
if (odometer_ok)
    distance = odom_distance_mm;
else
    distance = wheel_distance_mm;
```

### 6.2 航向估计

```c
// 短动作：sum_gyroz（低延迟）
// 长动作：yaw（限制漂移）
// 后备：左右轮差速积分
heading = imu_yaw_deg;  // 当 IMU 可用时
```

### 6.3 Kalman 实验阶段

- **Stage A**: 状态 `[heading, gyro_bias]`，测量：陀螺仪积分航向、绝对 yaw、轮速差航向
- **Stage B**: 状态 `[x, y, heading, velocity, gyro_bias]`，测量：里程计距离/速度、轮平均速度、IMU yaw

## 7. 实现优先级

1. ✅ PID 控制器（已完成）
2. 🔄 1D 互补滤波器 — 下一优先级
3. 🔄 2D 航位推算辅助器
4. 🔄 加权融合辅助器
5. 🔄 线性 Kalman（2 状态 heading+bias）
6. 🔄 EKF（4-6 状态）
