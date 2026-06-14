# User_Application — 应用层

应用层包含车辆状态估测、非阻塞运动控制、表格化路线调度和车辆模型常量。

## 文件列表

| 文件 | 功能 |
|------|------|
| `user_ui.h` | UI 模块统一入口（重定向到 `User_UI/user_ui_public.h`） |
| `userapp_vehicle_model.h` | 车辆几何参数与编码器标定常量（纯头文件） |
| `userapp_state_estimator.c/h` | **统一车辆状态估测** — 唯一直接读取编码器/IMU 的模块 |
| `userapp_mcm.c/h` | **非阻塞运动控制管理器** (Motion Control Manager) |
| `userapp_race_table.c/h` | 表格化路线调度器（动作表解释器） |
| `userapp_race.c/h` | 比赛路线启动请求与动作表推进 |

## 车辆模型 (`userapp_vehicle_model.h`)

纯头文件，集中管理所有物理标定常量：

| 参数 | 符号 | 值 |
|------|------|------|
| 驱动轮直径 | `VEHICLE_WHEEL_DIAMETER_MM` | 65.0 mm |
| 驱动轮周长 | `VEHICLE_WHEEL_CIRCUMFERENCE_MM` | π×65 ≈ 204.2 mm |
| 轮距 | `VEHICLE_TRACK_WIDTH_MM` | 114.0 mm |
| 传动比 | `VEHICLE_GEAR_RATIO` | 0.0357 |
| 编码器分辨率 | `VEHICLE_ENCODER_RESOLUTION_PPR` | 500 pulse/rev |
| 路程计轮直径 | `ODOMETER_WHEEL_DIAMETER_MM` | 35.0 mm |
| 路程计分辨率 | `ODOMETER_ENCODER_RESOLUTION_PPR` | 4096 pulse/rev |
| 控制周期 | `MCM_CONTROL_PERIOD_MS` | 10 ms |

自动推导的换算系数：
- `VEHICLE_ENCODER_PULSE_PER_MM` — 驱动轮 1mm 对应编码器脉冲数
- `ODOMETER_ENCODER_PULSE_PER_MM` — 路程计 1mm 对应脉冲数
- `ODOMETER_MM_PER_PULSE` — 路程计 1 脉冲对应 mm 数

## 状态估测器 (`userapp_state_estimator.c`)

### 职责

应用层中**唯一**直接读取 `encoder_data[]` 和 `imu_data` 的模块。
运动控制代码只能消费本模块发布的 `USER_STATE_Estimate_t`。

### 估测方法

通过 `user_config.h` 中的 `USER_STATE_DEFAULT_ESTIMATOR` 宏选择：

| 宏值 | 方法 | 状态 |
|------|------|------|
| `USER_STATE_ESTIMATOR_RAW` | 基础直通估测 | ✅ 当前默认可用 |
| `USER_STATE_ESTIMATOR_WEIGHTED` | 加权融合 | 🔄 预留入口（回落 RAW） |
| `USER_STATE_ESTIMATOR_COMPLEMENTARY` | 互补滤波 | 🔄 预留入口（回落 RAW） |
| `USER_STATE_ESTIMATOR_KALMAN` | Kalman 滤波 | 🔄 预留入口（回落 RAW） |
| `USER_STATE_ESTIMATOR_EKF` | 扩展 Kalman | 🔄 预留入口（回落 RAW） |

### 直通估测器策略

1. **距离/速度**：优先路程计 → 不可用时回落到驱动轮平均值
2. **航向/角速度**：优先 IMU → 不可用时回落到左右轮差速积分
3. **位置更新**：使用里程计增量 + 航向角做航位推算
4. **有效性标记**：`distance_valid` / `yaw_valid` / `velocity_valid` / `degraded`
5. **诊断输出**：打滑估计 `slip_error_mm`、航向差异 `yaw_disagree_deg`

### 输出结构

```c
typedef struct {
    float distance_mm;           // 主距离估计
    float x_mm, y_mm;            // 平面航位推算位置
    float yaw_deg;               // 主航向角估计
    float v_mm_s;                // 线速度
    float omega_deg_s;           // 角速度
    float gyro_bias_deg_s;       // 陀螺仪偏置（预留）

    // 诊断字段
    float odom_distance_mm, wheel_distance_mm;
    float left_wheel_speed_mm_s, right_wheel_speed_mm_s;
    float wheel_yaw_deg, imu_yaw_deg, imu_gyro_yaw_deg;
    float slip_error_mm, yaw_disagree_deg;

    uint8_t distance_valid, yaw_valid, velocity_valid;
    uint8_t degraded, quality;
    USER_STATE_Source_t source;
} USER_STATE_Estimate_t;
```

## MCM 运动控制 (`userapp_mcm.c`)

### 设计原则

- **只消费** `USER_STATE_Estimate_t`，不直接读传感器
- 非阻塞状态机，每 10ms 周期执行
- 拥有电机命令写入权（速度 PID 目标 + 电机模式）

### 状态机

```
IDLE → START → RUN → DONE / FAULT
```

### 支持的动作

