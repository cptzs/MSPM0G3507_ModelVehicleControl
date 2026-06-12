# Pure Algorithm Filter Design Plan

## 1. Purpose

Design reusable pure-algorithm filtering modules for future motion-control work. The modules must support both simple complementary filtering and Kalman-family filters, while staying independent from physical sensors, motors, MCM actions, and project globals.

This document is for the `User_Algorithm` layer only. It intentionally does not define how to read IMU, wheel encoders, odometer channels, or motor state. Those conversions belong in an adapter layer above the algorithm library.

Primary goals:

1. Provide a lightweight complementary-filter library for fast, reliable first-stage pose or signal estimation.
2. Provide a generic Kalman/EKF library that can later be used by a vehicle pose estimator.
3. Keep both libraries sensor-agnostic and application-agnostic.
4. Make the outputs suitable for a future nonblocking MCM state machine.
5. Avoid dynamic allocation and keep CPU/RAM use predictable on MSPM0.

## 2. Layering

Recommended split:

```text
User_Algorithm/
  userlib_complementary_filter.h/.c
  userlib_kalman.h/.c

User_Application/
  userapp_state_estimator.h/.c  mandatory state-estimator layer
  userapp_mcm.h/.c              motion controller consuming only state estimates
```

Strict rule for `User_Algorithm`:

1. No dependency on `encoder_data`.
2. No dependency on `imu_data`.
3. No dependency on motor IDs or motor modes.
4. No dependency on `sysTick`, OLED, Modbus, LiDAR, or MCM route state.
5. No direct physical units assumption unless the unit is part of a generic function name, for example `dt_s`.

The adapter layer is responsible for:

1. Reading physical sensors.
2. Converting counts to physical values.
3. Applying sign conventions.
4. Choosing which filter to run.
5. Mapping generic filter outputs into MCM-friendly state.
6. Publishing a raw-passthrough estimate even when no advanced filter is enabled.

MCM must consume only the adapter/state-estimator output. It must not read raw sensor globals directly.

## 3. Future MCM Needs

The future MCM state machine will need estimates like:

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

The exact units should be selected by the adapter, not by the algorithm library. For the current vehicle, the adapter will likely use:

1. `x`, `y`: mm.
2. `heading`: deg or rad, but choose one inside the adapter.
3. `linear_velocity`: mm/s.
4. `angular_velocity`: deg/s or rad/s.
5. `heading_bias`: deg/s or rad/s.
6. `quality`: normalized 0.0 to 1.0 or a discrete quality flag.

Algorithm layer output remains generic:

```c
float x[KALMAN_MAX_STATE_DIM];
float P[KALMAN_MAX_STATE_DIM][KALMAN_MAX_STATE_DIM];
```

or a small complementary-filter state:

```c
float value;
float bias;
float rate;
```

The adapter maps these to the MCM estimate structure.

## 4. Complementary Filter Design

### 4.0 Multi-Source Fusion Principle

More measurements are not automatically better. A source should be fused only when:

1. Its error source is different from the other sources.
2. Its timing is known well enough.
3. Its scale and sign are calibrated.
4. Its expected noise can be represented by a reasonable weight.
5. There is an outlier or disagreement policy above the pure algorithm layer.

For vehicle motion, linear acceleration from an IMU is useful but should usually be low confidence for distance because double integration amplifies bias and vibration. Wheel-speed difference is more useful for yaw assistance because it is directly constrained by differential-drive kinematics, but it can still be wrong during slip.

### 4.1 Role

Complementary filters should be the first implementation choice for short-distance vehicle actions because they are simple, stable, cheap, and easy to debug.

Use cases:

1. Fuse integrated rate with low-frequency absolute measurement.
2. Estimate heading from gyro rate and absolute yaw.
3. Estimate velocity from fast incremental input and slow absolute correction.
4. Fuse primary yaw with low-weight wheel-difference yaw when they agree.
5. Keep IMU acceleration integration available as a weak trend input, not as a primary distance source.
6. Provide a fallback when Kalman/EKF is disabled.

### 4.2 Generic 1D Complementary Filter

State:

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

API:

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

Update logic:

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

Interpretation:

1. `alpha` close to 1.0 trusts integration more.
2. `alpha` lower trusts absolute measurement more.
3. `bias_alpha` should be small; bias must adapt slowly.

