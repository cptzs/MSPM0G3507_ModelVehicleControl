# Nonblocking Motion Control State Machine Plan

## 1. Goal

Replace the current blocking motion functions completely with a new nonblocking motion-control module that runs once per control period, normally every `MCM_CONTROL_PERIOD_MS = 10ms`. Do not keep compatibility wrappers for the old blocking API.

Required actions:

1. Spin in place by a given relative angle.
2. Move straight forward or backward by a given distance.
3. Drive forward along a left or right radius by a given angle.
4. Drive backward along a left or right radius by a given angle.

Each action must:

1. Be nonblocking.
2. Have stable acceleration, deceleration, finish detection, and timeout protection.
3. Allow a maximum linear speed or maximum angular speed.
4. Fuse IMU gyro/yaw, drive-wheel encoders, and the odometer encoder where each sensor is useful.
5. Continue to use the existing left/right wheel speed PID as the inner loop.

The current code already contains the core pieces:

1. `encoder_data[0]`: odometer encoder, used as vehicle body distance feedback.
2. `encoder_data[1]`: left drive wheel encoder speed and accumulated count.
3. `encoder_data[2]`: right drive wheel encoder speed and accumulated count.
4. `imu_data.gyroz`, `imu_data.yaw`, `imu_data.sum_gyroz`: yaw-rate and yaw feedback.
5. `speed_pid[MOTOR_0_LEFT]`, `speed_pid[MOTOR_1_RIGHT]`: drive-wheel speed PID.
6. The existing blocking functions contain useful trapezoid planning and outer-loop correction ideas, but their public API and blocking execution model will be replaced.

## 2. Architecture

### 2.0 Layer Boundaries

Keep `User_Algorithm` as a pure algorithm layer. It must not depend on concrete project sensors such as `encoder_data`, `imu_data`, odometer channels, motor IDs, or `USER_MCM_*` types.

Recommended responsibility split:

1. `User_Algorithm/userlib_kalman.h/.c`
   - Generic Kalman/EKF math only.
   - Inputs are numeric vectors, matrices, model callbacks, and covariance parameters.
   - Outputs are estimated state vectors and covariance matrices.
   - No direct access to IMU, encoder, motor, SysTick, OLED, or MCM globals.

2. `User_Application/userapp_state_estimator.h/.c`
   - Vehicle-specific state estimation layer.
   - Reads IMU, drive-wheel encoder, and odometer data.
   - Converts sensor readings to a unified state estimate.
   - In the first version, may perform only raw passthrough, unit conversion, and validity tagging.
   - Later, may convert sensor readings to generic complementary-filter or Kalman/EKF input vectors.
   - Applies vehicle model, units, sign conventions, sensor validity checks, and quality flags.
   - Publishes a vehicle state estimate to MCM.

3. `User_Application/userapp_mcm.h/.c`
   - Motion-control state machine.
   - Consumes only the state-estimator output.
   - Does not read raw physical sensors directly.
   - Does not call low-level Kalman matrix operations directly.

This keeps the algorithm library reusable and testable. The vehicle-specific fusion policy belongs above the pure algorithm layer.

### 2.1 Module Layout

Replace the existing blocking motion API with a new nonblocking API. The old blocking entry points should be removed from `userapp_mcm.h` and all callers should move to the new start/task/status model.

Recommended files:

1. `User_Application/userapp_mcm.h`
2. `User_Application/userapp_mcm.c`

Recommended new public API:

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

`USER_MCM_Task()` is called from the 10ms periodic block in `main.c` and must replace direct speed PID writes from `main.c`. The module owns motor commands both while busy and while applying its idle policy.

Naming rule:

1. Use `USER_MCM_Task()` as the only 10ms MCM update function.
2. Use `USER_MCM_Cancel()` for user/application cancellation.
3. Keep `USER_MCM_FreezeAllMotions()` as the low-level immediate brake helper.
4. Use `USER_MCM_StartStraight()`, `USER_MCM_StartSpin()`, and `USER_MCM_StartArc()` as the active motion start API.
5. Do not introduce parallel names such as `USER_MCM_Update10ms()`, `USER_MCM_Stop()`, `USER_MCM_StartMoveDistance()`, or `USER_MCM_StartRotateAngle()` unless the whole document and codebase are renamed consistently.

