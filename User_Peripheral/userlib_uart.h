#ifndef USERLIB_UART_H
#define USERLIB_UART_H

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  /* 整数类型库函数 */
#include <stdbool.h> /* 布尔类型库函数 */

// 用户外设接口头文件
#include "userlib_sys.h" // 系统时间和SysTick相关函数

/// @brief UART实例枚举
typedef enum
{
    UART_0 = 0,
    UART_1,
    UART_2,
    UART_3
} UART_Instance;

/// @brief UART中断类型
typedef enum
{
    UART_INTERRUPT_DONE_TX,     // uart传输完成中断
    UART_INTERRUPT_DONE_RX,     // uart接收完成中断
    UART_INTERRUPT_DMA_DONE_TX, // uart DMA传输完成中断
    UART_INTERRUPT_DMA_DONE_RX, // uart DMA接收完成中断
} UART_Interrupt;

/// @brief UART方法枚举
typedef enum
{
    UART_METHOD_NONE = 0, // 不接收
    UART_METHOD_POLLING,  // 轮询接收
    UART_METHOD_DMA,      // DMA接收
    UART_METHOD_INTERRUPT // 中断接收
} UART_Method;

/// @brief UART状态枚举
typedef enum
{
    UART_STA_OK,
    UART_STA_TIMEOUT,
    UART_STA_OVERFLOW,
    UART_STA_UNKNOWN_ERROR
} UART_Status;

/// @brief 初始化UART实例
/// @param uart_inst UART实例
void USER_UART_Init(UART_Instance uart_inst);

/// @brief 停止UART实例工作
/// @param uart_inst UART实例
void USER_UART_Deinit(UART_Instance uart_inst);

/// @brief 使用堵塞方式发送数据到UART
/// @param uart_inst UART实例
/// @param tx_data 待发送数据指针
/// @param length 待发送数据长度
/// @param timeout_ms 超时时间（毫秒），如果为0则表示不启用超时
/// @return 发送是否成功
bool USER_UART_Transmit(UART_Instance uart_inst, uint8_t *tx_data, uint16_t length, uint32_t timeout_ms);

/// @brief 使用中断方式发送数据到UART
/// @param uart_inst UART实例
/// @param tx_data 待发送数据指针
/// @param length 待发送数据长度
/// @return 发送是否成功
bool USER_UART_Transmit_IT(UART_Instance uart_inst, uint8_t *tx_data, uint16_t length);

/// @brief 使用DMA方式发送数据到UART
/// @param uart_inst UART实例
/// @param tx_data 待发送数据指针
/// @param length 待发送数据长度
/// @return 发送是否成功
bool USER_UART_Transmit_DMA(UART_Instance uart_inst, uint8_t *tx_data, uint16_t length);

/// @brief 中止UART发送
/// @param uart_inst UART实例
/// @return 是否成功
bool USER_UART_Abort_Transmit(UART_Instance uart_inst);

/// @brief 使用堵塞方式从UART接收数据
/// @param uart_inst UART实例
/// @param rx_data 待接收数据指针
/// @param length 待接收数据长度
/// @return 接收是否成功
bool USER_UART_Receive(UART_Instance uart_inst, uint8_t *rx_data, uint16_t length, uint32_t timeout_ms);

/// @brief 使用中断方式从UART接收数据
/// @param uart_inst UART实例
/// @param rx_data 待接收数据指针
/// @param length 待接收数据长度
/// @return 接收是否成功
bool USER_UART_Receive_IT(UART_Instance uart_inst, uint8_t *rx_data, uint16_t length);

/// @brief 使用DMA从UART接收数据
/// @param uart_inst UART实例
/// @param rx_data 待接收数据指针
/// @param length 待接收数据长度
/// @return 接收是否成功
bool USER_UART_Receive_DMA(UART_Instance uart_inst, uint8_t *rx_data, uint16_t length);

/// @brief 中止UART接收
/// @param uart_inst UART实例
/// @return 是否成功
bool USER_UART_Abort_Receive(UART_Instance uart_inst);

/// @brief 获取UART实例已接收的字节数
/// @param uart_inst UART实例
/// @return 已接收的字节数
uint16_t USER_UART_GetRecievedBytes(UART_Instance uart_inst);

/// @brief 获取UART实例已发送的字节数
/// @param uart_inst UART实例
/// @return 已发送的字节数
uint16_t USER_UART_GetSentBytes(UART_Instance uart_inst);

/// @brief 获取UART实例DMA已发送的字节数
/// @param uart_inst UART实例
/// @return 已发送的字节数
uint16_t USER_UART_GetSentBytes_DMA(UART_Instance uart_inst);

/// @brief 获取UART实例DMA已接收的字节数
/// @param uart_inst UART实例
/// @return 已接收的字节数
uint16_t USER_UART_GetRecievedBytes_DMA(UART_Instance uart_inst);

/// @brief 注册UART中断回调函数
/// @param uart_inst UART实例
/// @param interrupt_type 中断类型
/// @param callback 回调函数指针
void USER_UART_RegisterCallback(UART_Instance uart_inst, UART_Interrupt interrupt_type, void (*callback)(void));

/// @brief 取消注册UART中断回调函数
/// @param uart_inst UART实例
/// @param interrupt_type 中断类型
void USER_UART_UnregisterCallback(UART_Instance uart_inst, UART_Interrupt interrupt_type);

#endif //_USERLIB_UART_H_