# User_Algorithm — 算法库

纯算法模块，与硬件、传感器、执行器完全解耦，可跨项目复用。

## 文件列表

| 文件 | 功能 |
|------|------|
| `userlib_pid.c/h` | 通用 PID 控制器（位置式/增量式/高级/斜坡） |

## PID 控制器 (`userlib_pid.c`)

### 结构体

```c
typedef struct {
    float kp, ki, kd;              // PID 系数

    int32_t target, current;       // 目标值、当前反馈值
    int32_t error;                 // 当前误差
    int32_t integral;              // 积分项
    int32_t derivative;            // 微分项

    int32_t deadzone;              // 死区阈值
    bool vi_enable;                // 变积分使能

    int32_t max_integral;          // 积分上限
    int32_t min_integral;          // 积分下限
    int32_t integral_depart;       // 积分分离阈值

    int32_t last_error;            // 上一次误差
    int32_t last_last_error;       // 上上次误差（增量式PID用）
    int32_t last_output;           // 上一次输出

    int32_t output;                // PID 输出值
    int32_t max_output;            // 输出上限
    int32_t min_output;            // 输出下限

    int32_t delta_output;          // 增量输出
    int32_t max_delta_output;      // 增量输出上限
    int32_t min_delta_output;      // 增量输出下限

    int32_t ramp_target;           // 斜坡最终目标
    int32_t ramp_rate;             // 斜坡速率 (%)
    bool ramp_enable;              // 斜坡使能
} PID_Control_Struct_TypeDef;
```

### 提供的算法

| 函数 | 说明 |
|------|------|
| `USER_Positional_PID_Control()` | 标准位置式 PID：死区→积分分离→积分限幅→输出限幅→抗饱和 |
| `USER_Positional_PID_Control_Advanced()` | 高级位置式 PID：变积分 + 梯形积分 |
| `USER_Positional_PID_Control_Advanced_ExD()` | 带扩展微分项的位置式 PID |
| `USER_Incremental_PID_Control()` | 增量式 PID：输出增量 + 限幅 |
| `USER_PID_SetTargetWithRamp()` | 设置目标值，可选启用斜坡控制 |
| `USER_PID_ClearAndStop()` | 清零积分/历史/斜坡，输出归零 |

### 斜坡控制

- 设置 `ramp_target`（最终目标）和 `ramp_rate`（每次逼近百分比）
- `ramp_enable = true` 后，每次 `UpdateRampTarget()` 将 `target` 逐步靠近 `ramp_target`
- 差值 ≤1 时自动锁定目标并关闭斜坡

### 使用示例

```c
// 初始化
speed_pid[0].kp = 1.5f;  speed_pid[0].ki = 0.1f;  speed_pid[0].kd = 0.05f;
speed_pid[0].max_output = 1000;  speed_pid[0].min_output = -1000;
speed_pid[0].deadzone = 5;
speed_pid[0].integral_depart = 200;
speed_pid[0].max_integral = 500;  speed_pid[0].min_integral = -500;

// 直接设目标
speed_pid[0].target = 300;
USER_Positional_PID_Control(&speed_pid[0]);

// 斜坡设目标（每次逼近剩余差值的 10%）
USER_PID_SetTargetWithRamp(&speed_pid[0], 500, 10);
```

## 设计原则

- 不依赖 `encoder_data`、`imu_data`、`adc_data` 等全局传感器变量
- 不依赖电机 ID、电机模式
- 不依赖 SysTick、OLED、Modbus、LiDAR
- 不假设物理单位（单位由调用方在上层适配）
- 不在本层做动态内存分配

## 后续扩展规划

根据 `docs/algorithm_filter_design_plan_zh.md`，本层后续可加入：
- `userlib_complementary_filter.c/h` — 1D/2D 互补滤波器
- `userlib_kalman.c/h` — 线性 Kalman / EKF 滤波器
- `userlib_math.c/h` — 矩阵运算、向量运算
- `userlib_ramp.c/h` — 通用斜坡/梯形规划器