MCM input rule:

```text
sensor drivers -> state estimator -> MCM
```

MCM must not read `encoder_data[]`, `imu_data`, or other raw physical sensor globals directly. Even when no advanced estimator is enabled, the state-estimator layer should publish a raw-passthrough estimate with converted units, validity flags, and quality fields.

Current project status:

1. `User_Application/userapp_state_estimator.h/.c` already exists.
2. The estimator already supports a selectable method switch with raw/default, weighted, complementary, Kalman, and EKF entries.
3. The nonblocking MCM work should refine this estimator output and consume it; it should not recreate a separate sensor adapter inside MCM.

### 2.2 Why One State Machine

All four actions are the same control problem:

1. A scalar progress variable moves from `0` to a target.
2. A profile generator computes a smooth reference speed.
3. A feedback term corrects tracking error.
4. A kinematic mapper converts body command `(v, omega)` into left/right wheel speed targets.
5. The existing wheel speed PID drives the motors.

The differences are only:

1. What progress means: distance in mm or angle in deg.
2. What constraints apply: max linear speed or max angular speed.
3. How `(v, omega)` is produced.
4. Which feedback sensor is primary.

So the implementation should use one state machine and small per-action calculation functions.

## 3. Core Data Structures

Recommended internal context:

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

Use `static USER_MCM_Context_t mcm;` in `userapp_mcm.c`.

Important rule: MCM snapshots only the state-estimator output. It must not snapshot raw sensor counters or clear global sensor accumulators.

```c
const USER_STATE_Estimate_t *est = USER_STATE_GetEstimate();

mcm.start_distance_mm = est->distance_mm;
mcm.start_x_mm = est->x_mm;
mcm.start_y_mm = est->y_mm;
mcm.start_yaw_deg = est->yaw_deg;
```

During an action, MCM computes relative motion from the same unified estimate:

```c
mcm.relative_distance_mm = est->distance_mm - mcm.start_distance_mm;
mcm.relative_x_mm = est->x_mm - mcm.start_x_mm;
mcm.relative_y_mm = est->y_mm - mcm.start_y_mm;
mcm.relative_yaw_deg = USER_MCM_AngleDifference(est->yaw_deg, mcm.start_yaw_deg);
```

Raw sensor snapshots and sensor-specific delta calculations belong inside the state-estimator layer. This avoids side effects where one action resets a sensor accumulator that another debug view or module is using.

## 4. Periodic Execution

Call flow every 10ms:

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

The race application must also become nonblocking. Instead of:

```c
USER_MCM_MoveDistance(500, 300);
USER_MCM_RotateAngle(180, 90);
```

use a route state machine:

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

## 5. Sensor Fusion Strategy

### 5.0 Estimator Boundary

The fusion strategy described below is vehicle-specific. If Kalman/EKF is used, the generic filter implementation should stay in `User_Algorithm`, while the sensor-to-state adapter should stay outside `User_Algorithm`.

Generic filter input example:

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

State-estimator sensor input example:

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

State-estimator output example:

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

The MCM layer consumes `USER_STATE_Estimate_t` or an equivalent state-estimator output. The Kalman library never sees concrete project globals.

### 5.1 Sensor Roles

Use each sensor where it is strongest:

1. Odometer encoder `encoder_data[0]`: best primary feedback for straight distance and arc length, because it measures vehicle body travel instead of drive wheel slip.
2. Left/right wheel encoders `encoder_data[1]`, `encoder_data[2]`: best for inner wheel-speed control and backup odometry.
3. IMU yaw/gyro: best for heading and spin/arc angle.
4. IMU linear acceleration: useful for short-term acceleration trend, impact detection, and slip diagnostics, but not a primary distance source.

Do not trust one sensor blindly. Use consistency checks. More sensors are not automatically better; a measurement should be fused only when its error source is understood, its timing is acceptable, and its confidence is appropriate for the current motion state.

