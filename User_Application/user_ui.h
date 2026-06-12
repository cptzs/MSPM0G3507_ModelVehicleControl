#ifndef USER_UI_H
#define USER_UI_H

/**
 * @file user_ui.h
 * @brief UI 子模块兼容入口。
 *
 * 历史代码仍通过 #include "user_ui.h" 引入 UI 内部页面声明。新代码若只需要
 * 调用 UI 周期任务，应优先包含 "UI/user_ui_public.h"。
 *
 * 本兼容头用于渐进式拆分 user_ui.c，避免一次性修改 main.c、IAR 工程和
 * legacy 页面实现。后续页面完全迁移后，本文件可以进一步瘦身为仅包含
 * UI/user_ui_public.h。
 */

#include "UI/user_ui_internal.h"

#endif /* USER_UI_H */
