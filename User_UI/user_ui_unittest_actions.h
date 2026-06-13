#ifndef USER_UI_UNITTEST_ACTIONS_H
#define USER_UI_UNITTEST_ACTIONS_H

/**
 * @file user_ui_unittest_actions.h
 * @brief UnitTest 页面硬件动作层接口。
 *
 * 将 UnitTest 页面的硬件操作与 UI 显示解耦：
 * - UI 页面只负责列表选择与状态显示
 * - 本层封装所有外设动作（电机、舵机、蜂鸣器、LED、IMU、LiDAR、UART/CAN）
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief UnitTest 测试项动作结果 */
    typedef enum
    {
        USER_UI_UT_ACTION_OK = 0,
        USER_UI_UT_ACTION_SKIP,
        USER_UI_UT_ACTION_ERROR,
    } USER_UI_UT_ActionResult_t;

    /**
     * @brief 初始化 UnitTest 动作层。
     *
     * 由 main 初始化阶段调用，确保所有依赖的外设已就绪。
     */
    void USER_UI_UT_Actions_Init(void);

    /**
     * @brief 根据测试项索引执行对应的硬件动作。
     *
     * @param item_index 测试项索引（与 unittest_items[] 对应）。
     * @return 动作执行结果。
     *
     * @note 本函数应为非阻塞，耗时动作通过状态机在后台完成。
     */
    USER_UI_UT_ActionResult_t USER_UI_UT_Action_Execute(uint8_t item_index);

    /**
     * @brief 获取指定测试项的当前状态文本。
     *
     * @param item_index 测试项索引。
     * @return 状态字符串（如 "OK"、"FAIL"、"running" 等），不可为 NULL。
     */
    const char *USER_UI_UT_Action_GetStatus(uint8_t item_index);

    /**
     * @brief 获取测试项总数。
     */
    uint8_t USER_UI_UT_Action_GetItemCount(void);

    /**
     * @brief 获取测试项名称。
     */
    const char *USER_UI_UT_Action_GetItemName(uint8_t item_index);

#ifdef __cplusplus
}
#endif

#endif /* USER_UI_UNITTEST_ACTIONS_H */
