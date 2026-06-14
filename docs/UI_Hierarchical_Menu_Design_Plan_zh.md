# UI 分级菜单实现说明

> 状态：已实现，当前代码位于 `User_UI/`。
> 最后更新：2026-06-14。

## 1. 当前结构

UI 已采用“一级分类菜单 + 二级页面列表 + 页面注册表”的结构：

```text
MAIN MENU
  -> SUB MENU
     -> PAGE VIEW
     -> PLACEHOLDER
```

实现目标：

- 上电进入 `MAIN MENU`。
- 页面通过 `user_ui_pages.c` 注册。
- 菜单通过 `user_ui_menu.c` 的只读表归类。
- Auto Pilot 和 Setting 的未实现项使用统一 SOON/PLACEHOLDER 机制。
- PREV/NEXT 只在当前分类内切换已实现页面。

## 2. 一级分类

| 顺序 | 一级分类 | 用途 |
|------|----------|------|
| 1 | Run Route | 执行预设固定路线 |
| 2 | Auto Pilot | 自动驾驶算法预留 |
| 3 | Sensors | 传感器观察、校准和诊断 |
| 4 | Service | 工程维护、手动控制、硬件测试和系统诊断 |
| 5 | Setting | PID、运动参数和系统参数预留 |

## 3. 二级页面

| 一级分类 | 二级项目 | 页面 |
|----------|----------|------|
| Run Route | Fixed Route | `PAGE_TEMPLATE` |
| Auto Pilot | Photo Pilot | PLACEHOLDER |
| Auto Pilot | Vision Pilot | PLACEHOLDER |
| Auto Pilot | Fusion Pilot | PLACEHOLDER |
| Sensors | Encoder Data | `PAGE_ENCODER` |
| Sensors | Photo Sensors | `PAGE_PHOTOELECTRIC` |
| Sensors | IMU Data | `PAGE_GYROSCOPE` |
| Sensors | IMU Sum Data | `PAGE_IMU_SUM` |
| Sensors | Smart Camera | `PAGE_CAMERA` |
| Sensors | ADC Data | `PAGE_ADC` |
| Sensors | LiDAR Sensors | `PAGE_LIDAR` |
| Service | Hardware Test | `PAGE_UNITTEST` |
| Service | Motor PID Mon | `PAGE_MOTOR` |
| Service | Servo Manual | `PAGE_SERVO` |
| Service | Thread Stats | `PAGE_THREADS` |
| Service | SysInfo | `PAGE_SYSINFO` |
| Setting | Motor PID / Motion Params / Servo Params / System | PLACEHOLDER |

## 4. 关键文件

| 文件 | 职责 |
|------|------|
| `user_ui_core.c` | UI 任务、输入消费、视图切换、动态刷新 |
| `user_ui_core_state.c` | 当前视图、当前页面、一级/二级光标 |
| `user_ui_menu.c/h` | 菜单数据表和菜单绘制 |
| `user_ui_pages.c` | 13 个页面描述符注册表 |
| `user_ui_dispatch.c` | 页面生命周期和回调分发 |
| `user_ui_input.c` | BoardIO 按键事件到 UI 事件的转换 |
| `user_ui_page_*.c` | 页面显示和业务逻辑 |

## 5. 按键语义

| 位置 | 按键 | 行为 |
|------|------|------|
| MAIN MENU | UP/DOWN 或 PREV/NEXT | 移动一级分类 |
| MAIN MENU | ENTER | 打开二级列表 |
| SUB MENU | UP/DOWN 或 PREV/NEXT | 移动二级项目 |
| SUB MENU | ENTER | 打开页面或占位提示 |
| SUB MENU | ESC | 返回 MAIN MENU |
| PAGE VIEW | PREV/NEXT | 当前分类内切换页面 |
| PAGE VIEW | ESC | 返回所属二级列表 |
| PLACEHOLDER | ESC | 返回所属二级列表 |

页面内方向键和 ENTER 由当前页面回调处理。

## 6. 特殊页面策略

### Fixed Route

```text
IDLE
  -> ENTER 长按 2000ms
  -> WAIT_RELEASE
  -> 倒计时 1000ms
  -> USER_Race_RequestStart()
```

充电、等待释放和倒计时阶段，`ESC` 取消流程；页面退出时 `on_exit` 也会取消未完成流程。

### Servo Manual

- 默认进入 MON 监视模式，不立即接管舵机。
- ENTER 进入/退出 MAN 手动模式。
- MAN 模式下 UP/DOWN 选择舵机，LEFT/RIGHT 以 1 度步进调整角度。
- 退出页面时通过 `on_exit` 释放 UI 控制权。

### SysInfo

Service 分类新增 `SysInfo` 页面，显示 CPU、LOAD、RAM、FLASH 和固件版本。CPU 利用率来自 `USER_OS_GetCpuLoadPermille()`，统计口径为非 WFI 睡眠时间占比。

## 7. 新增页面流程

1. 新建 `user_ui_page_xxx.c`。
2. 在 `user_ui_internal.h` 声明页面函数，必要时增加 `DisplayPage_t` 枚举。
3. 在 `user_ui_pages.c` 增加页面描述符。
4. 在 `user_ui_menu.c` 增加菜单项。
5. 在 `basic.ewp` 加入新源文件。
6. IAR Debug 全量构建验证。