Recommended starting values:

```c
alpha = 0.96f to 0.995f
bias_alpha = 0.001f to 0.02f
```

### 4.3 Generic 2D Dead-Reckoning Helper

Complementary filtering alone does not define vehicle position. Add a generic planar integration helper that accepts already-fused velocity and heading.

State:

```c
typedef struct
{
    float x;
    float y;
    float heading;
} USER_PLANAR_DR_Context_t;
```

API:

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

This helper is still pure algorithm code because it does not know where `linear_velocity` or `angular_velocity` came from.

The adapter may provide `linear_velocity` from odometer or wheel average. It may provide `angular_velocity` from gyro, wheel-speed difference, or a fused result. The algorithm helper does not assign trust to those sources.

### 4.4 Generic Weighted Blend Helper

A small weighted blend helper is useful before full Kalman/EKF is enabled.

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

The adapter can use this for cases such as:

```text
heading = 0.80 * imu_heading + 0.20 * wheel_heading
```

only after it verifies that the two sources do not disagree beyond a configured threshold.

### 4.5 Complementary Filter Advantages

1. Very low CPU and RAM cost.
2. Easy to tune on the vehicle.
3. Robust enough for short straight, spin, and arc actions.
4. Failure modes are easy to understand.
5. Useful as a fallback if EKF is unstable or disabled.

### 4.6 Complementary Filter Limitations

1. No covariance estimate.
2. No principled way to handle multiple measurements with different noise levels.
3. Bias estimation is heuristic.
4. Cross-coupled states such as `x/y/heading/bias` are not handled rigorously.
5. Sensor disagreement needs separate logic.

## 5. Kalman Filter Design

### 5.1 Role

Kalman-family filters should be available as a generic library for later vehicle state estimation. The first MCM implementation should not require EKF, but the architecture should allow it to be enabled when the state-estimator layer is ready.

Use cases:

1. Estimate heading and gyro bias.
2. Estimate position, heading, velocity, and bias.
3. Fuse multiple measurements with different noise assumptions.
4. Output residuals and covariance for quality assessment.

State-estimator measurements can include:

1. High-confidence distance or velocity from an odometer.
2. Medium-confidence linear velocity from drive-wheel average.
3. High-confidence short-term angular rate from gyro.
4. Medium-confidence yaw from absolute IMU yaw.
5. Medium-low-confidence yaw or angular rate from wheel-speed difference.
6. Low-confidence linear acceleration from IMU acceleration, mainly for trend and fault detection.

The Kalman library does not know these meanings. The adapter expresses trust through `R`, `Q`, and measurement selection.

### 5.2 Fixed Maximum Dimensions

Avoid dynamic allocation. Use fixed maximum dimensions:

```c
#define USER_KALMAN_MAX_STATE_DIM 6
#define USER_KALMAN_MAX_INPUT_DIM 4
#define USER_KALMAN_MAX_MEAS_DIM 6
```

The context stores matrices sized by the max constants, but uses runtime `state_dim`, `input_dim`, and `measure_dim`.

### 5.3 Linear Kalman Context

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

API:

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

The caller sets `F`, `B`, `Q`, `H`, and `R`. The library only performs math.

### 5.4 EKF Context and Callbacks

EKF needs model callbacks. Keep the callbacks generic:

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

Context:

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

API:

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

The `user` pointer may point to model parameters, such as wheelbase, but the EKF library treats it as opaque.

Recommended first vehicle EKF experiments, implemented outside the algorithm layer:

```text
Stage A state: [heading, gyro_bias]
Measurements: gyro-integrated heading, absolute yaw, wheel-difference yaw

Stage B state: [x, y, heading, velocity, gyro_bias]
Measurements: odometer distance/velocity, wheel-average velocity, IMU yaw, gyro rate, optional IMU acceleration
```

IMU acceleration should receive a large measurement noise value or be used only as a process cue. It should not dominate odometer or wheel-distance information.

### 5.5 Matrix Operations

Needed internal operations:

1. Matrix set zero/identity.
2. Matrix copy.
3. Matrix add/subtract.
4. Matrix multiply.
5. Matrix transpose.
6. Matrix-vector multiply.
7. Small matrix inverse for measurement covariance `S`.

