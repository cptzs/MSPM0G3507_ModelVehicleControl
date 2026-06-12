#include "userlib_motor.h"
/*
电机采用PWM控制方式，使用定时器A1的PWM0和PWM1通道来控制左电机和右电机的速度。
左电机使用PWM0通道（PB8），右电机使用PWM1通道（PB12）。
电机速度范围为-1000到1000，负值表示反转，正值表示正转。
电机模式包括能量回收制动、正常运行和空挡滑行。
电机方向通过IO口控制，左电机方向由PB9控制，右电机方向由PB13控制。
电机使能通过IO口控制（高电平使能），左电机使能由PB10控制，右电机使能由PB14控制。
电机模式：
能量回收制动：电机使能，方向随意，PWM输出为0
正常运行：电机使能，方向按照速度正负设置，PWM输出为速度值
空挡滑行：电机不使能，方向随意，PWM输出为0
*/

#define MOTOR_A_CH GPIO_PWM_0_C0_IDX // 左电机PWM通道
#define MOTOR_B_CH GPIO_PWM_0_C1_IDX // 右电机PWM通道

int16_t speed_cache[2] = {0, 0};                                             // 缓存电机速度
MOTOR_Mode_t motor_mode[2] = {MOTOR_MODE_NORMAL_RUN, MOTOR_MODE_NORMAL_RUN}; // 缓存电机工作模式

/// @brief 设置电机输出速度
/// @param motor_id 电机ID，0左电机，1右电机
/// @param speed 电机速度（范围：-1000到1000）
void USER_Motor_SetOutputValue(MOTOR_Instance_t motor_id, int16_t speed)
{
    // 限制速度范围
    if (speed > 1000)
    {
        speed = 1000;
    }
    else if (speed < -1000)
    {
        speed = -1000;
    }

    switch (motor_id)
    {
    case MOTOR_0_LEFT:
    {
        speed_cache[0] = speed; // 缓存左电机速度
#ifdef MOTOR_B_REVERSE
        if (speed < 0)
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_PH_B_PIN);                       // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_B_CH, (uint16_t)(-speed)); // 设置PWM占空比为负速度值
        }
        else
        {
            DL_GPIO_clearPins(MOTOR_PORT, MOTOR_PH_B_PIN);                  // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_B_CH, (uint16_t)speed); // 设置PWM占空比为正速度值
        }
#else
        if (speed < 0)
        {
            DL_GPIO_clearPins(MOTOR_PORT, MOTOR_PH_B_PIN);                     // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_B_CH, (uint16_t)(-speed)); // 设置PWM占空比为负速度值
        }
        else
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_PH_B_PIN);                    // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_B_CH, (uint16_t)speed); // 设置PWM占空比为正速度值
        }
#endif
        break;
    }

    case MOTOR_1_RIGHT:
    {
        speed_cache[1] = speed; // 缓存右电机速度
#ifdef MOTOR_A_REVERSE
        if (speed < 0)
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_PH_A_PIN);                       // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_A_CH, (uint16_t)(-speed)); // 设置PWM占空比为负速度值
        }
        else
        {
            DL_GPIO_clearPins(MOTOR_PORT, MOTOR_PH_A_PIN);                  // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_A_CH, (uint16_t)speed); // 设置PWM占空比为正速度值
        }
#else
        if (speed < 0)
        {
            DL_GPIO_clearPins(MOTOR_PORT, MOTOR_PH_A_PIN);                     // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_A_CH, (uint16_t)(-speed)); // 设置PWM占空比为负速度值
        }
        else
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_PH_A_PIN);                    // 设置方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_A_CH, (uint16_t)speed); // 设置PWM占空比为正速度值
        }
#endif
        break;
    }
    default:
        break;
    }
}

/// @brief 获取电机速度
/// @param motor_id 电机ID
/// @return 电机速度（范围：-1000到1000）
int16_t USER_Motor_GetOutputValue(MOTOR_Instance_t motor_id)
{
    switch (motor_id)
    {
    case MOTOR_0_LEFT:
        return speed_cache[0]; // 返回左电机速度
    case MOTOR_1_RIGHT:
        return speed_cache[1]; // 返回右电机速度
    default:
        return 0; // 无效电机ID，返回0
    }
}

