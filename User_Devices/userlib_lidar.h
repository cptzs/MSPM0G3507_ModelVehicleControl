#ifndef _USERLIB_LIDAR_H
#define _USERLIB_LIDAR_H

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  //整数类型库函数
#include <stdbool.h> //布尔类型库函数
#include <string.h>  //字符串操作库函数
#include <stdlib.h>  //标准库函数
#include <math.h>    //数学库函数

// 用户外设接口头文件
#include "userlib_systick.h" // SysTick驱动接口
#include "userlib_can.h"     // CAN通信相关函数

// 用户设备头文件
#include "userlib_lbb.h" // LED、按钮和蜂鸣器相关函数

#define LIDAR_COUNT 4       // 定义Lidar数量
#define LIDAR_RX_TIMEOUT 20 // 定义Lidar最大允许的接收超时次数
#define LIDAR_TX_INTERVAL 8 // 定义Lidar发送间隔时间（单位：ms）

/// @brief 激光雷达状态枚举
typedef enum
{
    LIDAR_STA_OK = 0,                     // 测量距离有效
    LIDAR_STA_STD_DEV_HIGH = 1,           // 标准差大于15mm
    LIDAR_STA_SIGNAL_LOW = 2,             // 信号强度低于1Mcps
    LIDAR_STA_DISTANCE_LOW = 3,           // 测距低于阈值
    LIDAR_STA_PHASE_OUT_OF_BOUNDS = 4,    // 相位超出界限
    LIDAR_STA_PHASE_MISMATCH = 7,         // 相位不匹配
    LIDAR_STA_SIGNAL_BELOW_CROSSTALK = 9, // 信号低于串扰阈值
    LIDAR_STA_MULTIPLE_TARGETS = 11,      // 多个目标距离
    LIDAR_STA_SIGNAL_WEAK = 12,           // 信号强度弱
    LIDAR_STA_DISTANCE_INVALID = 14,      // 测量距离无效
    LIDAR_STA_ID_ERROR = 252,             // ID错误
    LIDAR_STA_FRAME_ERROR = 253,          // 帧错误
    LIDAR_STA_TIMEOUT = 254,              // 无响应
    LIDAR_STA_NO_TARGET = 255             // 未检测到目标
} LidarSt_t_Typedef;

/// @brief Lidar接收数据结构体定义
typedef struct
{
    uint32_t id;              // 消息ID
    uint32_t distance;        // 距离数据
    LidarSt_t_Typedef status; // 状态字节
    uint16_t strength;        // 强度数据
    uint16_t reserved;        // 保留字段
    uint32_t rx_count;        // 接收帧计数
    uint32_t tx_count;        // 发送帧计数
    uint16_t timeout_count;   // 接收超时计数
} Lidar_Data_Typedef;

/**
 * @brief 定义Lidar数量+1，便于处理雷达序号和索引号同步
 */
#define LIDAR_COUNT_P1 (LIDAR_COUNT + 1)

/**
 * @brief 解析lidar接收数据
 * @note 该函数需要在主流程中调用，调用间隔应为8ms
 */
void USER_LIDAR_Task(void);

/**
 * @brief 初始化lidar模块
 * @param data_ptr 外部数据结构指针数组
 * @note 该函数会初始化lidar的CAN通信和相关数据结构，设置外部数据指针
 */
void USER_lidar_Init(Lidar_Data_Typedef *data_ptr);

#endif // _USERLIB_LIDAR_H