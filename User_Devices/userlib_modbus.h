#ifndef _USERLIB_MODBUS_H_
#define _USERLIB_MODBUS_H_

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
#include "userlib_uart.h"    // UART相关函数

/// @brief Modbus状态枚举
typedef enum
{
    MODBUS_STA_OK = 0,                   // 无错误 0
    MODBUS_STA_RX_BUSY,                  // 接收忙 1
    MODBUS_STA_RX_ID_MISMATCH,           // 接收帧起始标识不匹配 2
    MODBUS_STA_RX_OVERFLOW,              // 接收缓冲区溢出 3
    MODBUS_STA_RX_TIMEOUT,               // 接收帧超时 4
    MODBUS_STA_RX_FORMAT_ERROR,          // 接收帧格式错误 5
    MODBUS_STA_RX_CHECK_ERROR,           // 接收帧校验错误 6
    MODBUS_STA_RX_LENGTH_ERROR,          // 接收帧长度错误 7
    MODBUS_STA_RX_FUNC_NOT_EXIST,        // 接收帧功能码不存在 8
    MODBUS_STA_RX_REGISTER_OUT_OF_RANGE, // 寄存器地址超出范围 9
    MODBUS_STA_RX_UNKNOWN_ERROR,         // 未知错误 10
    MODBUS_STA_TX_BUSY,                  // 发送忙 11
} Modbus_Status_EnumTypeDef;

/// @brief Modbus功能码枚举
typedef enum
{
    // MODBUS_FC_READ_COILS = 0x01,                    // 读线圈寄存器
    // MODBUS_FC_READ_DISCRETE_INPUTS = 0x02,          // 读离散输入寄存器

    MODBUS_FC_READ_HOLDING_REGISTERS = 0x03, // 读多个保持寄存器（建议单次操作寄存器数量不超过8）

    // MODBUS_FC_READ_INPUT_REGISTERS = 0x04,   // 读输入寄存器
    // MODBUS_FC_WRITE_SINGLE_COIL = 0x05,      // 写单个线圈寄存器
    MODBUS_FC_WRITE_SINGLE_REGISTER = 0x06, // 写单个保持寄存器

    // MODBUS_FC_READ_EXCEPTION_STATUS = 0x07,  // 读异常状态
    // MODBUS_FC_DIAGNOSTICS = 0x08,            // 诊断
    // MODBUS_FC_GET_COMM_EVENT_COUNTER = 0x0B, // 获取通信事件计数
    // MODBUS_FC_GET_COMM_EVENT_LOG = 0x0C,     // 获取通信事件日志
    // MODBUS_FC_WRITE_MULTIPLE_COILS = 0x0F,   // 写多个线圈寄存器

    MODBUS_FC_WRITE_HOLDING_REGISTERS = 0x10, // 写多个保持寄存器（建议单次操作寄存器数量不超过8）

    // MODBUS_FC_REPORT_SLAVE_ID = 0x11,               // 报告从机ID
    // MODBUS_FC_READ_FILE_RECORD = 0x14,              // 读文件记录
    // MODBUS_FC_WRITE_FILE_RECORD = 0x15,             // 写文件记录
    // MODBUS_FC_MASK_WRITE_REGISTER = 0x16,           // 屏蔽写寄存器

    MODBUS_FC_READ_WRITE_HOLDING_REGISTERS = 0x17, // 读写多个保持寄存器（建议单次操作寄存器数量不超过8）

    // MODBUS_FC_READ_FIFO_QUEUE = 0x18,               // 读FIFO队列
    // MODBUS_FC_READ_DEVICE_IDENTIFICATION = 0x2B,    // 读设备识别

} Modbus_FC_EnumTypeDef;

/// @brief Modbus通信处理循环
/// @note 该循环函数需要定时调用，建议在1ms定时器中断或者1ms任务中调用
/// @param 无
void USER_Modbus_Comm_Routine(void);

/// @brief Modbus从机初始化
/// @param useSysTick 是否使用SysTick定时器
/// @param uart_channel 使用的UART通道
/// @param pStorage 寄存器存储区指针
/// @param pStatus Modbus状态指针
void USER_Modbus_Slave_Init(bool useSysTick, UART_Instance uart_channel, uint16_t *pStorage, uint8_t *pStatus);

#endif // _USERLIB_MODBUS_H_
