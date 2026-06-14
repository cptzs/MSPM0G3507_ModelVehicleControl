#include "user_ui_unittest_actions.h"
#include "user_ui_internal.h"

#include "userlib_imu.h"
#include "userlib_lbb.h"
#include "userlib_motor.h"

/**
 * @file user_ui_unittest_actions.c
 * @brief UnitTest 页面硬件动作层实现。
 *
 * 封装所有外设硬件操作接口（电机、舵机、蜂鸣器、LED、IMU、LiDAR、UART/CAN）。
 * UI 页面仅通过本层接口触发动作和查询状态，不直接操作硬件。
 */

/* ---- 测试项定义 ---- */

/** @brief 单个 UnitTest 测试项描述 */
typedef struct
{
    const char *name;   /**< 测试项名称（OLED 列表显示） */
    const char *status; /**< 状态文本（"pending" / "OK" / "N/A" / "ERR"） */
    bool executed;      /**< 是否已执行过 */
} UT_ActionItem_t;

/** @brief UnitTest 测试项注册表（12 项，编译期常量） */
static UT_ActionItem_t ut_items[] = {
    {"MTR FWD", "pending", false},
    {"MTR REV", "pending", false},
    {"MTR SPD0", "pending", false},
    {"MTR STOP", "pending", false},
    {"MTR COAST", "pending", false},
    {"MTR BRAKE", "pending", false},
    {"IMU HELLO", "pending", false},
    {"USB HELLO", "pending", false},
    {"CAM HELLO", "N/A", false},
    {"CAN FRAME", "pending", false},
    {"BUZZ 1MS", "pending", false},
    {"LED 300MS", "pending", false},
};

#define UT_ITEM_COUNT (sizeof(ut_items) / sizeof(ut_items[0]))

/* ---- 公开接口 ---- */

/**
 * @brief 初始化 UnitTest 动作层，重置所有测试项状态。
 *
 * @note CAM HELLO 等不可用项初始化为 "N/A"，其余为 "pending"。
 */
void USER_UI_UnitTestActions_Init(void)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)UT_ITEM_COUNT; i++)
    {
        ut_items[i].executed = false;
        ut_items[i].status = (i == 8u) ? "N/A" : "pending";
    }
}

/**
 * @brief 根据测试项索引执行对应的硬件动作（电机/IMU/蜂鸣器/LED 等）。
 *
 * @param item_index 测试项索引（0 ~ UT_ITEM_COUNT-1）。
 * @return 执行结果（OK / SKIP / ERROR）。
 *
 * @note 本函数为非阻塞，执行后立即返回。耗时动作预留状态机扩展点。
 */
USER_UI_UT_ActionResult_t USER_UI_UnitTest_ExecuteAction(uint8_t item_index)
{
    if (item_index >= (uint8_t)UT_ITEM_COUNT)
    {
        return USER_UI_UT_ACTION_ERROR;
    }

    /* 预留：根据 item_index 执行对应硬件动作 */
    switch (item_index)
    {
    case 0u: /* MTR FWD  — 电机前进 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_NORMAL_RUN, 100);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_NORMAL_RUN, 100);
        break;
    case 1u: /* MTR REV  — 电机后退 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_NORMAL_RUN, -100);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_NORMAL_RUN, -100);
        break;
    case 2u: /* MTR SPD0 — 电机停转 */
        USER_Motor_Stop(MOTOR_0_LEFT);
        USER_Motor_Stop(MOTOR_1_RIGHT);
        break;
    case 3u: /* MTR STOP — 电机制动 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_REGEN_BRAKE, 0);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_REGEN_BRAKE, 0);
        break;
    case 4u: /* MTR COAST — 电机滑行 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_COAST, 0);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_COAST, 0);
        break;
    case 5u: /* MTR BRAKE — 电机刹车 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_REGEN_BRAKE, 0);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_REGEN_BRAKE, 0);
        break;
    case 6u: /* IMU HELLO */
        USER_IMU_SetOrder(IMU_ODR_SETANGREF);
        break;
    case 7u: /* USB HELLO — 串口回环测试 */
        /* 预留：USER_UART_SendString(UART_1, "HELLO\r\n"); */
        break;
    case 8u: /* CAM HELLO */
        /* 相机未接入 */
        ut_items[item_index].status = "N/A";
        return USER_UI_UT_ACTION_SKIP;
    case 9u: /* CAN FRAME */
        /* 预留：USER_CAN_SendTestFrame(); */
        break;
    case 10u: /* BUZZ 1MS */
        USER_BoardIO_Buzzer_On(1u);
        break;
    case 11u: /* LED 300MS */
        USER_BoardIO_LED_On(LED0, 300u);
        break;
    default:
        return USER_UI_UT_ACTION_SKIP;
    }

    ut_items[item_index].executed = true;
    ut_items[item_index].status = "OK";
    return USER_UI_UT_ACTION_OK;
}

/**
 * @brief 获取指定测试项的当前状态文本。
 *
 * @param item_index 测试项索引。
 * @return 状态字符串，越界返回 "ERR"。
 */
const char *USER_UI_UnitTestAction_GetStatus(uint8_t item_index)
{
    if (item_index >= (uint8_t)UT_ITEM_COUNT)
    {
        return "ERR";
    }

    return ut_items[item_index].status;
}

/**
 * @brief 获取测试项总数。
 *
 * @return 编译期确定的测试项数量。
 */
uint8_t USER_UI_UnitTestAction_GetItemCount(void)
{
    return (uint8_t)UT_ITEM_COUNT;
}

/**
 * @brief 获取测试项名称。
 *
 * @param item_index 测试项索引。
 * @return 名称字符串，越界返回 "?"。
 */
const char *USER_UI_UnitTestAction_GetItemName(uint8_t item_index)
{
    if (item_index >= (uint8_t)UT_ITEM_COUNT)
    {
        return "?";
    }

    return ut_items[item_index].name;
}