| API | 动作 | 状态 |
|-----|------|------|
| `USER_MCM_StartStraight(distance_mm, max_speed_mm_s)` | 非阻塞直行/后退 | ✅ 已实现 |
| `USER_MCM_StartSpin(angle_deg, max_gyro_deg_s)` | 非阻塞原地旋转 | ✅ 已实现 |
| `USER_MCM_StartArc(...)` | 非阻塞圆弧运动 | 🔄 预留接口 |

### 直行动作细节

- **梯形速度规划**：加速段 (`MCM_LINEAR_ACCEL_MM_S2=800`) → 匀速段 → 减速段 (`MCM_LINEAR_DECEL_MM_S2=1000`)
- **航向保持**：`MCM_STRAIGHT_HEADING_KP_MM_S_PER_DEG=2.0`，修正左右轮差速以保持航向
- **距离反绩**：优先使用路程计（`relative_distance_mm`），位置式位置 PID (`MCM_LINEAR_POSITION_KP=2.0`)
- **完成判据**：进度到达目标 ± `MCM_LINEAR_DONE_DEADBAND_MM(5mm)` + 速度降到 `MCM_LINEAR_STOP_SPEED_MM_S(30mm/s)` 以下
- **超时保护**：`timeout_ms = distance/max_rate * 250% + 2000ms`
- **无进展保护**：500ms 内进度变化 < 1mm 则报 `NO_PROGRESS`

### 旋转动作细节

- **梯形角速度规划**：加速段 (`MCM_ROTATE_ACCEL_DEG_S2=360`) → 匀速段 → 减速段 (`MCM_ROTATE_DECEL_DEG_S2=420`)
- **角度反绩**：优先 IMU yaw（`relative_yaw_deg`），角度 PID (`MCM_ROTATE_ANGLE_KP=3.0`)
- **完成判据**：角度差 < `MCM_ROTATE_DEADZONE_DEG(1°)` + 角速度 < `MCM_ROTATE_STOP_GYRO_DEG_S(5°/s)`

### 公开 API

| 函数 | 说明 |
|------|------|
| `USER_MCM_IsIdle()` / `USER_MCM_IsBusy()` | 空闲/忙碌查询 |
| `USER_MCM_GetStatus()` / `USER_MCM_GetState()` | 状态码/状态机阶段查询 |
| `USER_MCM_GetActionType()` | 当前动作类型查询 |
| `USER_MCM_Task()` | 每 10ms 状态机更新（由调度器调用） |
| `USER_MCM_Cancel()` | 上层取消当前动作 |
| `USER_MCM_FreezeAllMotions()` | 紧急制动（左右轮立即制动） |
| `USER_MCM_DistanceToEncoderCount()` 等 | 编码器/物理量双向换算 |

## Race 路线调度

### 两层架构

```
userapp_race.c      → 路线启动管理、周期推进
userapp_race_table.c → 动作表解释器（逐行启动 MCM → 等待完成 → 跳转）
```

### 动作表结构

```c
typedef struct {
    USER_Race_ActionType_t action_type;  // MOVE_DISTANCE / ROTATE_ANGLE / WAIT_MS / STOP / END
    float param1;                        // 距离(mm) / 角度(deg) / 等待时间(ms)
    float param2;                        // 最大速度(mm/s) / 最大角速度(deg/s)
    uint32_t timeout_ms;                 // 表层超时（0=不启用）
    int16_t next_index;                  // 下个动作行索引（-1=结束）
    uint16_t flags;                      // 预留
} USER_Race_Action_t;
```

### 当前模板路线

```c
{MOVE_DISTANCE, 500mm,  300mm/s, 3000ms},  // 直行 50cm
{ROTATE_ANGLE,  180°,    90°/s, 2500ms},   // 180° 掉头
{MOVE_DISTANCE, 500mm,  300mm/s, 3000ms},  // 返回 50cm
{ROTATE_ANGLE,  180°,    90°/s, 2500ms},   // 180° 回转
{END,           0,       0,      0},        // 结束
```

### 路线启动流程

1. UI 路线页面（PAGE_TEMPLATE）ENTER 长按
2. 充电进度条 2s
3. 倒计时 1s
4. `USER_Race_RequestStart(RACE_ROUTE_TEMPLATE)`
5. `USER_Race_Task()` 启动 `USER_Race_TableExecutor_Start()`
6. 调度器每 10ms 调用 `USER_Race_TableExecutor_Update()` 推进

## 控制链路总览

```
┌─────────────────┐
│ 传感器原始数据     │  encoder_data[] / imu_data / adc_data[]
│   (globals.c)    │
└────────┬────────┘
         ▼
┌─────────────────┐
│ State Estimator │  每 10ms: 采集→换算→发布 USER_STATE_Estimate_t
│ (唯一读传感器)    │  包含: 距离/mm, 速度/mm/s, 偏航角/deg, 角速度/deg/s
└────────┬────────┘
         ▼
┌─────────────────┐
│      MCM        │  每 10ms: 消费 Estimate → 梯形规划 → 航向修正
│ (非阻塞状态机)    │  → 速度 PID 目标 → USER_Motor_SetSpeed()
└────────┬────────┘
         ▼
┌─────────────────┐
│    Race Table   │  每 10ms: 读动作表 → MCM_Start*() → 等待完成
│   (路线调度)     │  → 跳转下一行
└─────────────────┘
```
