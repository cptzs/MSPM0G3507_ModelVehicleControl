#include "userlib_can.h"

// // RxFIFO状态结构体定义，此定义在底层库中已存在，复制在此仅供参考
// typedef struct
// {
//   uint32_t num;      // 接收FIFO编号
//   uint32_t fillLvl;  // 接收FIFO填充级别
//   uint32_t getIdx;   // 接收FIFO获取索引
//   uint32_t putIdx;   // 接收FIFO放置索引
//   uint32_t fifoFull; // 接收FIFO是否已满
//   uint32_t msgLost;  // 接收FIFO是否丢失消息
// } DL_MCAN_RxFIFOStatus;

// // 发送FIFO状态结构体定义，此定义在底层库中已存在，复制在此仅供参考
// typedef struct
// {
//   uint32_t freeLvl;  //  发送FIFO空闲级别
//   uint32_t putIdx;   // 发送FIFO获取索引
//   uint32_t getIdx;   // 发送FIFO放置索引
//   uint32_t putIdx;   // 发送FIFO放置索引
//   uint32_t fifoFull; // 发送FIFO是否已满
// } DL_MCAN_TxFIFOStatus;

// 以下是MCAN中断源的掩码定义和映射表
// 这些掩码用于检查和清除特定的中断标志位

/// @brief CAN中断源到MASK值的映射表
/// @note 该数组按照CAN_IntSource枚举顺序排列，用于将枚举值转换为硬件寄存器掩码
const uint32_t CAN_INTR_MASK_TABLE[CAN_INTR_MAX] = {
    MCAN_IR_RF0N_MASK, ///< [0]  Rx FIFO 0 新消息中断掩码
    MCAN_IR_RF0W_MASK, ///< [1]  Rx FIFO 0 水位中断掩码
    MCAN_IR_RF0F_MASK, ///< [2]  Rx FIFO 0 满中断掩码
    MCAN_IR_RF0L_MASK, ///< [3]  Rx FIFO 0 消息丢失中断掩码
    MCAN_IR_RF1N_MASK, ///< [4]  Rx FIFO 1 新消息中断掩码
    MCAN_IR_RF1W_MASK, ///< [5]  Rx FIFO 1 水位中断掩码
    MCAN_IR_RF1F_MASK, ///< [6]  Rx FIFO 1 满中断掩码
    MCAN_IR_RF1L_MASK, ///< [7]  Rx FIFO 1 消息丢失中断掩码
    MCAN_IR_HPM_MASK,  ///< [8]  高优先级消息中断掩码
    MCAN_IR_TC_MASK,   ///< [9]  发送完成中断掩码
    MCAN_IR_TCF_MASK,  ///< [10] 发送取消完成中断掩码
    MCAN_IR_TFE_MASK,  ///< [11] 发送FIFO空中断掩码
    MCAN_IR_TEFN_MASK, ///< [12] 发送事件FIFO新条目中断掩码
    MCAN_IR_TEFW_MASK, ///< [13] 发送事件FIFO水位中断掩码
    MCAN_IR_TEFF_MASK, ///< [14] 发送事件FIFO满中断掩码
    MCAN_IR_TEFL_MASK, ///< [15] 发送事件FIFO元素丢失中断掩码
    MCAN_IR_TSW_MASK,  ///< [16] 时间戳回绕中断掩码
    MCAN_IR_MRAF_MASK, ///< [17] 消息RAM访问失败中断掩码
    MCAN_IR_TOO_MASK,  ///< [18] 超时发生中断掩码
    MCAN_IR_DRX_MASK,  ///< [19] 专用Rx缓冲区中断掩码
    MCAN_IR_BEU_MASK,  ///< [20] 位错误未纠正中断掩码
    MCAN_IR_ELO_MASK,  ///< [21] 错误日志溢出中断掩码
    MCAN_IR_EP_MASK,   ///< [22] 错误被动中断掩码
    MCAN_IR_EW_MASK,   ///< [23] 警告状态中断掩码
    MCAN_IR_BO_MASK,   ///< [24] 总线关闭状态中断掩码
    MCAN_IR_WDI_MASK,  ///< [25] 看门狗中断掩码
    MCAN_IR_PEA_MASK,  ///< [26] 仲裁阶段协议错误中断掩码
    MCAN_IR_PED_MASK,  ///< [27] 数据阶段协议错误中断掩码
    MCAN_IR_ARA_MASK   ///< [28] 访问保留地址中断掩码
};

/// @brief CAN发送FIFO状态结构体
DL_MCAN_RxFIFOStatus rxFIFOStatus;

/// @brief CAN发送FIFO状态结构体
DL_MCAN_TxFIFOStatus txFIFOStatus;

// 定义发送帧结构体
DL_MCAN_TxBufElement txMsg;

// 定义接收帧结构体
DL_MCAN_RxBufElement rxMsg;