For reliability, first support only `1x1`, `2x2`, `3x3`, and generic Gauss-Jordan up to `USER_KALMAN_MAX_MEAS_DIM`.

Return error if inverse fails or dimensions are invalid.

## 6. Algorithm Quality Outputs

The pure algorithm layer can provide generic quality indicators:

```c
typedef struct
{
    float innovation_norm;
    float covariance_trace;
    uint8_t update_accepted;
    uint8_t numerical_error;
} USER_KALMAN_Diagnostics_t;
```

The algorithm library should not decide that a specific IMU or encoder is bad. It only reports mathematical diagnostics. The adapter decides sensor health policy.

## 7. How MCM Will Use This Later

The future MCM flow should look like:

```text
physical sensors
    -> mandatory state-estimator layer
        -> optional complementary filter or EKF from User_Algorithm
            -> unified state estimate
                -> nonblocking MCM action controller
```

The MCM controller should only see:

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

This keeps the MCM controller independent from the chosen filtering method.

Even when no filter is enabled, the state-estimator layer still publishes the same structure using raw-passthrough conversion and validity flags. This keeps MCM decoupled from physical sensors from the first implementation.

## 8. Suggested Development Order

### Phase 1: Complementary Filter Library

1. Implement `USER_CF1D_Context_t`.
2. Implement 1D update with optional wrap handling.
3. Implement simple planar dead-reckoning helper.
4. Add small offline tests or debug test functions.

### Phase 2: State Estimator With Complementary Filter

1. State-estimator layer reads project sensors.
2. State-estimator layer converts counts and IMU values into the unified estimate.
3. First provide raw-passthrough output without advanced filtering.
4. Then optionally route selected fields through complementary filters.
5. State-estimator layer outputs `USER_STATE_Estimate_t`.
6. MCM consumes only `USER_STATE_Estimate_t`.

### Phase 3: Generic Kalman Library

1. Implement fixed-size matrix helpers.
2. Implement linear KF predict/update.
3. Add diagnostics.
4. Test with small known systems before vehicle use.

### Phase 4: EKF Support

1. Add EKF callback interface.
2. Implement predict/update using callbacks.
3. Keep vehicle model outside the algorithm library.
4. Use the state-estimator layer to provide model callbacks and measurements.

### Phase 5: Vehicle EKF Experiment

1. First estimate `[heading, gyro_bias]`.
2. Then extend to `[x, y, heading, velocity, gyro_bias]`.
3. Compare EKF output against complementary filter output.
4. Enable EKF in MCM only after it is demonstrably better.

## 9. Pros and Cons

### Complementary Filter

Pros:

1. Simple and deterministic.
2. Cheap enough for every 10ms cycle.
3. Easy to tune and debug.
4. Good fit for short actions.
5. Supports practical low-weight yaw assistance from wheel-speed difference.

Cons:

1. Heuristic tuning.
2. Limited diagnostics.
3. Does not model coupled uncertainty.
4. IMU acceleration distance estimates remain drift-prone without external correction.

### Kalman/EKF

Pros:

1. Unified state and covariance.
2. Better multi-sensor fusion when models and noise values are correct.
3. Can estimate bias.
4. Provides innovation and covariance diagnostics.

Cons:

1. More implementation complexity.
2. More sensitive to sign, unit, model, and timing errors.
3. Requires careful noise tuning.
4. Ordinary KF/EKF is not automatically robust to wheel slip.
5. Higher CPU/RAM cost.
6. Adding weak sensors such as double-integrated acceleration can make estimates worse if their noise is underestimated.

## 10. Key Decisions

1. `User_Algorithm` provides only pure filtering primitives.
2. Vehicle-specific sensor fusion policy lives outside `User_Algorithm`.
3. MCM consumes an estimated state, not raw Kalman internals.
4. Complementary filtering should be implemented first.
5. EKF should be optional and experimentally enabled later.
6. No dynamic allocation in the filter libraries.
7. Units and physical sign conventions are adapter responsibilities.
8. IMU acceleration is a low-confidence auxiliary input for distance, not a default primary source.
9. Wheel-speed difference is a useful auxiliary yaw source, but the adapter must reject or downweight it during slip.
