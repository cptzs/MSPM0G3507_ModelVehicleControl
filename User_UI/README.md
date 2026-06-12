# User_UI — 注册表驱动的 OLED 多页面框架

## 架构概览

```
USER_UI_Task() [core.c, 每 5ms 由调度器调用]
  ├─ 路线启动交互（充电→倒计时→启动）[route_context.c]
  ├─ PREV/NEXT 页面导航事件 [core_state.c]
  ├─ 按键分发到当前页 on_key() [input.c → dispatch.c → page_*.c]
  ├─ 静态内容脏刷新 [dispatch.c + core_state.c]
  └─ 动态内容逐行/逐项轮询刷新 [dispatch.c → page_*.c]
```

## 文件说明

| 文件 | 功能 |
|------|------|
| `user_ui_public.h` | **对外唯一公开接口** — 仅声明 `USER_UI_Task()` |
| `user_ui_internal.h` | UI 内部共享：页面枚举（12页）、描述符结构、页面函数声明、驱动头引用 |
| `user_ui_core.c` | 主循环：注册表驱动页面导航、按键分发、路线交互优先级 |
| `user_ui_core_state.c` | 核心状态：当前页 + 静态脏标志维护 |
| `user_ui_dispatch.c` | 通用分发：查表绘制静态/动态、分发按键、获取相邻页 |
| `user_ui_input.c` | 输入适配：将底层 `USER_LBB_ButtonEvent_t` 转为 `USER_UI_KeyEvent_t` |
| `user_ui_pages.c` | **页面注册表** — 编译期静态数组，12 页描述符 |
| `user_ui_route_context.c` | 路线启动交互状态机（充电 2s → 倒计时 2s → 启动） |
| `user_ui_page_motor.c` | 电机页面：左右轮 PID 7 行数据逐行刷新 |
| `user_ui_page_sensors.c` | 传感器页面：编码器(3路) / 光电(8路) / ADC / LiDAR(4路) / IMU |
| `user_ui_page_imu_sum.c` | IMU 积分汇总页面：sum_gyroz 等累计量 |
| `user_ui_page_camera.c` | 智能相机页面（预留） |
| `user_ui_page_actuator.c` | 执行器页面：舵机控制 |
| `user_ui_page_debug.c` | 线程调试页面：调度器各任务耗时统计 |
| `user_ui_page_route.c` | 路线页面：赛道图形 + 动作预览 + ENTER 长按启动 |
| `user_ui_page_unittest.c` | 单元测试页面：12 项硬件测试（电机/IMU/蜂鸣器/LED等） |
| `user_ui_unittest_actions.c/h` | UnitTest 硬件动作封装层 |

## 核心设计模式

### 1. 页面描述符注册表

```c
typedef struct {
    DisplayPage_t page;                    // 页面枚举值
    const char *title;                     // 标题（固定 13 字符宽）
    USER_UI_PageDrawFunc_t show_static;    // 静态绘制（页面切换时调用一次）
    USER_UI_PageDrawFunc_t show_dynamic;   // 动态刷新（每 5ms 周期调用）
    USER_UI_PageKeyFunc_t on_key;          // 按键回调（NULL=无交互）
} USER_UI_PageDef_t;
```

注册表定义在 `user_ui_pages.c` 中，编译期常量，决定 PREV/NEXT 导航顺序。

### 2. 静态/动态分离

- **静态内容**：标题行 `[XX] Title` + 固定标签文字。仅在页面切换时绘制一次。
- **动态内容**：传感器数值等变化数据，每周期刷新。

由 `core_state.c` 的 `ui_static_dirty` 标志控制。

### 3. 事件消费模型

底层 LBB 驱动提供"计数型"按钮事件（SHORT/LONG/LONG_REPEAT），
`input.c` 的 `USER_UI_ConsumeButtonEvent()` **读取并清除**事件计数器，
防止同一事件被多次处理。

### 4. 逐行/逐项轮询刷新

每个页面的动态绘制函数每周期只刷新 1 行/1 项数据，降低 OLED SPI 瞬时负载。

