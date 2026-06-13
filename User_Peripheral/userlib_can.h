#ifndef __USER_CAN_H__
#define __USER_CAN_H__

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  /* 整数类型库函数 */
#include <stdbool.h> /* 布尔类型库函数 */
#include <string.h>  /* 字符串操作库函数 */

// 用户外设接口头文件
#include "userlib_systick.h" // SysTick驱动接口

//*****************CAN中断源定义******************/

/// @brief CAN中断源枚举
typedef enum
{
  CAN_INTR_RF0N = 0, ///< Rx FIFO 0 新消息中断
  CAN_INTR_RF0W,     ///< Rx FIFO 0 水位中断
  CAN_INTR_RF0F,     ///< Rx FIFO 0 满中断
  CAN_INTR_RF0L,     ///< Rx FIFO 0 消息丢失中断
  CAN_INTR_RF1N,     ///< Rx FIFO 1 新消息中断
  CAN_INTR_RF1W,     ///< Rx FIFO 1 水位中断
  CAN_INTR_RF1F,     ///< Rx FIFO 1 满中断
  CAN_INTR_RF1L,     ///< Rx FIFO 1 消息丢失中断
  CAN_INTR_HPM,      ///< 高优先级消息中断
  CAN_INTR_TC,       ///< 发送完成中断
  CAN_INTR_TCF,      ///< 发送取消完成中断
  CAN_INTR_TFE,      ///< 发送FIFO空中断
  CAN_INTR_TEFN,     ///< 发送事件FIFO新条目中断
  CAN_INTR_TEFW,     ///< 发送事件FIFO水位中断
  CAN_INTR_TEFF,     ///< 发送事件FIFO满中断
  CAN_INTR_TEFL,     ///< 发送事件FIFO元素丢失中断
  CAN_INTR_TSW,      ///< 时间戳回绕中断
  CAN_INTR_MRAF,     ///< 消息RAM访问失败中断
  CAN_INTR_TOO,      ///< 超时发生中断
  CAN_INTR_DRX,      ///< 专用Rx缓冲区中断
  CAN_INTR_BEU,      ///< 位错误未纠正中断
  CAN_INTR_ELO,      ///< 错误日志溢出中断
  CAN_INTR_EP,       ///< 错误被动中断
  CAN_INTR_EW,       ///< 警告状态中断
  CAN_INTR_BO,       ///< 总线关闭状态中断
  CAN_INTR_WDI,      ///< 看门狗中断
  CAN_INTR_PEA,      ///< 仲裁阶段协议错误中断
  CAN_INTR_PED,      ///< 数据阶段协议错误中断
  CAN_INTR_ARA,      ///< 访问保留地址中断
  CAN_INTR_MAX       ///< 中断源总数（用于数组大小等）
} CAN_IntSource;

//*****************CAN数据定义******************/
typedef struct
{
  uint32_t id;     // 消息ID
  uint8_t dlc;     // 数据长度
  uint8_t data[8]; // 数据
} CAN_Msg_StructTypedef;

//*****************外部变量声明******************/

//*****************函数声明******************/

/// @brief 初始化CAN模块
/// @return true: 初始化成功, false: 初始化失败
bool USER_CAN_Init(void);

/// @brief 获取接收FIFO中的消息数量
/// @return 当前队列中的消息数量
uint16_t USER_CAN_GetRxFIFOAmount(void);

/// @brief 获取发送FIFO中的消息数量
/// @return 当前队列中的消息数量
uint16_t USER_CAN_GetTxFIFOAmount(void);

/// @brief 发送CAN消息
/// @param msg 要发送的消息结构体指针
/// @return true: 发送成功, false: 发送失败
bool USER_CAN_SendMessage(CAN_Msg_StructTypedef *msg);

/// @brief 从接收FIFO中获取一条消息
/// @param msg 用于存储接收消息的结构体指针
/// @return true: 读取成功, false: FIFO为空或参数无效
bool USER_CAN_ReadMessage(CAN_Msg_StructTypedef *msg);

/// @brief 注册CAN中断处理函数
/// @param intrSrc 中断源（CAN_IntSource枚举值）
/// @param handler 中断处理函数指针，当指定中断发生时会调用此函数
/// @return true: 注册成功, false: 注册失败（中断源超出范围）
/// @note 如果handler为NULL，相当于注销该中断的回调函数
bool USER_CAN_RegisterCallback(CAN_IntSource intrSrc, void (*handler)(void));

/// @brief 注销指定的CAN中断处理函数
/// @param intrSrc 要注销的中断源（CAN_IntSource枚举值）
/// @return true: 注销成功, false: 注销失败（中断源超出范围或未注册回调函数）
bool USER_CAN_UnregisterCallback(CAN_IntSource intrSrc);

/// @brief 注销全部CAN中断处理函数
/// @details 清除所有已注册的中断回调函数，恢复到初始状态
/// @note 调用此函数后，所有中断将不再有用户回调
void USER_CAN_UnregisterAllCallbacks(void);

#endif // __USER_CAN_H__