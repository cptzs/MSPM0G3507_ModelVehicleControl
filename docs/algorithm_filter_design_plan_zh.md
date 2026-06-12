# 纯算法滤波器设计计划

## 1. 目的

为后续运动控制工作设计可复用的纯算法滤波模块。模块需要同时支持简单互补滤波和 Kalman 系列滤波，同时保持与物理传感器、电机、MCM 动作和工程全局变量隔离。

本文档只面向 `User_Algorithm` 层。它有意不定义如何读取 IMU、轮编码器、路程计通道或电机状态。这些转换应属于算法库之上的适配层。

主要目标：

1. 提供轻量级互补滤波库，用于快速、可靠的第一阶段姿态或信号估计。
2. 提供通用 Kalman/EKF 库，后续可供车辆姿态估计器使用。
3. 保持两个库都与传感器无关、与应用无关。
4. 让输出适配未来的非阻塞 MCM 状态机。
5. 避免动态内存分配，在 MSPM0 上保持 CPU/RAM 使用可预测。

## 2. 分层

推荐划分：

```text
User_Algorithm/
  userlib_complementary_filter.h/.c
  userlib_kalman.h/.c

User_Application/
  userapp_state_estimator.h/.c  mandatory state-estimator layer
  userapp_mcm.h/.c              motion controller consuming only state estimates
```

`User_Algorithm` 的严格规则：

1. 不依赖 `encoder_data`。
2. 不依赖 `imu_data`。
3. 不依赖电机 ID 或电机模式。
4. 不依赖 `sysTick`、OLED、Modbus、LiDAR 或 MCM 路线状态。
5. 除非单位是通用函数名的一部分，例如 `dt_s`，否则不直接假设物理单位。

适配层负责：

1. 读取物理传感器。
2. 将计数转换为物理量。
3. 应用符号约定。
4. 选择运行哪个滤波器。
5. 将通用滤波输出映射为 MCM 友好的状态。
6. 即使没有启用高级滤波器，也发布 raw-passthrough 估计。

MCM 必须只消费适配层/状态估测层输出。它不得直接读取原始传感器全局变量。

## 3. 后续 MCM 需求

未来 MCM 状态机需要类似以下估计量：

```c
typedef struct
{
    float x;
    float y;
    float heading;
    float linear_velocity;
    float angular_velocity;
    float heading_bias;
    float quality;
} USER_STATE_Estimate_t;
```

具体单位应由适配层选择，而不是由算法库选择。对当前车辆，适配层可能使用：

1. `x`、`y`：mm。
2. `heading`：deg 或 rad，但在适配层内部统一选择一种。
3. `linear_velocity`：mm/s。
4. `angular_velocity`：deg/s 或 rad/s。
5. `heading_bias`：deg/s 或 rad/s。
6. `quality`：0.0 到 1.0 的归一化质量，或离散质量标志。

算法层输出保持通用：

```c
float x[KALMAN_MAX_STATE_DIM];
float P[KALMAN_MAX_STATE_DIM][KALMAN_MAX_STATE_DIM];
```

或者小型互补滤波状态：

```c
float value;
float bias;
float rate;
```

适配层将这些映射为 MCM 估计结构。

## 4. 互补滤波设计

### 4.0 多源融合原则

测量源并不是越多越好。只有满足以下条件时，才应融合某个来源：

1. 它的误差来源与其他来源不同。
2. 它的时序足够明确。
3. 它的比例和符号已经标定。
4. 它的预期噪声可以用合理权重表示。
5. 纯算法层之上有异常值或不一致处理策略。

对车辆运动来说，IMU 线性加速度有用，但用于距离估计时通常应是低置信度，因为二次积分会放大零偏和振动。轮速差对航向辅助更有价值，因为它受到差速车运动学直接约束，但在打滑时仍然可能错误。

### 4.1 角色

互补滤波应作为短距离车辆动作的第一实现选择，因为它简单、稳定、计算成本低且容易调试。

使用场景：

1. 融合积分速率和低频绝对测量。
2. 由陀螺仪角速度和绝对航向估计 heading。
3. 由快速增量输入和慢速绝对修正估计速度。
4. 当主航向与轮速差航向一致时，以低权重融合轮速差航向。
5. 保留 IMU 加速度积分作为弱趋势输入，而不是主距离来源。
6. 在 Kalman/EKF 禁用时提供备用方案。

### 4.2 通用 1D 互补滤波器

状态：

```c
typedef struct
{
    float value;
    float rate;
    float bias;
    float alpha;
    float bias_alpha;
    float min_value;
    float max_value;
    uint8_t wrap_enable;
} USER_CF1D_Context_t;
```

API：

