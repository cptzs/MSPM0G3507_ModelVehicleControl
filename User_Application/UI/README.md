# UI 子模块拆分说明

本目录用于承载 `User_Application` 下的本地人机交互子系统。当前拆分采用渐进式策略：先建立公开接口和内部接口边界，再逐页迁移 legacy `user_ui.c` 中的页面实现。

## 当前阶段

- `user_ui_public.h`
  - UI 对外公开接口。
  - 外部模块只应依赖 `USER_UI_Task()`。

- `user_ui_internal.h`
  - UI 内部接口。
  - 暂时保存页面枚举、legacy 页面函数声明和旧页面实现依赖。
  - 已新增页面描述符 `USER_UI_PageDef_t`、页面表访问接口和注册表分发 helper。
  - 后续每迁移一个页面，就应把对应依赖下沉到该页面自己的 `.c` 文件中。

- `user_ui_pages.c`
  - 新 UI 页面注册表。
  - 当前注册了前 11 个 legacy 页面，并注册 `PAGE_UNITTEST`。
  - 该文件暂未接入 legacy `user_ui.c` 的页面切换逻辑，避免一次性重写主 UI 文件。

- `user_ui_dispatch.c`
  - 页面注册表通用分发层。
  - 提供按页面号绘制静态内容、绘制动态内容、分发页面按键、获取上一页/下一页等 helper。
  - 后续 legacy `user_ui.c` 中的页面标题数组、两个页面分发 `switch`、`PREV/NEXT` 边界判断应优先替换为这里的 helper。

- `user_ui_page_unittest.c`
  - UnitTest 页面的 UI 骨架。
  - 当前只实现列表、上下选择、ENTER 更新状态，不直接执行电机/CAN/串口等硬件动作。
  - 后续硬件动作建议通过单独的测试动作层接入，避免 UI 页面直接堆积危险操作细节。

- `../user_ui.h`
  - 兼容入口。
  - 旧代码仍可 `#include "user_ui.h"`。
  - 新代码只需要调用 UI 周期任务时，应优先包含 `UI/user_ui_public.h`。

## 当前迁移边界

`Template Path` 页的长按蓄力、稳定倒计时、蜂鸣提示状态仍保存在 legacy `user_ui.c` 的文件内 `static` 变量中，`USER_UI_ShowTemplateDynamic()` 也直接读取这些变量。因此在迁移 `USER_UI_Task()` 之前，必须先把路线启动交互状态抽成独立 context 或迁移到 `user_ui_page_route.c`，否则直接新增独立 `user_ui_core.c` 会破坏模板路线启动逻辑。

## 后续目标结构

建议后续逐步拆成以下文件：

```text
User_Application/UI/
├─ user_ui_public.h
├─ user_ui_internal.h
├─ user_ui_core.c
├─ user_ui_pages.c
├─ user_ui_dispatch.c
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