### 5.2 Distance Estimate

Estimator-layer example for converting counts to mm:

```c
odom_distance_mm = (encoder_data[0].sum_distance - start_odom_count) * ODOMETER_MM_PER_PULSE;

left_distance_mm = (encoder_data[1].sum_distance - start_left_count) / VEHICLE_ENCODER_PULSE_PER_MM;
right_distance_mm = (encoder_data[2].sum_distance - start_right_count) / VEHICLE_ENCODER_PULSE_PER_MM;
wheel_distance_mm = (left_distance_mm + right_distance_mm) * 0.5f;
```

This conversion belongs inside `userapp_state_estimator.c`; MCM must consume only the published estimate fields.

Distance fusion:

1. If odometer is OK, use it as primary.
2. If odometer is not OK but both wheel encoders are OK, use wheel average.
3. If both are OK, compute slip estimate:

```c
slip_error_mm = odom_distance_mm - wheel_distance_mm;
```

Use slip only as a diagnostic or as a small correction. The drive wheels can slip; the odometer should remain primary for body displacement.

IMU linear acceleration can also be pre-integrated by the state-estimator layer:

```c
accel_velocity_mm_s += linear_accel_mm_s2 * dt_s;
accel_distance_mm += accel_velocity_mm_s * dt_s;
```

However, this value should be low confidence. Small acceleration bias, vibration, gravity projection error, and mounting angle error grow quickly after double integration. Use it mainly for:

1. Detecting impact, sudden braking, or abnormal acceleration.
2. Checking whether wheel/odometer distance is plausible during short windows.
3. Providing a weak EKF observation or process cue with high noise.

Do not use IMU acceleration as the first-version distance feedback for straight or arc completion.

Recommended:

```c
if (odometer_ok)
    fused_distance_mm = odom_distance_mm;
else
    fused_distance_mm = wheel_distance_mm;
```

For advanced tuning:

```c
fused_distance_mm = 0.85f * odom_distance_mm + 0.15f * wheel_distance_mm;
```

only if the odometer mounting is mechanically stable.

Recommended distance confidence order:

```text
odometer distance > wheel-average distance > IMU acceleration integration
```

### 5.3 Heading and Angle Estimate

Use two IMU angle sources:

1. `imu_data.yaw`: absolute unwrapped yaw from IMU frame parsing.
2. `imu_data.sum_gyroz`: local integrated gyro angle.

For short actions, `sum_gyroz` has low latency. For longer actions, `yaw` helps bound integration drift.

Wheel-speed difference should also be integrated as an auxiliary yaw source:

```c
wheel_angle_rad = (right_distance_mm - left_distance_mm) / VEHICLE_TRACK_WIDTH_MM;
wheel_angle_deg = wheel_angle_rad * 180.0f / MCM_PI;
```

Estimator-layer example for fused angle when IMU and wheel yaw are both plausible:

```c
gyro_angle_deg = imu_data.sum_gyroz - start_gyro_sum_deg;
yaw_angle_deg = USER_MCM_AngleDifference(imu_data.yaw, start_yaw_deg);
imu_angle_deg = 0.75f * gyro_angle_deg + 0.25f * yaw_angle_deg;
fused_angle_deg = 0.80f * imu_angle_deg + 0.20f * wheel_angle_deg;
```

This calculation belongs inside the state-estimator layer. MCM should use `est->yaw_deg`, `est->omega_deg_s`, `est->wheel_yaw_deg`, and diagnostic flags instead of reading `imu_data` or wheel encoder globals directly.

If IMU and wheel yaw disagree beyond a threshold, do not average blindly. Prefer the sensor that is more plausible for the motion state and mark a diagnostic flag:

1. IMU yaw estimate valid, wheel yaw disagrees: prefer IMU and mark possible wheel slip or wheelbase calibration error.
2. IMU invalid, wheel encoders OK: optionally use wheel yaw as degraded feedback.
3. Both disagree repeatedly: fault or reduce speed.