```c
void USER_CF1D_Init(USER_CF1D_Context_t *ctx,
                    float initial_value,
                    float alpha,
                    float bias_alpha);

void USER_CF1D_SetLimits(USER_CF1D_Context_t *ctx,
                         float min_value,
                         float max_value,
                         uint8_t wrap_enable);

float USER_CF1D_Update(USER_CF1D_Context_t *ctx,
                       float rate_input,
                       float absolute_input,
                       uint8_t absolute_valid,
                       float dt_s);

float USER_CF1D_GetValue(const USER_CF1D_Context_t *ctx);
float USER_CF1D_GetRate(const USER_CF1D_Context_t *ctx);
float USER_CF1D_GetBias(const USER_CF1D_Context_t *ctx);
```

更新逻辑：

```text
predicted = value + (rate_input - bias) * dt

if absolute_valid:
    error = absolute_input - predicted
    if wrap_enable:
        error = normalized_shortest_error(absolute_input, predicted)
    value = predicted + (1 - alpha) * error
    bias = bias + bias_alpha * error / dt
else:
    value = predicted

rate = rate_input - bias
```

含义：

1. `alpha` 接近 1.0 表示更信任积分。
2. `alpha` 更低表示更信任绝对测量。
3. `bias_alpha` 应该较小；偏置必须缓慢适应。

推荐起始值：

```c
alpha = 0.96f to 0.995f
bias_alpha = 0.001f to 0.02f
```

### 4.3 通用 2D 航位推算辅助器

互补滤波本身不定义车辆位置。增加一个通用平面积分辅助器，接收已经融合好的速度和航向。

状态：

```c
typedef struct
{
    float x;
    float y;
    float heading;
} USER_PLANAR_DR_Context_t;
```

API：

```c
void USER_PLANAR_DR_Init(USER_PLANAR_DR_Context_t *ctx,
                         float x0,
                         float y0,
                         float heading0);

void USER_PLANAR_DR_Update(USER_PLANAR_DR_Context_t *ctx,
                           float linear_velocity,
                           float angular_velocity,
                           float dt_s);

void USER_PLANAR_DR_CorrectPosition(USER_PLANAR_DR_Context_t *ctx,
                                    float x_meas,
                                    float y_meas,
                                    float correction_gain);
```

这个辅助器仍然是纯算法代码，因为它不知道 `linear_velocity` 或 `angular_velocity` 来自哪里。

适配层可以从路程计或左右轮平均值提供 `linear_velocity`。它可以从陀螺仪、轮速差或融合结果提供 `angular_velocity`。算法辅助器不为这些来源分配信任度。

### 4.4 通用加权融合辅助器

在完整 Kalman/EKF 启用前，小型加权融合辅助器很有用。

```c
typedef struct
{
    float value;
    float weight;
    uint8_t valid;
} USER_BLEND_Input_t;

float USER_BLEND_WeightedAverage(const USER_BLEND_Input_t *inputs,
                                 uint8_t count,
                                 float fallback_value);
```

适配层可将它用于如下场景：

```text
heading = 0.80 * imu_heading + 0.20 * wheel_heading
```

但前提是适配层已经确认两者差异没有超过配置阈值。

### 4.5 互补滤波优点

1. CPU 和 RAM 成本很低。
2. 容易在实车上调参。
3. 对短直行、自旋和圆弧动作足够稳健。
4. 故障模式容易理解。
5. 如果 EKF 不稳定或禁用，可作为备用。

### 4.6 互补滤波局限

1. 没有协方差估计。
2. 缺少严格方法来处理不同噪声水平的多个测量。
3. 偏置估计是启发式的。
4. 无法严格处理 `x/y/heading/bias` 这类耦合状态。
5. 传感器不一致需要额外逻辑。

## 5. Kalman 滤波设计

### 5.1 角色

Kalman 系列滤波器应作为通用库存在，供后续车辆状态估计使用。第一版 MCM 实现不应要求 EKF，但架构应允许在状态估测层准备好后启用。

使用场景：

1. 估计航向和陀螺仪偏置。
2. 估计位置、航向、速度和偏置。
3. 融合具有不同噪声假设的多个测量。
4. 输出残差和协方差，用于质量评估。

状态估测层测量可以包括：

1. 来自路程计的高置信度距离或速度。
2. 来自驱动轮平均值的中等置信度线速度。
3. 来自陀螺仪的高置信度短时角速度。
4. 来自 IMU 绝对航向的中等置信度 yaw。
5. 来自轮速差的中低置信度 yaw 或角速度。
6. 来自 IMU 加速度的低置信度线性加速度，主要用于趋势和故障检测。

Kalman 库并不知道这些含义。适配层通过 `R`、`Q` 和测量选择表达信任度。

### 5.2 固定最大维度

避免动态内存分配。使用固定最大维度：

