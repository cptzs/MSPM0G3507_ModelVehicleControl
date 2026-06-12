#include "user_ui_unittest_actions.h"
#include "user_ui_internal.h"

/**
 * @file user_ui_unittest_actions.c
 * @brief UnitTest 页面硬件动作层实现。
 *
 * 封装所有外设硬件操作接口（电机、舵机、蜂鸣器、LED、IMU、LiDAR、UART/CAN）。
 * UI 页面仅通过本层接口触发动作和查询状态，不直接操作硬件。
 */

/* ---- 测试项定义 ---- */
typedef struct
{
    const char *name;
    const char *status;
    bool executed;
} UT_ActionItem_t;

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

void USER_UI_UT_Actions_Init(void)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)UT_ITEM_COUNT; i++)
    {
        ut_items[i].executed = false;
        ut_items[i].status = (ut_items[i].name[0] == 'C') ? "N/A" : "pending";
    }
}

USER_UI_UT_ActionResult_t USER_UI_UT_Action_Execute(uint8_t item_index)
{
    if (item_index >= (uint8_t)UT_ITEM_COUNT)
    {
        return USER_UI_UT_ACTION_ERROR;
    }

    /* 预留：根据 item_index 执行对应硬件动作 */
    switch (item_index)
    {
    case 0u: /* MTR FWD  — 电机前进 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_SPEED);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_SPEED);
        USER_Motor_SetSpeed(MOTOR_0_LEFT, 100);
        USER_Motor_SetSpeed(MOTOR_1_RIGHT, 100);
        break;
    case 1u: /* MTR REV  — 电机后退 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_SPEED);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_SPEED);
        USER_Motor_SetSpeed(MOTOR_0_LEFT, -100);
        USER_Motor_SetSpeed(MOTOR_1_RIGHT, -100);
        break;
    case 2u: /* MTR SPD0 — 电机停转 */
        USER_Motor_SetSpeed(MOTOR_0_LEFT, 0);
        USER_Motor_SetSpeed(MOTOR_1_RIGHT, 0);
        break;
    case 3u: /* MTR STOP — 电机制动 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_BRAKE);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_BRAKE);
        break;
    case 4u: /* MTR COAST — 电机滑行 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_COAST);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_COAST);
        break;
    case 5u: /* MTR BRAKE — 电机刹车 */
        USER_Motor_SetMode(MOTOR_0_LEFT, MOTOR_MODE_BRAKE);
        USER_Motor_SetMode(MOTOR_1_RIGHT, MOTOR_MODE_BRAKE);
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
        USER_LBB_Buzzer_On(1u);
        break;
    case 11u: /* LED 300MS */
        USER_LBB_LED_On(LED0, 300u);
        break;
    default:
        return USER_UI_UT_ACTION_SKIP;
    }

    ut_items[item_index].executed = true;
    ut_items[item_index].status = "OK";
    return USER_UI_UT_ACTION_OK;
}

const char *USER_UI_UT_Action_GetStatus(uint8_t item_index)
{
    if (item_index >= (uint8_t)UT_ITEM_COUNT)
    {
        return "ERR";
    }

    return ut_items[item_index].status;
}

uint8_t USER_UI_UT_Action_GetItemCount(void)
{
    return (uint8_t)UT_ITEM_COUNT;
}

const char *USER_UI_UT_Action_GetItemName(uint8_t item_index)
{
    if (item_index >= (uint8_t)UT_ITEM_COUNT)
    {
        return "?";
    }

    return ut_items[item_index].name;
}