If the estimator detects that IMU yaw/gyro data is invalid, it should publish `yaw_valid = 0`. MCM should not start spin or arc by default when `yaw_valid == 0`. For straight motion, MCM may run distance-only in degraded mode if policy allows it.

### 5.4 Wheel-Based Yaw Backup

Wheel yaw estimate:

```c
wheel_angle_rad = (right_distance_mm - left_distance_mm) / VEHICLE_TRACK_WIDTH_MM;
wheel_angle_deg = wheel_angle_rad * 180.0f / MCM_PI;
```

Use this for:

1. Detecting IMU inconsistency.
2. Emergency fallback if IMU is unavailable and the action is not safety critical.
3. Assisting normal yaw estimation with low-to-medium confidence when it agrees with IMU.
4. Checking actual arc curvature during radius turns.

Recommended for first implementation:

1. Spin and arc require `yaw_valid != 0`.
2. Wheel yaw is used for diagnostics, no-progress checks, and low-weight yaw assistance.
3. After stable testing, allow an optional fallback mode for arcs using wheel yaw.

## 6. Motion Profile

Use a trapezoid profile initially. It matches the existing implementation and is cheap for the MCU.

Generic scalar progress:

1. `target_abs`: target distance mm or target angle deg.
2. `profile_rate`: planned speed mm/s or deg/s.
3. `max_rate`: maximum speed.
4. `accel`, `decel`: acceleration/deceleration.
5. `remaining = target_abs - progress`.

Per tick:

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

Use the existing constants as starting values:

1. Straight: `MCM_LINEAR_ACCEL_MM_S2`, `MCM_LINEAR_DECEL_MM_S2`.
2. Spin: `MCM_ROTATE_ACCEL_DEG_S2`, `MCM_ROTATE_DECEL_DEG_S2`.
3. Arc linear profile: same as straight, but also clamp by angular speed.

For arcs:

```c
max_speed_by_gyro = radius_mm * max_gyro_deg_s * MCM_PI / 180.0f;
effective_max_speed = min(max_speed_mm_s, max_speed_by_gyro);
```

This guarantees both linear speed and angular speed limits are respected.

## 7. Kinematics

### 7.1 Body Command

All actions produce body command:

```c
v_mm_s      /* vehicle center linear speed, forward positive */
omega_deg_s /* yaw angular speed, left/CCW positive */
```

Then convert body command to wheel speeds:

```c
omega_rad_s = omega_deg_s * MCM_PI / 180.0f;
left_speed_mm_s = v_mm_s - omega_rad_s * VEHICLE_TRACK_WIDTH_MM * 0.5f;
right_speed_mm_s = v_mm_s + omega_rad_s * VEHICLE_TRACK_WIDTH_MM * 0.5f;
```

Finally convert to encoder counts per control period and use existing wheel speed PID.

### 7.2 Spin In Place

For spin:

```c
v_mm_s = 0;
omega_deg_s = spin_dir * profile_gyro_deg_s + angle_kp * angle_error_deg;
```

Wheel speeds:

```c
left = -omega_rad_s * track / 2;
right = omega_rad_s * track / 2;
```

### 7.3 Straight Motion

For straight:

```c
v_mm_s = drive_dir * profile_speed_mm_s + position_kp * distance_error_mm;
omega_deg_s = heading_kp * heading_error_deg;
```

`heading_error_deg` is the difference between the initial heading and current fused heading:

```c
heading_error_deg = -fused_angle_deg;
```

`omega_deg_s` should be clamped to a small correction limit, for example `30 deg/s`, so a distance error does not produce an aggressive steering response.

### 7.4 Arc Motion

Define conventions:

1. `turn_dir = +1`: circle center is on the left side of the vehicle at action start.
2. `turn_dir = -1`: circle center is on the right side.
3. `drive_dir = +1`: vehicle moves forward along the arc.
4. `drive_dir = -1`: vehicle moves backward along the arc.

Curvature:

```c
kappa = turn_dir / radius_mm;
v_ff = drive_dir * profile_speed_mm_s;
omega_ff_rad_s = v_ff * kappa;
omega_ff_deg_s = omega_ff_rad_s * 180.0f / MCM_PI;
```

