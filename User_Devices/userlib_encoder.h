#ifndef USERLIB_ENCODER_H
#define USERLIB_ENCODER_H

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

// 用户设备头文件
#include "userlib_lbb.h" // LED、按钮和蜂鸣器相关函数

#define ENCODER_COUNT 3       // 定义编码器数量
#define TIM_MAX_COUNTER 65535 // 定义计数器最大值

/// @brief 编码器状态枚举
typedef enum
{
    ENCODER_STA_OK,       // 编码器正常工作状态
    ENCODER_STA_UNINIT,   // 编码器未初始化状态
    ENCODER_STA_OVERFLOW, // 编码器溢出状态
    ENCODER_STA_NO_PULSE, // 编码器无脉冲状态
    ENCODER_STA_UNKNOWN,  // 编码器未知状态
} EncoderStatus_t_Typedef;

/// @brief 编码器数据结构体
typedef struct
{
    int16_t speed;                  /// @brief 编码器速度
    EncoderStatus_t_Typedef status; /// @brief 编码器状态
    int8_t direction;               /// @brief 编码器方向
    int32_t sum_distance;           /// @brief 编码器行进距离积分
} EncoderData_t_Typedef;

/// @brief 编码器全部初始化函数
/// @details 使用外部传入的encoder_data数组初始化所有编码器
/// @param encoder_data_array 指向编码器数据数组的指针
/// @return 初始化结果
bool USER_ENCODER_Init(EncoderData_t_Typedef *encoder_data_array);

#endif // USERLIB_ENCODER_H