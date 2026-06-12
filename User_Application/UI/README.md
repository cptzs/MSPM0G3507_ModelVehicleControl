# UI 子模块拆分说明

本目录用于承载 `User_Application` 下的本地人机交互子系统。当前拆分采用渐进式策略：先建立公开接口和内部接口边界，再逐页迁移 legacy `user_ui.c` 中的页面实现。

## 当前阶段

- `user_ui_public.h`
  - UI 对外公开接口。
  - 外部模块只应依赖 `USER_UI_Task()`。

- `user_ui_internal.h`
  - UI 内部接口。
  - 暂时保存页面枚举、legacy 页面函数声明和旧页面实现依赖。
  - 已新增页面描述符 `USER_UI_PageDef_t`、页面表访问接口、注册表分发 helper、输入事件适配、页面状态 helper、路线启动 context 接口和新 Motor/Actuator/Debug/Route/UnitTest 页面接口。
  - 后续每迁移一个页面，就应把对应依赖下沉到该页面自己的 `.c` 文件中。

- `user_ui_pages.c`
  - 新 UI 页面注册表。
  - 当前注册了 legacy 页面、新 Motor 页面、新 Actuator 页面、新 Debug 页面、新 Route 页面和 `PAGE_UNITTEST`。
  - `PAGE_MOTOR` 已切到 `user_ui_page_motor.c` 的新实现。
  - `PAGE_SERVO` 已切到 `user_ui_page_actuator.c` 的新实现。
  - `PAGE_DEBUG` 已切到 `user_ui_page_debug.c` 的新实现。
  - `PAGE_TEMPLATE` 已切到 `user_ui_page_route.c` 的新实现；其他 legacy 页面仍临时调用 legacy 页面绘制函数。

- `user_ui_dispatch.c`
  - 页面注册表通用分发层。
  - 提供按页面号绘制静态内容、绘制动态内容、分发页面按键、获取上一页/下一页等 helper。
  - 后续 legacy `user_ui.c` 中的页面标题数组、两个页面分发 `switch`、`PREV/NEXT` 边界判断应优先替换为这里的 helper。

- `user_ui_input.c`
  - UI 输入适配层。
  - 将 `userlib_lbb` 的计数型按钮事件转换为 UI 内部的 `USER_UI_KeyEvent_t`。
  - `user_ui_core.c` 通过该文件统一消费按钮事件，再分发给当前页面。

- `user_ui_core_state.c`
  - UI 核心状态 helper。
  - 维护当前页面和静态内容脏标志，提供页面切换、静态重绘和动态刷新 helper。

- `user_ui_core.c`
  - 新的注册表驱动 UI 周期任务。
  - 当前文件已经完成。
  - 工程切换时，应让本文件成为唯一提供 `USER_UI_Task()` 的编译单元。
  - 可使用仓库根目录下的 `tools/migrate_iar_ui_core.py` 自动修改 `basic.ewp`。

- `user_ui_legacy_pages.c`
  - 过渡期 legacy 页面包装文件。
  - 通过包含 `../user_ui.c` 复用旧页面绘制函数，同时把旧 `USER_UI_Task()` 重命名为 `USER_UI_LegacyTask()`，避免和新 `user_ui_core.c` 的 `USER_UI_Task()` 重复定义。
  - 工程切换后，应使用本文件替代直接编译 `../user_ui.c`。
  - 后续当 sensors 等页面逐步迁移到独立文件后，本包装文件可以删除。

- `user_ui_route_context.c`
  - 路线启动交互 context。
  - 承接 `Template Path` 页的长按蓄力、稳定倒计时、待启动路线和倒计时结束启动路线逻辑。

- `user_ui_page_motor.c`
  - 新 Motor Control 页面实现。
  - 已从 legacy `user_ui.c` 抽出电机模式、目标速度、实际速度、误差、积分、微分和 PWM 显示。
  - 页面注册表中的 `PAGE_MOTOR` 已切到该文件的新函数。

- `user_ui_page_actuator.c`
  - 新 Actuator 页面实现。
  - 当前先迁移 `Servo Control` 页面，负责显示两路舵机角度、PWM 宽度和误差。
  - 页面注册表中的 `PAGE_SERVO` 已切到该文件的新函数。

- `user_ui_page_debug.c`
  - 新 Threads/Debug 页面实现。
  - 负责显示协作式调度器任务最近/最大运行耗时。
  - 当前仍复用 `USER_OS_GetTaskCount()` / `USER_OS_GetTaskStats()` 的显示窗口语义；当任务数量超过 OLED 可见行数时，OS stats 层负责根据 UP/DOWN 事件做窗口滚动。