This convention is important:

1. Forward-left: `v > 0`, `omega > 0`.
2. Forward-right: `v > 0`, `omega < 0`.
3. Backward-left: `v < 0`, `omega < 0`.
4. Backward-right: `v < 0`, `omega > 0`.

That preserves the geometric rule that the instantaneous circle center is on the requested side of the vehicle.

Arc length target:

```c
target_arc_length_mm = radius_mm * abs(arc_angle_deg) * MCM_PI / 180.0f;
```

Arc progress can be estimated from either:

1. Distance: `abs(fused_distance_mm)`.
2. Angle: `abs(fused_angle_deg)`.

Recommended primary completion variable:

```c
progress_angle_deg = abs(fused_angle_deg);
remaining_angle_deg = target_angle_abs_deg - progress_angle_deg;
```

Recommended profile variable:

Use distance profile for `v`, but cap it by remaining angle:

```c
progress_distance_mm = abs(fused_distance_mm);
reference_arc_angle_deg = reference_progress_mm / radius_mm * 180.0f / MCM_PI;
```

Arc feedback has two errors:

1. Along-track error: distance profile minus measured arc length.
2. Heading/curvature error: reference arc angle minus fused yaw angle.

Control law:

```c
distance_error_mm = reference_progress_mm - progress_distance_mm;
angle_error_deg = turn_sign_for_motion * reference_arc_angle_deg - fused_angle_deg;

v_cmd = v_ff + drive_dir * arc_distance_kp * distance_error_mm;
omega_cmd_deg_s = omega_ff_deg_s + arc_angle_kp * angle_error_deg;
```

Where:

```c
turn_sign_for_motion = turn_dir * drive_dir;
```

Clamp:

```c
abs(v_cmd) <= effective_max_speed
abs(omega_cmd_deg_s) <= max_gyro_deg_s
```

Then convert `(v_cmd, omega_cmd)` to wheel speed targets.

## 8. Action Details

### 8.1 Straight Forward/Backward

Start validation:

1. `distance_mm != 0`.
2. `max_speed_mm_s > 0`.
3. State estimate has `distance_valid != 0`.
4. Heading hold is enabled only when `yaw_valid != 0`; otherwise straight motion may run distance-only in degraded mode if policy allows it.

Start snapshot:

1. Store `distance_mm`, `x_mm`, `y_mm`, and `yaw_deg` from `USER_STATE_Estimate_t`.
2. Clear speed PID states.
3. Initialize profile and timers.

RUN step:

1. Read the latest `USER_STATE_Estimate_t`.
2. Update relative distance from `est->distance_mm - start_distance_mm`.
3. Compute progress `drive_dir * relative_distance_mm`.
3. Update trapezoid linear profile.
4. Compute `v_cmd`.
5. If `yaw_valid != 0`, compute heading correction from relative yaw.
6. Convert to wheel speeds.
7. Run wheel speed PID.

Finish condition:

1. `abs(target_distance - relative_distance_mm) <= MCM_LINEAR_DONE_DEADBAND_MM`.
2. `velocity_valid != 0` and `abs(est->v_mm_s)` is below the stop threshold.
3. Condition holds for `MCM_FINISH_HOLD_CYCLES`.

### 8.2 Spin In Place

Start validation:

1. `relative_angle_deg != 0`.
2. `max_gyro_deg_s > 0`.
3. State estimate has `yaw_valid != 0`.
4. State estimate quality is above the configured minimum.

RUN step:

1. Read the latest `USER_STATE_Estimate_t`.
2. Update relative yaw from `est->yaw_deg - start_yaw_deg`.
3. Progress is `spin_dir * relative_yaw_deg`.
3. Update trapezoid angular profile.
4. Compute `omega_cmd`.
5. Convert to opposite wheel speeds.
6. Run wheel speed PID.

Finish condition:

1. `abs(target_angle - relative_yaw_deg) <= MCM_ROTATE_DEADZONE_DEG`.
2. `abs(est->omega_deg_s) <= MCM_ROTATE_STOP_GYRO_DEG_S`.
3. Optional: commanded wheel speed targets have also decayed below threshold.
4. Condition holds for `MCM_FINISH_HOLD_CYCLES`.

