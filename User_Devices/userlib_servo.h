#ifndef USERLIB_SERVO_H
#define USERLIB_SERVO_H

// TI底层库头文件
#include "ti_msp_dl_config.h"
// C标准库头文件
#include <stdint.h>  //整数类型库函数
#include <stdbool.h> //布尔类型库函数
#include <string.h>  //字符串操作库函数
#include <stdlib.h>  //标准库函数
#include <math.h>    //数学库函数

// 用户外设接口头文件

/// @brief 舵机实例枚举
typedef enum
{
    SERVO_0 = 0, // 舵机0
    SERVO_1,     // 舵机1
    SERVO_2,     // 舵机2
    SERVO_3      // 舵机3
} Servo_Instance;

typedef struct
{
    Servo_Instance instance;      // 舵机实例
    int16_t max_angle;            // 舵机最大角度
    int16_t min_angle;            // 舵机最小角度
    int16_t center_angle;         // 舵机中心角度
    int16_t max_angle_pulsewidth; // 最大角度对应的脉冲宽度，单位：us
    int16_t min_angle_pulsewidth; // 最小角度对应的脉冲宽度，单位：us
    GPIO_Regs *port;              // 舵机控制端口
    uint32_t pin;                 // 舵机控制引脚
} ServoConfig_Struct_TypeDef;

/// @brief 使能舵机
/// @param servo_index 舵机索引，范围为0-3
/// @return bool 使能成功返回true，失败返回false
bool USER_Servo_Enable(Servo_Instance servo_index);

/// @brief 禁用舵机
/// @param servo_index 舵机索引，范围为0-3
/// @return bool 禁用成功返回true，失败返回false
bool USER_Servo_Disable(Servo_Instance servo_index);

/// @brief 计算舵机脉冲宽度
/// @param relative_angle 舵机相对角度，范围为[min_angle, max_angle]，左转为负，右转为正
/// @param servo_index 舵机索引，范围为0-3
/// @return 计算得到的脉冲宽度，单位为us
uint16_t USER_Servo_CalculatePulseWidth(Servo_Instance servo_index, int16_t relative_angle);

/// @brief 设置舵机角度
/// @param servo_index 舵机索引，范围为0-3
/// @param angle 舵机角度，范围为[min_angle, max_angle]
/// @return bool 设置成功返回true，失败返回false
bool USER_Servo_SetAngle(Servo_Instance servo_index, int16_t angle);

/// @brief 获取舵机当前角度
/// @param servo_index 舵机索引，范围为0-3
/// @return 当前舵机角度，单位为度
int16_t USER_Servo_GetAngle(Servo_Instance servo_index);

/// @brief 启动舵机控制
void USER_Servo_Start();

/// @brief 停止舵机控制
void USER_Servo_Stop();

/// @brief 获取指定索引的舵机当前脉冲宽度
/// @param servo_index 舵机索引
/// @return 当前脉冲宽度，单位：us
uint16_t USER_Servo_GetWidth(Servo_Instance servo_index);

/// @brief 获取舵机误差修正系数
/// @return 当前误差修正系数
float USER_Servo_GetFError();

/// @brief 配置舵机用定时器
/// @return bool 配置成功返回true，失败返回false
void USER_Servo_TimerConfig();

/// @brief 初始化舵机配置
/// @param servo_config 舵机配置结构体指针
/// @return bool 初始化成功返回true，失败返回false
bool USER_Servo_Config(ServoConfig_Struct_TypeDef *servo_config);

/// @brief 初始化全部舵机
/// @return bool 初始化成功返回true，失败返回false
bool USER_Servo_Init();

#endif // USERLIB_SERVO_H