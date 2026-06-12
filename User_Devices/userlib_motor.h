#ifndef _USERLIB_MOTOR_H_
#define _USERLIB_MOTOR_H_

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  //整数类型库函数
#include <stdbool.h> //布尔类型库函数
#include <string.h>  //字符串操作库函数
#include <stdlib.h>  //标准库函数
#include <math.h>    //数学库函数

// 用户外设接口头文件
#include "userlib_sys.h" // 系统时间和SysTick相关函数
#include "userlib_pwm.h" // PWM相关函数

// 用户设备头文件
// #define MOTOR_B_REVERSE // 设置左电机反转
#define MOTOR_A_REVERSE // 设置右电机反转

/// @brief 电机实例枚举
/// @details 定义了两个电机实例：左电机和右电机
typedef enum
{
    MOTOR_0_LEFT = 0, // 左电机
    MOTOR_1_RIGHT     // 右电机
} MOTOR_Instance_t;

/// @brief 电机模式枚举
/// @details 定义了三种电机模式：能量回收制动、正常运行和空挡滑行
typedef enum
{
    MOTOR_MODE_REGEN_BRAKE = 0, // 能量回收制动
    MOTOR_MODE_NORMAL_RUN,      // 正常运行
    MOTOR_MODE_COAST            // 空挡滑行
} MOTOR_Mode_t;

/// @brief 初始化电机模块
/// @param  无
void USER_Motor_Init(void);

/// @brief 仅设置电机速度
/// @param motor_id 电机ID，0左电机，1右电机
/// @param speed 电机速度（范围：-1000到1000）
/// @note 该函数设置电机速度，调用时将强制退出能量回收制动模式！
void USER_Motor_SetOutputValue(MOTOR_Instance_t motor_id, int16_t speed);

/// @brief 设置电机工作状态
/// @param motor_id 电机ID，0左电机，1右电机
/// @param mode 电机模式（0：能量回收制动，1：正常运行，2：空挡滑行）
/// @param speed 电机速度（范围：-1000到1000）
void USER_Motor_SetMode(MOTOR_Instance_t motor_id, MOTOR_Mode_t mode, int16_t speed);

/// @brief 停止电机
/// @param motor_id 电机ID，0左电机，1右电机
void USER_Motor_Stop(MOTOR_Instance_t motor_id);

/// @brief 获取电机当前工作模式
/// @param motor_id 电机ID，0左电机，1右电机
/// @return 当前电机工作模式
MOTOR_Mode_t USER_Motor_GetMode(MOTOR_Instance_t motor_id);

/// @brief 获取电机输出速度
/// @param motor_id 电机ID，0左电机，1右电机
/// @return 当前电机输出速度（范围：-1000到1000）
int16_t USER_Motor_GetOutputValue(MOTOR_Instance_t motor_id);

#endif //_USERLIB_MOTOR_H_