### 8.3 Forward Arc

Parameters:

1. `radius_mm > VEHICLE_TRACK_WIDTH_MM / 2`.
2. `arc_angle_deg > 0`.
3. `turn_dir` left or right.
4. `drive_dir = USER_MCM_DRIVE_FORWARD`.
5. `max_speed_mm_s > 0`.
6. `max_gyro_deg_s > 0`.

RUN step:

1. Convert radius and target angle to target arc length.
2. Use state-estimated relative yaw as primary angular progress.
3. Use state-estimated relative distance as primary linear progress.
4. Generate linear profile with speed limited by both `max_speed_mm_s` and `radius * max_gyro`.
5. Feed-forward:

```c
v_ff = +profile_speed;
omega_ff = turn_dir * v_ff / radius;
```

6. Feedback:

```c
distance_error = reference_arc_length - abs(fused_distance);
angle_error = turn_dir * reference_arc_angle - fused_angle;
```

7. Command:

```c
v_cmd = v_ff + distance_kp * distance_error;
omega_cmd = omega_ff + angle_kp * angle_error;
```

8. Convert to wheel speeds and run wheel speed PID.

Finish condition:

1. `abs(target_angle - abs(relative_yaw_deg)) <= arc_angle_deadband`.
2. `abs(target_arc_length - abs(relative_distance_mm)) <= arc_distance_deadband`.
3. State-estimated angular and linear speeds are below stop thresholds.
4. Hold for `MCM_FINISH_HOLD_CYCLES`.

### 8.4 Backward Arc

Parameters are the same as forward arc, except:

```c
drive_dir = USER_MCM_DRIVE_BACKWARD;
```

Feed-forward:

```c
v_ff = -profile_speed;
omega_ff = v_ff * turn_dir / radius;
```

Examples:

1. Backward-left: `v_ff < 0`, `omega_ff < 0`.
2. Backward-right: `v_ff < 0`, `omega_ff > 0`.

Feedback signs must use the same convention:

```c
expected_angle = drive_dir * turn_dir * reference_arc_angle;
angle_error = expected_angle - fused_angle;
v_cmd = v_ff + drive_dir * distance_kp * distance_error;
omega_cmd = omega_ff + angle_kp * angle_error;
```

Finish condition is identical to forward arc.

## 9. Stability Strategy

### 9.1 Cascaded Control

Use cascaded loops:

1. Outer profile: smooth reference progress.
2. Outer feedback: distance/angle error to body speed correction.
3. Kinematic mapping: body speed to wheel speed.
4. Inner PID: wheel speed target to PWM/motor command.

This is more stable than directly commanding motor PWM from distance/angle error.

### 9.2 Saturation Order

Always clamp in this order:

1. Clamp profile speed to action max.
2. Clamp feedback-corrected `v_cmd` and `omega_cmd`.
3. Convert to wheel speeds.
4. If either wheel speed exceeds physical max, scale both wheel speeds by the same ratio to preserve curvature:

```c
scale = max(abs(left), abs(right)) / max_wheel_speed;
if (scale > 1.0f)
{
    left /= scale;
    right /= scale;
}
```

5. Convert to encoder-count targets.
6. Let speed PID output clamps protect motor output.

### 9.3 No Sudden Direction Reversal

At action start, clear speed PID integral and last errors.

During RUN, if the profile speed reaches zero near the target, keep the sign of the commanded speed consistent with the action direction unless finishing. This avoids oscillation around zero.

### 9.4 Finish Hold

Do not mark done on the first tick inside the deadband. Require stable hold:

```c
finish_hold_count >= MCM_FINISH_HOLD_CYCLES
```

At 10ms and count 5, this means 50ms stable.

### 9.5 Brake Behavior

When done or fault:

1. Set target wheel speeds to zero for one or more ticks if a soft stop is desired.
2. Then use `MOTOR_MODE_REGEN_BRAKE`.