### 5. 路线交互优先级

`USER_UI_Route_IsBusy()` 为真时（充电/倒计时阶段），主循环**阻止页面导航**，
确保比赛启动过程不被 PREV/NEXT 中断。

## 页面列表

| # | 枚举 | 标题 | 文件 | 按键交互 |
|---|------|------|------|---------|
| 1 | `PAGE_MOTOR` | Motor Control | `page_motor.c` | 无（纯显示 7 行 PID 数据） |
| 2 | `PAGE_ENCODER` | Encoder Data | `page_sensors.c` | UP/DN 切换编码器, ENTER 清零 |
| 3 | `PAGE_PHOTOELECTRIC` | Photo Sensors | `page_sensors.c` | 全按键支持（UP/DN/LEFT/RIGHT/ENTER/ESC） |
| 4 | `PAGE_ADC` | ADC Data | `page_sensors.c` | 无 |
| 5 | `PAGE_LIDAR` | LiDAR Sensors | `page_sensors.c` | UP/DN 选择传感器 |
| 6 | `PAGE_GYROSCOPE` | IMU Data | `page_sensors.c` | ENTER/ESC 校准 |
| 7 | `PAGE_IMU_SUM` | IMU Sum Data | `page_imu_sum.c` | ENTER 清零积分量 |
| 8 | `PAGE_CAMERA` | Smart Camera | `page_camera.c` | 无（预留） |
| 9 | `PAGE_SERVO` | Servo Control | `page_actuator.c` | 无 |
| 10 | `PAGE_DEBUG` | Threads | `page_debug.c` | UP/DN 滚动查看任务 |
| 11 | `PAGE_TEMPLATE` | Template Path | `page_route.c` | ENTER 长按启动, ESC 取消 |
| 12 | `PAGE_UNITTEST` | UnitTest | `page_unittest.c` | UP/DN 选测试项, ENTER 执行 |

## 按键事件类型

| 枚举值 | 触发条件 |
|--------|---------|
| `USER_UI_KEY_EVENT_NONE` | 无事件 |
| `USER_UI_KEY_EVENT_SHORT` | 短按释放 |
| `USER_UI_KEY_EVENT_LONG` | 长按（单次触发） |
| `USER_UI_KEY_EVENT_LONG_REPEAT` | 长按（连续触发） |

物理按钮：UP / DOWN / LEFT / RIGHT / ENTER / ESC / PREV / NEXT。
其中 PREV/NEXT 专门用于翻页导航，其余 6 个分发给当前页面。

## 路线启动交互

```
IDLE → (ENTER 按下) → CHARGING（2s 充电进度条）
  → (2s 满) → COUNTDOWN（2s 倒计时）
  → 调用 USER_Race_RequestStart() → 回到 IDLE

任意时刻松开 ENTER 或按 ESC → 取消，回到 IDLE
```

## UnitTest 测试项

| # | 名称 | 动作 |
|---|------|------|
| 0 | MTR FWD | 电机前进 |
| 1 | MTR REV | 电机后退 |
| 2 | MTR SPD0 | 电机停转 |
| 3 | MTR STOP | 电机制动 |
| 4 | MTR COAST | 电机滑行 |
| 5 | MTR BRAKE | 电机刹车 |
| 6 | IMU HELLO | IMU 通信测试 |
| 7 | USB HELLO | USB/Modbus 通信测试 |
| 8 | CAM HELLO | 相机通信测试（N/A 预留） |
| 9 | CAN FRAME | CAN 帧发送测试 |
| 10 | BUZZ 1MS | 蜂鸣器测试 |
| 11 | LED 300MS | LED 闪烁测试 |

## 添加新页面

1. 创建 `user_ui_page_xxx.c`，实现 `ShowXxxStatic()`, `ShowXxxDynamic()`, `XxxOnKey()`
2. 在 `user_ui_internal.h` 的枚举和函数声明区添加对应内容
3. 在 `user_ui_pages.c` 的 `ui_page_table[]` 中追加条目
4. 在 `basic.ewp` 的 `User_UI` 分组中加入 `.c` 文件