```c
#define USER_KALMAN_MAX_STATE_DIM 6
#define USER_KALMAN_MAX_INPUT_DIM 4
#define USER_KALMAN_MAX_MEAS_DIM 6
```

上下文按最大常量存储矩阵，但运行时使用 `state_dim`、`input_dim` 和 `measure_dim`。

### 5.3 线性 Kalman 上下文

```c
typedef struct
{
    uint8_t state_dim;
    uint8_t input_dim;
    uint8_t measure_dim;

    float x[USER_KALMAN_MAX_STATE_DIM];
    float P[USER_KALMAN_MAX_STATE_DIM][USER_KALMAN_MAX_STATE_DIM];

    float F[USER_KALMAN_MAX_STATE_DIM][USER_KALMAN_MAX_STATE_DIM];
    float B[USER_KALMAN_MAX_STATE_DIM][USER_KALMAN_MAX_INPUT_DIM];
    float Q[USER_KALMAN_MAX_STATE_DIM][USER_KALMAN_MAX_STATE_DIM];

    float H[USER_KALMAN_MAX_MEAS_DIM][USER_KALMAN_MAX_STATE_DIM];
    float R[USER_KALMAN_MAX_MEAS_DIM][USER_KALMAN_MAX_MEAS_DIM];

    float y[USER_KALMAN_MAX_MEAS_DIM];
    float S[USER_KALMAN_MAX_MEAS_DIM][USER_KALMAN_MAX_MEAS_DIM];
    float K[USER_KALMAN_MAX_STATE_DIM][USER_KALMAN_MAX_MEAS_DIM];
} USER_KALMAN_Context_t;
```

API：

```c
uint8_t USER_KALMAN_Init(USER_KALMAN_Context_t *ctx,
                         uint8_t state_dim,
                         uint8_t input_dim,
                         uint8_t measure_dim);

uint8_t USER_KALMAN_Predict(USER_KALMAN_Context_t *ctx,
                            const float *u);

uint8_t USER_KALMAN_Update(USER_KALMAN_Context_t *ctx,
                           const float *z);

void USER_KALMAN_SetState(USER_KALMAN_Context_t *ctx,
                          const float *x,
                          const float *P_diag);
```

调用方设置 `F`、`B`、`Q`、`H` 和 `R`。库只执行数学计算。

### 5.4 EKF 上下文和回调

EKF 需要模型回调。保持回调通用：

```c
typedef void (*USER_EKF_StateFunc)(const float *x,
                                   const float *u,
                                   float dt_s,
                                   float *x_pred,
                                   void *user);

typedef void (*USER_EKF_StateJacobianFunc)(const float *x,
                                           const float *u,
                                           float dt_s,
                                           float F[][USER_KALMAN_MAX_STATE_DIM],
                                           void *user);

typedef void (*USER_EKF_MeasureFunc)(const float *x,
                                     float *z_pred,
                                     void *user);

typedef void (*USER_EKF_MeasureJacobianFunc)(const float *x,
                                             float H[][USER_KALMAN_MAX_STATE_DIM],
                                             void *user);
```

上下文：

```c
typedef struct
{
    USER_KALMAN_Context_t kf;
    USER_EKF_StateFunc f;
    USER_EKF_StateJacobianFunc jac_f;
    USER_EKF_MeasureFunc h;
    USER_EKF_MeasureJacobianFunc jac_h;
    void *user;
} USER_EKF_Context_t;
```

API：

```c
uint8_t USER_EKF_Init(USER_EKF_Context_t *ctx,
                      uint8_t state_dim,
                      uint8_t input_dim,
                      uint8_t measure_dim,
                      USER_EKF_StateFunc f,
                      USER_EKF_StateJacobianFunc jac_f,
                      USER_EKF_MeasureFunc h,
                      USER_EKF_MeasureJacobianFunc jac_h,
                      void *user);

uint8_t USER_EKF_Predict(USER_EKF_Context_t *ctx,
                         const float *u,
                         float dt_s);

uint8_t USER_EKF_Update(USER_EKF_Context_t *ctx,
                        const float *z);
```

`user` 指针可以指向模型参数，例如轮距，但 EKF 库将其视为不透明数据。

推荐第一批车辆 EKF 实验在算法层之外实现：

```text
Stage A state: [heading, gyro_bias]
Measurements: gyro-integrated heading, absolute yaw, wheel-difference yaw

Stage B state: [x, y, heading, velocity, gyro_bias]
Measurements: odometer distance/velocity, wheel-average velocity, IMU yaw, gyro rate, optional IMU acceleration
```

IMU 加速度应设置较大的测量噪声，或者只作为过程提示。它不应压过路程计或轮式距离信息。

### 5.5 矩阵运算

需要的内部运算：

