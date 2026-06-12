#ifndef USER_UI_H
#define USER_UI_H

/**
 * @file user_ui.h
 * @brief UI 模块统一入口。
 *
 * 已全面切换到注册表驱动的新 UI 架构，不再包含 legacy 页面实现。
 * 外部模块只需包含本头文件即可获得所有 UI 内部声明。
 * 若仅需调用周期任务，可轻量包含 "User_UI/user_ui_public.h"。
 */

#include "../User_UI/user_ui_internal.h"

#endif /* USER_UI_H */