/// @brief CAN中断处理函数指针数组
/// @details 存储用户注册的各种中断回调函数，数组索引对应CAN_IntSource枚举值
void (*can_int_handler[CAN_INTR_MAX])(void) = {0};

/// @brief 获取当前CAN接收队列中的消息数量
/// @param None
/// @return 返回当前队列中的消息数量
uint16_t USER_CAN_GetRxFIFOAmount(void)
{
  // 初始化填充级别为0
  rxFIFOStatus.fillLvl = 0;
  // 获取接收FIFO状态
  DL_MCAN_getRxFIFOStatus(MCAN0_INST, &rxFIFOStatus);
  // 返回当前填充级别作为消息数量
  return rxFIFOStatus.fillLvl;
}

/// @brief 获取当前CAN发送队列中的消息数量
/// @param None
/// @return 返回当前队列中的消息数量
uint16_t USER_CAN_GetTxFIFOAmount(void)
{
  // 初始化填充级别为0
  txFIFOStatus.freeLvl = 0;
  // 获取发送FIFO状态
  DL_MCAN_getTxFIFOQueStatus(MCAN0_INST, &txFIFOStatus);
  // 返回当前空闲级别作为消息数量
  return txFIFOStatus.freeLvl;
}

/// @brief 从接收FIFO中获取一条消息
/// @param msg 用于存储接收消息的结构体指针
/// @return 如果FIFO为空则返回false，否则返回true
bool USER_CAN_ReadMessage(CAN_Msg_StructTypedef *msg)
{
  // 检查参数有效性
  if (msg == 0)
  {
    return false;
  }

  // 获取接收FIFO状态
  DL_MCAN_getRxFIFOStatus(MCAN0_INST, &rxFIFOStatus);

  // 检查FIFO是否为空
  if (rxFIFOStatus.fillLvl == 0)
  {
    // 如果FIFO为空，返回失败
    return false;
  }

  // 从接收FIFO读取消息
  DL_MCAN_readMsgRam(MCAN0_INST, DL_MCAN_MEM_TYPE_FIFO, rxFIFOStatus.getIdx,
                     rxFIFOStatus.num, &rxMsg);

  // 清除接收FIFO的消息
  DL_MCAN_writeRxFIFOAck(MCAN0_INST, DL_MCAN_RX_FIFO_NUM_0,
                         rxFIFOStatus.getIdx);

  // 将接收到的消息数据复制到用户提供的结构体中
  msg->id = rxMsg.id >> 18;                // 消息ID
  msg->dlc = rxMsg.dlc;                    // 数据长度
  memcpy(msg->data, rxMsg.data, msg->dlc); // 复制数据

  return true; // 成功获取消息
}

/// @brief 发送CAN消息
/// @param msg 要发送的消息结构体指针
///        msg->id: 消息ID
///        msg->dlc: 数据长度(0-8)
///        msg->data: 数据内容(最多8字节)
/// @return true: 发送成功, false: 发送失败
bool USER_CAN_SendMessage(CAN_Msg_StructTypedef *msg)
{

  // 参数有效性检查
  if (msg == 0 || msg->dlc > 8)
  {
    return false;
  }

  // 检查TX FIFO是否已满
  DL_MCAN_getTxFIFOQueStatus(MCAN0_INST, &txFIFOStatus);

  // 如果TX FIFO已满，返回发送失败
  if (txFIFOStatus.fifoFull)
  {
    return false;
  }

  // 配置发送消息
  txMsg.id = msg->id << 18; // 消息ID
  txMsg.dlc = msg->dlc;     // 数据长度

  // 复制数据
  memcpy(txMsg.data, msg->data, msg->dlc);

  // 写入发送缓冲区
  DL_MCAN_writeMsgRam(MCAN0_INST, DL_MCAN_MEM_TYPE_BUF, txFIFOStatus.putIdx,
                      &txMsg);

  // 请求发送缓冲区中对应的内容
  DL_MCAN_TXBufAddReq(MCAN0_INST, txFIFOStatus.putIdx);

  return true; // 返回发送成功
}

/// @brief 注册CAN中断处理函数
/// @param intrSrc 中断源（CAN_IntSource枚举值）
/// @param handler 中断处理函数指针，当指定中断发生时会调用此函数
/// @return true: 注册成功, false: 注册失败（中断源超出范围）
/// @note 如果handler为0，相当于注销该中断的回调函数
bool USER_CAN_RegisterCallback(CAN_IntSource intrSrc, void (*handler)(void))
{
  // 检查中断源是否在有效范围内
  if (intrSrc >= CAN_INTR_MAX)
  {
    return false; // 无效的中断源
  }

  // 注册中断处理函数（允许为0以注销回调）
  can_int_handler[intrSrc] = handler;

  return true; // 成功注册中断处理函数
}

