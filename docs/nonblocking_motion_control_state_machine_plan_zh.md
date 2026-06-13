# 非阻塞运动控制状态机方案

> 状态：✅ 已实现（基于最新代码 2026-06-13）
> 实现文件：`User_Application/userapp_mcm.c/h`

## 1. 目标与现状

已将阻塞式运动函数彻底替换为全新的非阻塞运动控制模块。每个控制周期（10ms）运行一次。

### 已支持的动作

| 动作 | API | 状态 |
|------|-----|------|
| 直行前进/后退 | `USER_MCM_StartStraight(distance_mm, max_speed_mm_s)` | ✅ 已实现 |
| 原地自旋 | `USER_MCM_StartSpin(angle_deg, max_gyro_deg_s)` | ✅ 已实现 |
| 圆弧运动 | `USER_MCM_StartArc(...)` | 🔄 预留接口 |

### 设计特点

- ✅ 非阻塞
- ✅ 梯形速度/角速度规划（加速→匀速→减速）
- ✅ 航向保持修正（直行动作）
- ✅ 完成判定（距离/角度死区 + 速度归零）
- ✅ 超时保护 + 无进展保护
- ✅ 使用左右轮速度 PID 作为内环
- ✅ 融合 IMU 陀螺仪/航向角、路程计编码器、驱动轮编码器

## 2. 架构

### 2.1 分层边界

```text
sensor drivers → State Estimator → MCM → speed PID → motors
```

MCM **不得**直接读取 `encoder_data[]`、`imu_data` 或其他原始传感器全局变量。
只消费状态估测层发布的 `USER_STATE_Estimate_t`。

### 2.2 核心数据结构

```c
typedef struct {
    USER_MCM_ActionType_t type;        // NONE / STRAIGHT / SPIN / ARC
    USER_MCM_State_t state;            // IDLE / START / RUN / DONE / FAULT
    USER_MCM_Status_t status;          // OK / BUSY / TIMEOUT / NO_PROGRESS / ...

    int32_t drive_dir;                 // +1=forward, -1=backward
    int32_t spin_dir;                  // 旋转方向

    float target_abs;                  // 目标距离(mm)或角度(deg)的绝对值
    float target_signed;               // 带符号目标值
    float max_rate;                    // 最大速率

    float start_distance_mm;           // 动作开始时快照的路程计距离
    float start_x_mm, start_y_mm;      // 动作开始时快照的平面位置
    float start_yaw_deg;               // 动作开始时快照的航向

    float relative_distance_mm;        // 相对距离 = current - start
    float relative_yaw_deg;            // 相对航向 = current_yaw - start_yaw
    float progress;                    // 当前进度
    float remaining;                   // 剩余量

    float profile_rate;                // 规划器当前目标速率
    float reference_progress;          // 规划器参考进度
    float last_progress;               // 上一周期进度（无进展检测用）

    uint8_t heading_hold_enabled;      // 航向保持使能
    uint8_t finish_hold_count;         // 完成保持计数
    uint32_t start_tick;               // 动作开始 tick
    uint32_t progress_tick;            // 上次有进展的 tick
    uint32_t timeout_ms;               // 总超时时间
} USER_MCM_Context_t;
```

## 3. 状态机

```text
IDLE ──[StartStraight/StartSpin]──> START ──[EnterRun]──> RUN
                                      ↑                     │
                                      │            ┌────────┘
                                      │            ▼
                                      │         DONE / FAULT
                                      │            │
                                      └──[Cancel]──┘
```

### 状态说明

| 状态 | 含义 |
|------|------|
| `IDLE` | 空闲，可接受新动作 |
| `START` | 动作已排入，下一周期进入 RUN |
| `RUN` | 正在执行：更新传感器→检查故障→更新规划器→输出电机命令→检查完成 |
| `DONE` | 动作正常完成 |
| `FAULT` | 动作异常终止（超时/无进展/传感器错误/取消） |

### 每 10ms 执行流程

```c
void USER_MCM_Task(void) {
    switch (mcm.state) {
    case IDLE/DONE/FAULT: return;
    case START: USER_MCM_EnterRun(); break;
    case RUN:
        USER_MCM_UpdateSensors();      // 快照 STATE_Estimate
        USER_MCM_CheckFaults();        // 超时/无进展检查
        USER_MCM_UpdateProfile();      // 梯形规划器
        USER_MCM_UpdateBodyCommand();  // 车体命令 → 左右轮目标
        USER_MCM_ApplyWheelSpeedCommand(); // → speed_pid.target
        USER_MCM_CheckFinish();        // 完成判据
        break;
    }
}
```

## 4. 直行动作细节

### 梯形速度规划

```text
加速段 (800mm/s²) → 匀速段 → 减速段 (1000mm/s²) → 停止
```

### 航向保持

```c
// 启动时快照 yaw，运行期间修正：
heading_error = MCM_AngleDifference(mcm.start_yaw_deg, est->yaw_deg);
yaw_correction = heading_error * MCM_STRAIGHT_HEADING_KP_MM_S_PER_DEG; // 2.0
// 左轮减速、右轮加速（或反之）以修正航向
```

### 距离反馈

- 优先使用路程计（`est->distance_mm`）
- 位置式 PID：`MCM_LINEAR_POSITION_KP = 2.0`