- `user_ui_page_route.c`
  - 新 Route 页面实现。
  - 负责 Template Path 的路线图、进度条、动作摘要、倒计时显示和 ESC 取消。
  - 显示逻辑读取 `user_ui_route_context.c`，不再依赖 legacy `user_ui.c` 的文件内 `static` 状态。

- `user_ui_page_unittest.c`
  - UnitTest 页面的 UI 骨架。
  - 当前只实现列表、上下选择、ENTER 更新状态，不直接执行电机/CAN/串口等硬件动作。
  - 后续硬件动作建议通过单独的测试动作层接入，避免 UI 页面直接堆积危险操作细节。

- `../user_ui.h`
  - 兼容入口。
  - 旧代码仍可 `#include "user_ui.h"`。
  - 新代码只需要调用 UI 周期任务时，应优先包含 `UI/user_ui_public.h`。

## 工程切换 helper

仓库已新增 `tools/migrate_iar_ui_core.py`，用于把 `basic.ewp` 从旧 UI 入口切换到新 UI core。建议在本地仓库根目录执行：

```bash
python tools/migrate_iar_ui_core.py
```

该脚本会执行三类确定性修改：

1. 在 IAR C include path 中加入 `$PROJ_DIR$\User_Application\UI`。
2. 将 `User_Application\user_ui.c` 编译项替换为 `User_Application\UI\user_ui_legacy_pages.c`。
3. 新增 `User_Application_UI` 分组，加入 `user_ui_core.c`、`user_ui_core_state.c`、`user_ui_dispatch.c`、`user_ui_input.c`、`user_ui_pages.c`、`user_ui_route_context.c` 以及当前已稳定的新页面文件，包括 `user_ui_page_motor.c`。

脚本保持 `Minimal_Bringup` 配置排除应用层 UI 文件，和现有工程配置语义一致。执行后建议用 IAR 打开 `basic.eww`，确认 Debug 配置下只存在一个 `USER_UI_Task()`。

## 当前迁移边界

新 `user_ui_core.c`、`user_ui_page_motor.c`、`user_ui_page_actuator.c`、`user_ui_page_debug.c`、`user_ui_page_route.c`、`user_ui_pages.c` 等文件已经准备好。旧页面绘制函数仍通过 `user_ui_legacy_pages.c` 过渡复用。

工程切换时的目标状态是：

1. 不再直接编译 `User_Application/user_ui.c`。
2. 编译 `User_Application/UI/user_ui_core.c`，由它提供唯一 `USER_UI_Task()`。
3. 编译 `User_Application/UI/user_ui_legacy_pages.c`，由它复用尚未迁移的 legacy 页面绘制函数。
4. 编译当前所有已稳定的 `User_Application/UI/*.c` 新模块。

这样新 core 提供唯一 `USER_UI_Task()`，legacy wrapper 提供尚未迁移的旧页面绘制函数。

## 后续目标结构

建议后续逐步拆成以下文件：

```text
User_Application/UI/
├─ user_ui_public.h
├─ user_ui_internal.h
├─ user_ui_core.c
├─ user_ui_core_state.c
├─ user_ui_input.c
├─ user_ui_pages.c
├─ user_ui_dispatch.c
├─ user_ui_route_context.c
├─ user_ui_legacy_pages.c        # 过渡期文件，最终删除
├─ user_ui_page_motor.c
├─ user_ui_page_sensors.c
├─ user_ui_page_actuator.c
├─ user_ui_page_debug.c
├─ user_ui_page_route.c
└─ user_ui_page_unittest.c
```

## 迁移原则

1. 每次只迁移一组页面，保证每一步都可编译。
2. 外部模块不要依赖页面枚举和页面绘制函数。
3. 页面内按键不要直接抢读所有按键，后续应由 `user_ui_core.c` 统一消费并分发。
4. UnitTest 页面应作为独立页面新增，不再复用 `PAGE_CAMERA`。
5. 修改 IAR 工程文件前，先确保新增 `.c` 文件职责稳定。
6. legacy `user_ui.c` 中的页面标题数组和两个页面分发 `switch` 后续应替换为 `user_ui_dispatch.c` helper。
7. legacy `user_ui.c` 后续应降级为 legacy 页面绘制文件，不再保留 UI 主循环入口。