/// @brief 注销指定的CAN中断处理函数
/// @param intrSrc 要注销的中断源（CAN_IntSource枚举值）
/// @return true: 注销成功, false: 注销失败（中断源超出范围或未注册回调函数）
bool USER_CAN_UnregisterCallback(CAN_IntSource intrSrc)
{
  // 检查中断源是否在有效范围内
  if (intrSrc >= CAN_INTR_MAX)
  {
    return false; // 无效的中断源
  }

  // 检查中断处理函数是否已注册
  if (can_int_handler[intrSrc] == 0)
  {
    return false; // 未找到该中断处理函数
  }

  // 注销中断处理函数
  can_int_handler[intrSrc] = 0;

  return true; // 成功注销中断处理函数
}

/// @brief 注销全部CAN中断处理函数
/// @details 清除所有已注册的中断回调函数，恢复到初始状态
/// @note 调用此函数后，所有中断将不再有用户回调函数响应
void USER_CAN_UnregisterAllCallbacks(void)
{
  // 清除所有中断处理函数指针
  memset(can_int_handler, 0, sizeof(can_int_handler));
}

/// @brief 初始化CAN模块
/// @details 执行CAN模块的完整初始化流程，包括：
///          - 配置接收FIFO状态
///          - 清除中断标志
///          - 初始化回调函数数组
///          - 启用CAN中断
///          - 检查模块工作状态
/// @return true: 初始化成功（CAN模块进入正常工作模式）
///         false: 初始化失败（CAN模块未能进入正常模式）
/// @note 调用此函数前应确保CAN模块的时钟和引脚已正确配置
bool USER_CAN_Init(void)
{
  // 配置接收FIFO状态结构体
  rxFIFOStatus.fillLvl = 0;                 // 清除FIFO填充级别
  rxFIFOStatus.num = DL_MCAN_RX_FIFO_NUM_0; // 使用FIFO 0作为主接收FIFO
  rxFIFOStatus.fifoFull = false;            // FIFO未满标志
  rxFIFOStatus.msgLost = false;             // 未丢失消息标志

  // 配置发送数据结构体
  txMsg.id = 0;  // 消息ID
  txMsg.rtr = 0; // 数据帧
  txMsg.xtd = 0; // 标准ID
  txMsg.esi = 0; // 错误状态指示符
  txMsg.dlc = 0; // 数据长度
  txMsg.brs = 0; // 不使用位速率切换
  txMsg.fdf = 0; // CAN格式(非CAN-FD)
  txMsg.efc = 0; // 事件FIFO控制（不启用）
  txMsg.mm = 0;  // 消息标记

  // 初始化CAN中断回调函数指针数组为0
  memset(can_int_handler, 0, sizeof(can_int_handler));

  // 启用MCAN0中断到NVIC（嵌套向量中断控制器）
  NVIC_EnableIRQ(MCAN0_INST_INT_IRQN);

  // 延时等待CAN模块完全启动
  delay_ms(10);

  // 检测CAN模块当前工作模式
  if (DL_MCAN_OPERATION_MODE_NORMAL != DL_MCAN_getOpMode(MCAN0_INST))
  {
    // CAN模块未进入正常工作模式，初始化失败
    return false;
  }

  // CAN模块已成功进入正常工作模式，初始化完成
  return true;
}

/// @brief CAN硬件中断服务程序
/// @details 处理MCAN模块产生的各种中断事件，包括：
///          - 接收FIFO新消息中断
///          - 发送完成中断
///          - 错误状态中断等
/// @note 该函数由硬件自动调用，不应由用户直接调用
void MCAN0_INST_IRQHandler(void)
{
  uint32_t pendingIntr; // 挂起的中断源
  uint32_t intrStatus;  // 当前中断状态寄存器的值
  uint32_t i;           // 循环变量，用于遍历中断源

  // 获取挂起的中断线
  pendingIntr = DL_MCAN_getPendingInterrupt(MCAN0_INST);

  // 检查是否为中断线1的中断
  if (pendingIntr == DL_MCAN_IIDX_LINE1)
  {
    // 获取当前中断状态寄存器的值
    intrStatus = DL_MCAN_getIntrStatus(MCAN0_INST);

    // 清除所有已处理的中断标志位
    DL_MCAN_clearIntrStatus(MCAN0_INST, intrStatus,
                            DL_MCAN_INTR_SRC_MCAN_LINE_1);

    // 遍历所有可能的中断源，检查并处理已触发的中断
    for (i = 0; i < CAN_INTR_MAX; i++)
    {
      // 检查当前中断源是否被触发
      if (intrStatus & CAN_INTR_MASK_TABLE[i])
      {
        // 检查是否有注册的回调函数
        if (can_int_handler[i] != 0)
        {
          can_int_handler[i]();
        }
        // 清除当前中断源的标志位
        intrStatus &= ~(CAN_INTR_MASK_TABLE[i]);
      }
    }
  }
}