For first implementation, keep existing behavior:

```c
USER_MCM_FreezeAllMotions();
```

## 10. Fault Handling

Recommended checks each tick:

1. Total timeout.
2. No progress timeout.
3. Encoder overflow or uninitialized.
4. IMU fault for actions that require IMU.
5. Sensor disagreement too large.
6. Requested radius too small.
7. Wheel command exceeds feasible limit for too long.

### 10.1 Timeout

Straight:

```c
timeout_ms = base + abs(distance_mm) / max_speed_mm_s * 1000 * scale;
```

Spin:

```c
timeout_ms = base + abs(angle_deg) / max_gyro_deg_s * 1000 * scale;
```

Arc:

```c
arc_length_mm = radius_mm * abs(angle_deg) * pi / 180;
timeout_by_speed = arc_length_mm / effective_max_speed * 1000;
timeout_by_gyro = abs(angle_deg) / max_gyro_deg_s * 1000;
timeout_ms = base + max(timeout_by_speed, timeout_by_gyro) * scale;
```

### 10.2 No Progress

For straight:

```c
progress = drive_dir * fused_distance_mm;
```

For spin:

```c
progress = spin_dir * fused_angle_deg;
```

For arc:

```c
progress = abs(fused_angle_deg);
```

If progress does not increase by a small deadband for `MCM_NO_PROGRESS_TIMEOUT_MS`, fault with `USER_MCM_STATUS_NO_PROGRESS`.

### 10.3 Sensor Disagreement

Straight:

```c
abs(odom_distance_mm - wheel_distance_mm) > straight_slip_limit_mm
```

Arc/spin:

```c
abs(fused_angle_deg - wheel_angle_deg) > angle_disagreement_limit_deg
```

Recommended first behavior:

1. Do not immediately fault on one bad sample.
2. Count consecutive bad ticks.
3. Fault only after 5 to 10 consecutive bad ticks.

## 11. Route-Level Nonblocking Execution

After the MCM module is nonblocking, route code should use a table-driven action scheduler instead of hand-written route state machines. The route scheduler is a second state machine above MCM:

```text
race request -> start countdown -> route table scheduler -> MCM start/status API
```

The current race request entry point is:

```c
void USER_RACE_RequestStart(uint8_t race_route, uint8_t run_mode);
```

Do not reintroduce shared globals such as `selected_route` or `selected_mode`.

Recommended files:

1. `User_Application/userapp_race_table.h`
2. `User_Application/userapp_race_table.c`

Recommended template route table:

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

The scheduler maps table actions to the canonical MCM API:

1. `USER_RACE_ACTION_MOVE_DISTANCE` -> `USER_MCM_StartStraight()`
2. `USER_RACE_ACTION_ROTATE_ANGLE` -> `USER_MCM_StartSpin()`
3. Later arc actions -> `USER_MCM_StartArc()`

The route scheduler must not write motor PWM, wheel speed targets, raw encoder fields, or IMU fields. It only starts actions, checks MCM state/status, handles route timeout, and advances to the next table row.

## 12. Implementation Phases

### Phase 1: Nonblocking MCM Core, Straight, and Spin

1. Add `USER_MCM_Context_t`.
2. Use the existing `userapp_state_estimator.h/.c` output as the only MCM feedback source.
3. Add `USER_MCM_Task()`.
4. Add `USER_MCM_StartStraight()` and `USER_MCM_StartSpin()`.
5. Make MCM consume only `USER_STATE_Estimate_t`.
6. Remove `main.c` direct speed PID target writes and motor writes from the 10ms block; call `USER_STATE_Update10ms()` and then `USER_MCM_Task()` instead.
7. Remove the old blocking straight/spin declarations from the active API after all callers are migrated.
8. Verify straight 500mm and spin 180deg with a minimal test path.

### Phase 2: Table-Driven Race Route Scheduler

