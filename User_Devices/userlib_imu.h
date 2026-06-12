#ifndef _USERLIB_IMU_H_
#define _USERLIB_IMU_H_

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  //整数类型库函数
#include <stdbool.h> //布尔类型库函数
#include <string.h>  //字符串操作库函数
#include <stdlib.h>  //标准库函数
#include <math.h>    //数学库函数

// 用户外设接口头文件
#include "userlib_sys.h"  // 系统时间和SysTick相关函数
#include "userlib_uart.h" // UART相关函数

// 用户设备头文件
#include "userlib_lbb.h" // LED、按钮和蜂鸣器相关函数

/// @brief IMU状态枚举
typedef enum
{
    IMU_STA_OK = 0,           /* 无错误 0 */
    IMU_STA_ERROR,            /* 通用错误 1 */
    IMU_STA_RX_BUSY,          /* 接收忙 2 */
    IMU_STA_RX_OVERFLOW,      /* 接收缓冲区溢出 3 */
    IMU_STA_RX_TIMEOUT,       /* 接收帧超时 4 */
    IMU_STA_RX_FORMAT_ERROR,  /* 接收帧格式错误 5 */
    IMU_STA_RX_CHECK_ERROR,   /* 接收帧校验错误 6 */
    IMU_STA_RX_LENGTH_ERROR,  /* 接收帧长度错误 7 */
    IMU_STA_RX_UNKNOWN_ERROR, /* 未知错误 8 */
    IMU_STA_TX_BUSY,          /* 发送忙 9 */
} IMU_Status_EnumTypeDef;

enum
{
    IMU_ODR_NONE,      // IMU指令空
    IMU_ODR_READDATA,  // IMU指令读取数据
    IMU_ODR_UNLOCK,    // IMU指令解锁
    IMU_ODR_LOCK,      // IMU指令锁定
    IMU_ODR_SETANGREF, // IMU指令设置角度参考
    IMU_ODR_SETYAWREF, // IMU指令设置偏航参考
};

typedef struct
{
    float accx;                    // x轴向加速度
    float accy;                    // y轴向加速度
    float accz;                    // z轴向加速度
    float gyrox;                   // 绕x轴旋转角速度
    float gyroy;                   // 绕y轴旋转角速度
    float gyroz;                   // 绕z轴旋转角速度
    float roll;                    // 横滚角
    float pitch;                   // 俯仰角
    float yaw;                     /* 偏航角 */
    float temperature;             /* 温度 */
    IMU_Status_EnumTypeDef status; /* 状态 */
    uint16_t tx_count;             /* 发送计数 */
    uint16_t rx_count;             /* 接收计数 */

    float sum_accx;  // x轴向加速度累加和
    float sum_accy;  // y轴向加速度累加和
    float sum_accz;  // z轴向加速度累加和
    float sum_gyrox; // 绕x轴旋转角速度累加和
    float sum_gyroy; // 绕y轴旋转角速度累加和
    float sum_gyroz; // 绕z轴旋转角速度累加和
} IMU_Data_StructTypeDef;

/// @brief IMU通信处理函数
/// @param  无
/// @note 该函数需要在1ms系统滴答定时器回调中调用，或者在主流程中1ms定时器中调用
void USER_IMU_Comm_Routine(void);

/**
 * @brief IMU初始化函数
 * @param useSysTick 是否使用SysTick定时器
 * @param data_ptr 指向IMU数据结构的指针
 * @param uart_channel 串口通道
 * @note 初始化指定串口通道的IMU模块，建立与外部数据的连接
 */
void USER_IMU_Init(bool useSysTick, IMU_Data_StructTypeDef *data_ptr, UART_Instance uart_channel);

/**
 * @brief 设置IMU指令
 * @param order IMU指令
 * @note 将IMU指令推送至串口发送队列
 */
void USER_IMU_SetOrder(uint8_t order);

#endif // _USERLIB_IMU_H_
