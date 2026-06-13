#ifndef USER_UI_PUBLIC_H
#define USER_UI_PUBLIC_H

/**
 * @file user_ui_public.h
 * @brief UI 子模块对外公开接口。
 *
 * 外部模块只应依赖本头文件中的公开入口，不应直接依赖 UI 页面枚举、
 * 页面绘制函数或具体页面实现。
 */

/**
 * @brief UI 模块周期任务。
 *
 * 由协作式调度器周期调用，负责页面导航、按键交互和当前页面刷新。
 */
void USER_UI_Init(void);
void USER_UI_Task(void);

#endif /* USER_UI_PUBLIC_H */