### 完成判定

1. 进度到达目标 ± 5mm（`MCM_LINEAR_DONE_DEADBAND_MM`）
2. 速度降到 30mm/s 以下（`MCM_LINEAR_STOP_SPEED_MM_S`）
3. 连续 5 个周期满足条件（`MCM_FINISH_HOLD_CYCLES`）

### 保护

- 总超时：`timeout_ms = distance/max_rate × 250% + 2000ms`
- 无进展超时：500ms 内进度变化 < 1mm（`MCM_NO_PROGRESS_TIMEOUT_MS`）

## 5. 旋转动作细节

### 梯形角速度规划

```text
加速段 (360°/s²) → 匀速段 → 减速段 (420°/s²) → 停止
```

### 角度反馈

- 优先使用 IMU（`relative_yaw_deg`）
- 角度 PID：`MCM_ROTATE_ANGLE_KP = 3.0`

### 完成判定

1. 角度差 < 1°（`MCM_ROTATE_DEADZONE_DEG`）
2. 角速度 < 5°/s（`MCM_ROTATE_STOP_GYRO_DEG_S`）

### 保护

- 总超时：`timeout_ms = angle/max_rate × 250% + 1500ms`

## 6. 公开 API

### 启动动作

```c
USER_MCM_Status_t USER_MCM_StartStraight(int32_t distance_mm, int32_t max_speed_mm_s);
USER_MCM_Status_t USER_MCM_StartSpin(int32_t relative_angle_deg, int32_t max_gyro_deg_s);
USER_MCM_Status_t USER_MCM_StartArc(radius_mm, arc_angle_deg, turn_dir, drive_dir, max_speed, max_gyro);
```

### 查询

```c
bool USER_MCM_IsIdle(void);                    // 是否空闲
bool USER_MCM_IsBusy(void);                    // 是否执行中
USER_MCM_Status_t USER_MCM_GetStatus(void);     // 状态码
USER_MCM_State_t USER_MCM_GetState(void);       // 状态机阶段
USER_MCM_ActionType_t USER_MCM_GetActionType(void); // 动作类型
```

### 控制

```c
void USER_MCM_Task(void);           // 每 10ms 周期更新
void USER_MCM_Cancel(void);         // 上层取消
void USER_MCM_FreezeAllMotions(void); // 紧急制动
```

### 单位换算

```c
int32_t USER_MCM_DistanceToEncoderCount(int32_t distance_mm);
int32_t USER_MCM_EncoderCountToDistance(int32_t encoder_count);
int32_t USER_MCM_SpeedToEncoderCount(int32_t speed_mm_s);
int32_t USER_MCM_EncoderCountToSpeed(int32_t encoder_count_per_period);
int32_t USER_MCM_DistanceToOdometerEncoderCount(int32_t distance_mm);
int32_t USER_MCM_OdometerEncoderCountToDistance(int32_t odometer_count);
int32_t USER_MCM_SpeedToOdometerEncoderCount(int32_t speed_mm_s);
int32_t USER_MCM_OdometerEncoderCountToSpeed(int32_t odometer_count_per_period);
```

## 7. 标定参数（`userapp_mcm.h`）

| 参数 | 值 | 说明 |
|------|------|------|
| `MCM_LINEAR_ACCEL_MM_S2` | 800 | 直线加速度 |
| `MCM_LINEAR_DECEL_MM_S2` | 1000 | 直线减速度 |
| `MCM_LINEAR_POSITION_KP` | 2.0 | 直线位置反馈增益 |
| `MCM_LINEAR_DONE_DEADBAND_MM` | 5.0 | 直线完成死区(mm) |
| `MCM_LINEAR_STOP_SPEED_MM_S` | 30.0 | 直线停机速度(mm/s) |
| `MCM_STRAIGHT_HEADING_KP_MM_S_PER_DEG` | 2.0 | 航向保持修正增益 |
| `MCM_ROTATE_ACCEL_DEG_S2` | 360 | 旋转角加速度 |
| `MCM_ROTATE_DECEL_DEG_S2` | 420 | 旋转角减速度 |
| `MCM_ROTATE_ANGLE_KP` | 3.0 | 旋转角度反馈增益 |
| `MCM_ROTATE_DEADZONE_DEG` | 1.0 | 旋转完成死区(°) |
| `MCM_ROTATE_STOP_GYRO_DEG_S` | 5.0 | 旋转停机角速度(°/s) |

## 8. 车辆模型参数（`userapp_vehicle_model.h`）

| 参数 | 值 | 说明 |
|------|------|------|
| `VEHICLE_WHEEL_DIAMETER_MM` | 65.0 | 驱动轮直径 |
| `VEHICLE_TRACK_WIDTH_MM` | 114.0 | 轮距 |
| `VEHICLE_GEAR_RATIO` | 0.0357 | 传动比 |
| `VEHICLE_ENCODER_RESOLUTION_PPR` | 500 | 编码器分辨率 |
| `ODOMETER_WHEEL_DIAMETER_MM` | 35.0 | 路程计轮直径 |
| `ODOMETER_ENCODER_RESOLUTION_PPR` | 4096 | 路程计分辨率 |
| `MCM_CONTROL_PERIOD_MS` | 10 | 控制周期 |
