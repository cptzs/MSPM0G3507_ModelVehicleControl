#ifndef USERLIB_ENCODER_H
#define USERLIB_ENCODER_H

// TI 底层库头文件
#include "ti_msp_dl_config.h"

// C 标准库头文件
#include <stdint.h>  // 整数类型
#include <stdbool.h> // bool 类型
#include <string.h>  // 字符串/内存操作
#include <stdlib.h>  // 标准库
#include <math.h>    // 数学库

// 用户外设接口头文件
#include "userlib_systick.h" // SysTick 驱动接口

// 用户设备头文件
#include "userlib_lbb.h" // LED、按键和蜂鸣器相关接口

#define ENCODER_COUNT 3       // 编码器数量：0=路程计，1=左轮，2=右轮
#define TIM_MAX_COUNTER 65535 // 16bit GPTIMER 外部计数器重装值

/// @brief 编码器状态枚举。
typedef enum
{
    ENCODER_STA_OK,       // 编码器正常工作
    ENCODER_STA_UNINIT,   // 编码器未初始化
    ENCODER_STA_OVERFLOW, // 编码器计数溢出
    ENCODER_STA_NO_PULSE, // 当前周期无脉冲
    ENCODER_STA_UNKNOWN,  // 未知状态
} EncoderStatus_t_Typedef;

/// @brief 对上层公开的编码器数据结构。
typedef struct
{
    int16_t speed;                  /// @brief 当前发布周期内的有符号脉冲数。
    EncoderStatus_t_Typedef status; /// @brief 编码器状态。
    int8_t direction;               /// @brief 当前方向，1=正向，-1=反向。
    int32_t sum_distance;           /// @brief 累计有符号脉冲数。
} EncoderData_t_Typedef;

/**
 * @brief 初始化全部编码器。
 * @details 使用外部传入的 encoder_data 数组初始化 3 路编码器，并启动 GPTIMER 外部计数。
 * @param encoder_data_array 指向编码器数据数组的指针，数组长度应为 ENCODER_COUNT。
 * @return true 初始化成功；false 至少一路初始化失败或参数非法。
 */
bool USER_Encoder_Init(EncoderData_t_Typedef *encoder_data_array);

/**
 * @brief 编码器前台任务。
 * @details 由协作式调度器周期调用，读取 SysTick 发布的影子样本并更新公开数据。
 * @retval 无。
 */
void USER_Encoder_Task(void);

#endif // USERLIB_ENCODER_H