1. Add `User_Application/userapp_race_table.h/.c`.
2. Implement the action table executor and result codes.
3. Map table actions to `USER_MCM_StartStraight()`, `USER_MCM_StartSpin()`, and later `USER_MCM_StartArc()`.
4. Keep `USER_RACE_RequestStart()` as the race request entry point.
5. Convert the template route to an action table.
6. Call the table executor periodically after the countdown has completed.
7. Verify that OLED, LiDAR, buttons, and communication tasks remain responsive while the route is running.

### Phase 3: Arc Forward/Backward

1. Add `USER_MCM_StartArc()`.
2. Implement radius validation and linear/angular speed limiting.
3. Implement arc progress and finish detection.
4. Add arc action rows to the route table executor.
5. Test large radius first, for example 500mm and 90deg.
6. Then test smaller radius, but never below `track_width / 2 + margin`.

### Phase 4: Fusion and Diagnostics

1. Keep sensor snapshots inside the state estimator instead of clearing accumulators from MCM.
2. Add odometer/wheel/yaw disagreement counters in the state estimator.
3. Refine the estimator method switch so weighted/complementary/Kalman/EKF methods can replace the raw/default method without changing MCM.
4. Export debug fields to OLED or Modbus:
   - action type
   - state
   - progress
   - remaining
   - fused distance
   - fused angle
   - left/right target speed
   - last fault

### Phase 5: Remove Old Blocking Flow

1. Delete the old blocking implementations after straight, spin, and arc actions are validated.
2. Remove any remaining `delay_ms()` based motion loops from race/application code.
3. Make route execution use only the table executor, action start calls, `USER_MCM_Task()`, and status checks.
4. Build with warnings enabled and search for removed old symbols to confirm no stale callers remain.

## 13. Tuning Order

Tune in this order:

1. Wheel speed PID with wheels lifted, then on ground.
2. Straight distance without heading correction.
3. Straight heading correction with IMU.
4. Spin angle control.
5. Large-radius forward arc.
6. Large-radius backward arc.
7. Smaller-radius arcs.
8. Timeout and no-progress thresholds.

Do not tune arc control before straight and spin are stable, because arc control uses both.

## 14. Initial Constants

Recommended starting constants:

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

Use existing constants:

1. `MCM_CONTROL_PERIOD_MS`
2. `MCM_LINEAR_ACCEL_MM_S2`
3. `MCM_LINEAR_DECEL_MM_S2`
4. `MCM_ROTATE_ACCEL_DEG_S2`
5. `MCM_ROTATE_DECEL_DEG_S2`
6. `MCM_FINISH_HOLD_CYCLES`
7. `MCM_NO_PROGRESS_TIMEOUT_MS`

## 15. Key Design Decisions to Confirm

Before implementation, confirm these decisions:

1. Radius means vehicle centerline radius, not inner-wheel or outer-wheel radius.
2. Positive spin angle follows the verified positive-yaw motor direction used by the new spin action.
3. Left arc means the circle center is on the vehicle's left side at action start.
4. Backward-left uses negative linear speed and negative yaw rate under the convention above.
5. Spin and arc require `yaw_valid != 0` for the first implementation.
6. Odometer is primary distance feedback unless it reports fault.

These conventions should be written into `userapp_mcm.h` comments so callers do not have to infer signs from motor behavior.

## 16. Main-Loop Integration

Current `main.c` 10ms block directly overwrites speed targets and motor outputs with zero targets. That will conflict with a nonblocking MCM task.

Current behavior:

```c
speed_pid[MOTOR_0_LEFT].target = 0;
speed_pid[MOTOR_1_RIGHT].target = 0;
...
USER_Motor_SetMode(...);
```

Recommended behavior:

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

Or simply:

```c
USER_MCM_Task();
```

and let `USER_MCM_Task()` do nothing while idle.

The route task should run at 20ms or 50ms. It only starts actions and checks status; it must not directly command motors.

## 17. Expected Benefits

1. The main loop stays responsive while the vehicle moves.
2. OLED, LiDAR, Modbus, buttons, and safety checks continue to update.
3. Race routes can be paused, canceled, or faulted cleanly.
4. All action types share the same finish, timeout, and motor-command path.
5. Arc forward/backward becomes a first-class action instead of a separate special case.
