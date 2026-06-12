# UI 子模块拆分说明

本目录用于承载 `User_Application` 下的本地人机交互子系统。当前拆分采用渐进式策略：先建立公开接口和内部接口边界，再逐页迁移 legacy `user_ui.c` 中的页面实现。

## 当前阶段

- `user_ui_public.h`
  - UI 对外公开接口。
  - 外部模块只应依赖 `USER_UI_Task()`。

- `user_ui_internal.h`
  - UI 内部接口。
  - 暂时保存页面枚举、legacy 页面函数声明和旧页面实现依赖。
  - 后续每迁移一个页面，就应把对应依赖下沉到该页面自己的 `.c` 文件中。

- `../user_ui.h`
  - 兼容入口。
  - 旧代码仍可 `#include "user_ui.h"`。
  - 新代码只需要调用 UI 周期任务时，应优先包含 `UI/user_ui_public.h`。

## 后续目标结构

建议后续逐步拆成以下文件：

```text
User_Application/UI/
├─ user_ui_public.h
├─ user_ui_internal.h
├─ user_ui_core.c
├─ user_ui_pages.c
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