1. 矩阵置零/单位阵。
2. 矩阵复制。
3. 矩阵加/减。
4. 矩阵乘法。
5. 矩阵转置。
6. 矩阵-向量乘法。
7. 测量协方差 `S` 的小矩阵求逆。

为了可靠性，第一版先支持 `1x1`、`2x2`、`3x3`，以及最大到 `USER_KALMAN_MAX_MEAS_DIM` 的通用 Gauss-Jordan。

如果求逆失败或维度非法，返回错误。

## 6. 算法质量输出

纯算法层可以提供通用质量指标：

```c
typedef struct
{
    float innovation_norm;
    float covariance_trace;
    uint8_t update_accepted;
    uint8_t numerical_error;
} USER_KALMAN_Diagnostics_t;
```

算法库不应判定某个具体 IMU 或编码器坏了。它只报告数学诊断。适配层决定传感器健康策略。

## 7. 后续 MCM 如何使用

未来 MCM 流程应类似：

```text
physical sensors
    -> mandatory state-estimator layer
        -> optional complementary filter or EKF from User_Algorithm
            -> unified state estimate
                -> nonblocking MCM action controller
```

MCM 控制器只应看到：

```c
typedef struct
{
    float x_mm;
    float y_mm;
    float yaw_deg;
    float distance_mm;
    float v_mm_s;
    float omega_deg_s;
    uint8_t distance_valid;
    uint8_t yaw_valid;
    uint8_t velocity_valid;
    uint8_t degraded;
    uint8_t quality;
} USER_STATE_Estimate_t;
```

这样 MCM 控制器就与所选滤波方法无关。

即使没有启用任何滤波器，状态估测层仍然用 raw-passthrough 换算和有效性标志发布同一个结构。这样从第一版实现开始，MCM 就与物理传感器解耦。

## 8. 建议开发顺序

### Phase 1：互补滤波库

1. 实现 `USER_CF1D_Context_t`。
2. 实现带可选环绕处理的 1D 更新。
3. 实现简单平面航位推算辅助器。
4. 添加小型离线测试或调试测试函数。

### Phase 2：使用互补滤波的状态估测层

1. 状态估测层读取工程传感器。
2. 状态估测层将计数和 IMU 数值转换为统一估计。
3. 首先提供不带高级滤波的 raw-passthrough 输出。
4. 然后可选地将部分字段接入互补滤波器。
5. 状态估测层输出 `USER_STATE_Estimate_t`。
6. MCM 只消费 `USER_STATE_Estimate_t`。

### Phase 3：通用 Kalman 库

1. 实现固定尺寸矩阵辅助函数。
2. 实现线性 KF predict/update。
3. 添加诊断信息。
4. 在用于车辆前，用小型已知系统测试。

### Phase 4：EKF 支持

1. 添加 EKF 回调接口。
2. 用回调实现 predict/update。
3. 将车辆模型保留在算法库之外。
4. 由状态估测层提供模型回调和测量。

### Phase 5：车辆 EKF 实验

1. 先估计 `[heading, gyro_bias]`。
2. 再扩展到 `[x, y, heading, velocity, gyro_bias]`。
3. 比较 EKF 输出与互补滤波输出。
4. 只有在 EKF 明显更好后，才在 MCM 中启用。

## 9. 利弊

### 互补滤波

优点：

1. 简单且确定性强。
2. 足够便宜，可每 10ms 周期运行。
3. 容易调参和调试。
4. 适合短动作。
5. 支持实用的低权重轮速差航向辅助。

缺点：

1. 调参偏启发式。
2. 诊断能力有限。
3. 不建模耦合不确定性。
4. 没有外部修正时，IMU 加速度距离估计仍然容易漂移。

### Kalman/EKF

优点：

1. 统一状态和协方差。
2. 当模型和噪声取值正确时，多传感器融合效果更好。
3. 可以估计偏置。
4. 提供创新量和协方差诊断。

缺点：

1. 实现复杂度更高。
2. 对符号、单位、模型和时序错误更敏感。
3. 需要仔细调整噪声。
4. 普通 KF/EKF 不会自动对轮胎打滑鲁棒。
5. CPU/RAM 成本更高。
6. 如果低估二次积分加速度这类弱传感器的噪声，加入它们反而可能让估计变差。

## 10. 关键决策

1. `User_Algorithm` 只提供纯滤波基础组件。
2. 车辆专用传感器融合策略位于 `User_Algorithm` 之外。
3. MCM 消费估计状态，而不是原始 Kalman 内部量。
4. 应先实现互补滤波。
5. EKF 应作为可选功能，后续实验性启用。
6. 滤波器库不使用动态内存分配。
7. 单位和物理符号约定由适配层负责。
8. IMU 加速度是距离估计的低置信度辅助输入，不是默认主来源。
9. 轮速差是有用的辅助航向来源，但适配层必须在打滑时拒绝或降低其权重。