/// @brief 设置电机工作状态
/// @param motor_id 电机ID，0左电机，1右电机
/// @param mode 电机模式（0：能量回收制动，1：正常运行，2：空挡滑行）
/// @param speed 电机速度（范围：-1000到1000）
void USER_Motor_SetMode(MOTOR_Instance_t motor_id, MOTOR_Mode_t mode, int16_t speed)
{
    if (motor_id > MOTOR_1_RIGHT)
    {
        return;
    }

    // 更新电机模式
    motor_mode[motor_id] = mode;

    // 设置电机输出
    switch (mode)
    {
    case MOTOR_MODE_REGEN_BRAKE: // 能量回收制动
    {
        switch (motor_id)
        {
        case MOTOR_0_LEFT:
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_NSLP_B_PIN);    // 使能左电机
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_PH_B_PIN);      // 设置左电机方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_B_CH, 0); // 设置左电机PWM占空比为0
            break;
        }
        case MOTOR_1_RIGHT:
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_NSLP_A_PIN);    // 使能右电机
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_PH_A_PIN);      // 设置右电机方向
            USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_A_CH, 0); // 设置右电机PWM占空比为0
            break;
        }
        default:
            break;
        }
        break;
    }

    case MOTOR_MODE_NORMAL_RUN: // 正常运行
    {
        switch (motor_id)
        {
        case MOTOR_0_LEFT:
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_NSLP_B_PIN); // 使能电机
            USER_Motor_SetOutputValue(motor_id, speed);    // 设置左电机速度
            break;
        }

        case MOTOR_1_RIGHT:
        {
            DL_GPIO_setPins(MOTOR_PORT, MOTOR_NSLP_A_PIN); // 使能电机
            USER_Motor_SetOutputValue(motor_id, speed);    // 设置右电机速度
            break;
        }
        default:
            break;
        }
        break;
    }

    case MOTOR_MODE_COAST: // 空挡滑行
    {
        switch (motor_id)
        {
        case MOTOR_0_LEFT:
        {
            DL_GPIO_clearPins(MOTOR_PORT, MOTOR_NSLP_B_PIN); // 禁用左电机
            break;
        }
        case MOTOR_1_RIGHT:
        {
            DL_GPIO_clearPins(MOTOR_PORT, MOTOR_NSLP_A_PIN); // 禁用右电机
            break;
        }
        default:
            break;
        }
        break;
    }

    default:
        break;
    }
}

/// @brief 获取电机当前工作模式
/// @param motor_id 电机ID（MOTOR_0_LEFT 或 MOTOR_1_RIGHT）
/// @return 当前电机工作模式（MOTOR_Mode_t 枚举值），无效ID返回 MOTOR_MODE_NORMAL_RUN
MOTOR_Mode_t USER_Motor_GetMode(MOTOR_Instance_t motor_id)
{
    switch (motor_id)
    {
    case MOTOR_0_LEFT:
        return motor_mode[0]; // 返回左电机模式
    case MOTOR_1_RIGHT:
        return motor_mode[1]; // 返回右电机模式
    default:
        return MOTOR_MODE_NORMAL_RUN; // 无效电机ID，返回默认模式
    }
}

/// @brief 停止电机
/// @param motor_id 电机ID，0左电机，1右电机
void USER_Motor_Stop(MOTOR_Instance_t motor_id)
{
    switch (motor_id)
    {
    case MOTOR_0_LEFT:
        DL_GPIO_clearPins(MOTOR_PORT, MOTOR_NSLP_B_PIN); // 禁用左电机
        break;
    case MOTOR_1_RIGHT:
        DL_GPIO_clearPins(MOTOR_PORT, MOTOR_NSLP_A_PIN); // 禁用右电机
        break;
    default:
        break;
    }
}

/// @brief 初始化电机模块
/// @param  无
void USER_Motor_Init(void)
{
    // 初始化PWM模块
    USER_PWM_SetFrequency(PWM_0_INST, 15000, 1000);   // 设置PWM频率为1000Hz，重载值为1000
    USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_B_CH, 0); // 初始化左电机PWM占空比为0
    USER_PWM_SetDutyCycle(PWM_0_INST, MOTOR_A_CH, 0); // 初始化右电机PWM占空比为0
    USER_PWM_Start(PWM_0_INST);                       // 启动PWM模块

    // 初始化GPIO端口
    DL_GPIO_setPins(MOTOR_PORT, MOTOR_NSLP_B_PIN | MOTOR_PH_B_PIN); // 设置左电机使能和方向引脚
    DL_GPIO_setPins(MOTOR_PORT, MOTOR_NSLP_A_PIN | MOTOR_PH_A_PIN); // 设置右电机使能和方向引脚
}
