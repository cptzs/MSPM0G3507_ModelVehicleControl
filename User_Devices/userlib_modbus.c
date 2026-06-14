/**
 * @file userlib_modbus.c
 * @brief Modbus RTU 从机协议栈。
 *
 * 支持标准 Modbus 功能码：读保持寄存器、写单寄存器、写多寄存器。
 * 含 CRC16 校验、帧超时检测和通信状态管理。
 */

#include "userlib_modbus.h"

/* 采用标准Modbus协议，支持读取和设置单独寄存器、读取和设置多个寄存器功能 */

#define MODBUS_FRAME_MIN_LENGTH 6 // Modbus最小帧长度
#define TX_BUF_SIZE 100           // 串口发送缓冲区大小
#define RX_BUF_SIZE 100           // 串口接收缓冲区大小
#define RX_MAX_LENGTH 64          // 串口接收最大长度
#define RX_MAX_ERROR 100          // 串口接收最大错误次数

/// @brief Modbus配置结构体
typedef struct
{
    UART_Instance channel;  // 串口通道
    uint8_t local_id;       // 本地ID
    uint16_t rx_timeout;    // 串口接收超时时间（单位：ms）
    uint16_t *local_pRegs;  // 指向存储寄存器区域的指针
    uint8_t *local_pStatus; // 通信错误数据指针
    bool userSysTick;       // 是否使用SysTick定时器
} Modbus_Config_StructTypeDef;

uint8_t tx_buff[TX_BUF_SIZE] = {0}; // 串口发送缓冲区
uint16_t tx_bytes = 0;              // 串口要发送的字节数
bool tx_busy_flag = false;          // 串口发送忙标志
bool tx_request_flag = false;       // 串口发送请求标志

uint8_t rx_buff[RX_BUF_SIZE] = {0};   // 串口接收缓冲区
bool rx_new_frame_flag = false;       // 串口接收到新帧标志
uint16_t rx_bytes = 0;                // 串口接收完成的字节数
uint16_t rx_bytes_old = 0;            // 前一时刻串口接收的字节数
bool rx_timeout_flag = false;         // 串口接收超时标志
uint16_t rx_await_time_countdown = 0; // 串口接收超时倒计时
uint16_t rx_no_frame_count = 0;       // 串口接收帧超时次数
uint16_t rx_max_await = 0;            // 串口接收最大等待时间,单位ms
bool rx_request_flag = false;         // 串口接收请求标志

uint8_t local_id = 0;          // 本地ID
uint16_t *local_pRegs = NULL;  // 指向存储寄存器区域的指针
uint8_t *local_pStatus = NULL; // 通信错误数据指针
UART_Instance channel_bck = 0; // 串口通道备份

static const uint8_t aucCRCHi[256]; // CRC高位表
static const uint8_t aucCRCLo[256]; // CRC低位表

static uint16_t _CRC16(uint8_t *pucFrame, uint16_t usLen);
Modbus_Status_EnumTypeDef USER_Modbus_RxFramePreCheck(uint8_t *rx_data, uint16_t rxbytes);
void USER_Modbus_ReadRegs(uint8_t *rx_data);
void USER_Modbus_WriteSingleReg(uint8_t *rx_data);
void USER_Modbus_WriteRegs(uint8_t *rx_data);
void USER_Modbus_ReadWriteRegs(uint8_t *rx_data);
void USER_Modbus_TxFrameFinish_Callback(void);
void USER_Modbus_RxFrameFinish_Callback(void);

/// @brief Modbus通信处理循环
/// @note 该循环函数需要定时调用，建议在1ms定时器中断或者1ms任务中调用
/// @param 无
void USER_Modbus_Comm_Routine()
{
    Modbus_Status_EnumTypeDef status = MODBUS_STA_OK; // 初始化状态为无错误

    /**********************************处理串口接收流程************************************/

    //--------------------如果串口接收未完成但是还未超时---------------------
    if (rx_await_time_countdown > 0)
    {
        // 读取当前串口接收字节数
        rx_bytes = USER_UART_GetReceivedBytes_DMA(channel_bck);
        if (rx_bytes_old != rx_bytes)
        {
            // 如果接收字节数变化，重置接收超时倒计时
            rx_await_time_countdown = rx_max_await;
            rx_bytes_old = rx_bytes;
        }
        else
        {
            // 如果接收字节数未变化，倒计时-1
            rx_await_time_countdown--;
        }
    }
    else
    {
        // 如果倒计时已经结束，串口接收超时标志置位
        rx_timeout_flag = true;
    }

    //--------------------------如果串口接收超时----------------------------
    if (rx_timeout_flag)
    {
        // 停止串口接收
        USER_UART_Abort_Receive(channel_bck);
        // 记录接收的字节数
        rx_bytes = USER_UART_GetReceivedBytes_DMA(channel_bck);
        // 如果接收的字节数大于0
        if (rx_bytes > 0)
        {
            // 串口接收完成标志置位
            rx_new_frame_flag = true;
        }
        // 如果接收的字节数为0
        else
        {
            // 如果超过最大错误次数，设置状态为超时
            if (rx_no_frame_count > RX_MAX_ERROR)
            {
                *local_pStatus = MODBUS_STA_RX_TIMEOUT;
            }
            else
            {
                // 否则增加接收超时次数
                rx_no_frame_count++;
            }
            // 置位新的接收请求标志
            rx_request_flag = true;
        }

        // 串口接收超时标志清零
        rx_timeout_flag = false;
    }

    // --------------------------如果串口接收完成---------------------------
    if (rx_new_frame_flag)
    {
        // 调用接收帧预检查函数
        status = USER_Modbus_RxFramePreCheck(rx_buff, rx_bytes);
        // 如果接收帧预检查未通过
        if (status != MODBUS_STA_OK)
        {
            // 设置状态为错误类型
            *local_pStatus = status;
        }
        // 如果接收帧预检查通过
        else
        {
            // 串口接收错误次数清零
            rx_no_frame_count = 0;

            // 处理接收帧
            switch (rx_buff[1])
            {
            case MODBUS_FC_READ_HOLDING_REGISTERS:
                USER_Modbus_ReadRegs(rx_buff);
                break;
            case MODBUS_FC_WRITE_SINGLE_REGISTER:
                USER_Modbus_WriteSingleReg(rx_buff);
                break;
            case MODBUS_FC_WRITE_HOLDING_REGISTERS:
                USER_Modbus_WriteRegs(rx_buff);
                break;
            case MODBUS_FC_READ_WRITE_HOLDING_REGISTERS:
                USER_Modbus_ReadWriteRegs(rx_buff);
                break;
            default:
                *local_pStatus = MODBUS_STA_RX_FUNC_NOT_EXIST; // 功能码不存在
                break;
            }
        }

        // 置位新的接收请求标志
        rx_request_flag = true;
    }

    /********************************处理串口接收重启流程**********************************/
    if (rx_request_flag)
    {
        // 串口接收完成标志清零
        rx_new_frame_flag = false;

        // 串口接收超时倒计时重置
        rx_await_time_countdown = rx_max_await;

        // 串口接收字节数清零
        rx_bytes_old = 0;
        rx_bytes = 0;

        // 重新初始化串口接收
        USER_UART_Receive_DMA(channel_bck, rx_buff, RX_BUF_SIZE);

        // 清除接收请求标志
        rx_request_flag = false;
    }

    /**********************************处理串口发送流程************************************/
    // 如果串口发送请求标志置位
    if (tx_request_flag)
    {
        // 如果串口发送不忙
        if (tx_busy_flag != true)
        {
            // 发送数据
            USER_UART_Transmit_DMA(channel_bck, tx_buff, tx_bytes);
            // 清除发送请求标志
            tx_request_flag = false;
            // 串口发送忙标志置位
            tx_busy_flag = true;
        }
        // 如果串口发送忙
        else
        {
            // 设置状态为发送忙
            *local_pStatus = MODBUS_STA_TX_BUSY;
        }
    }
}

/**
 * @brief Modbus发送完成回调函数
 * @note 无
 * @param 无
 * @retval 无
 */
void USER_Modbus_TxFrameFinish_Callback(void)
{
    // 串口发送忙标志清零
    tx_busy_flag = false;
    // 串口要发送字节数清零
    tx_bytes = 0;
}

/**
 * @brief Modbus接收完成回调函数
 * @note 无
 * @param 无
 * @retval 无
 */
void USER_Modbus_RxFrameFinish_Callback(void)
{
    // 停止接收
    USER_UART_Abort_Receive(channel_bck);
    // 清除串口接收完成标志
    rx_new_frame_flag = false;
    // 串口接收完成的字节数清零
    rx_bytes_old = 0;
    rx_bytes = 0;
    // 串口接收超时倒计时重置
    rx_await_time_countdown = 0;
    // 串口接收超时标志清零
    rx_timeout_flag = false;
    // 串口状态改为接收溢出错误
    *local_pStatus = MODBUS_STA_RX_OVERFLOW;
    // 重新初始化串口接收
    rx_request_flag = true;
}

/**
 * @brief CRC16校验
 * @note 无
 * @param pucFrame: 数据指针
 * @param usLen: 数据长度
 * @retval 校验结果
 */
static uint16_t _CRC16(uint8_t *pucFrame, uint16_t usLen)
{
    uint8_t ucCRCHi = 0xFF;
    uint8_t ucCRCLo = 0xFF;
    int iIndex;

    while (usLen--)
    {
        iIndex = ucCRCLo ^ *(pucFrame++);
        ucCRCLo = (uint8_t)(ucCRCHi ^ aucCRCHi[iIndex]);
        ucCRCHi = aucCRCLo[iIndex];
    }
    return (uint16_t)(ucCRCHi << 8 | ucCRCLo);
}

/**
 * @brief 串口接收数据预检查
 * @note 无
 * @param rx_data: 接收数据指针
 * @param rxbytes: 接收字节数
 * @retval 帧错误类型
 */
Modbus_Status_EnumTypeDef USER_Modbus_RxFramePreCheck(uint8_t *rx_data, uint16_t rxbytes)
{
    uint16_t check_result = 0; // 校验计算结果
    uint16_t rec_check = 0;    // 接收校验

    // 如果接收字节数小于最小帧长度
    if (rxbytes < MODBUS_FRAME_MIN_LENGTH)
    {
        return MODBUS_STA_RX_LENGTH_ERROR; // 返回帧长度错误
    }

    // 如果接收字节数大于最大帧长度
    if (rxbytes > RX_MAX_LENGTH)
    {
        return MODBUS_STA_RX_OVERFLOW; // 返回接收缓冲区溢出
    }

    // 校验从机ID
    if (rx_data[0] != local_id)
    {
        return MODBUS_STA_RX_ID_MISMATCH; // 返回从机ID不匹配
    }

    // 校验功能码
    switch (rx_data[1])
    {
    case MODBUS_FC_READ_HOLDING_REGISTERS:       // 读多个保持寄存器
    case MODBUS_FC_WRITE_SINGLE_REGISTER:        // 写单个保持寄存器
    case MODBUS_FC_WRITE_HOLDING_REGISTERS:      // 写多个保持寄存器
    case MODBUS_FC_READ_WRITE_HOLDING_REGISTERS: // 读写多个保持寄存器
    {
        break;
    }
    default:
        return MODBUS_STA_RX_FUNC_NOT_EXIST;
    }

    // 校验 (Modbus CRC使用小端序)
    rec_check = ((uint16_t)rx_data[rxbytes - 1] << 8) | rx_data[rxbytes - 2];
    check_result = _CRC16(rx_data, rxbytes - 2);
    if (check_result != rec_check)
        return MODBUS_STA_RX_CHECK_ERROR; // 返回校验错误

    // 返回帧正常
    return MODBUS_STA_OK;
}

/**
 * @brief Modbus读多个寄存器
 * @note 无
 * @param rx_data: 接收数据指针
 * @retval 无
 */
void USER_Modbus_ReadRegs(uint8_t *rx_data)
{
    uint16_t read_addr = 0;    /* 读取地址 */
    uint16_t read_regs = 0;    /* 读取寄存器数量 */
    uint16_t read_bytes = 0;   /* 读取字节数 */
    uint16_t check_result = 0; /* 校验计算结果 */

    /* 读取地址 (Modbus使用大端序) */
    read_addr = ((uint16_t)rx_data[2] << 8) | rx_data[3];

    /* 读取寄存器数量 (Modbus使用大端序) */
    read_regs = ((uint16_t)rx_data[4] << 8) | rx_data[5];
    read_bytes = read_regs << 1; /* 转换为字节数 */

    /* 边界检查：防止缓冲区溢出 */
    if (read_bytes > (TX_BUF_SIZE - 5))
    {
        *local_pStatus = MODBUS_STA_RX_REGISTER_OUT_OF_RANGE;
        return;
    }

    /* 设置帧头 */
    tx_buff[0] = local_id;                         /* 从机ID */
    tx_buff[1] = MODBUS_FC_READ_HOLDING_REGISTERS; /* 功能码 */
    tx_buff[2] = (uint8_t)read_bytes;              /* 数据字节数 */

    /* 从存储区读取数据*/
    // 注意：Modbus使用大端序，所以需要转换字节序
    for (uint16_t i = 0; i < read_bytes; i += 2) // 每次读取2个字节
    {
        tx_buff[3 + i] = (uint8_t)(local_pRegs[read_addr + (i >> 1)] >> 8);   // 高字节
        tx_buff[4 + i] = (uint8_t)(local_pRegs[read_addr + (i >> 1)] & 0xFF); // 低字节
    }

    /* 设置校验 (Modbus CRC使用小端序：低位在前，高位在后) */
    check_result = _CRC16(&tx_buff[0], 3 + read_bytes);
    tx_buff[3 + read_bytes] = (uint8_t)check_result;        /* CRC低位 */
    tx_buff[4 + read_bytes] = (uint8_t)(check_result >> 8); /* CRC高位 */

    /* 设置发送字节数 */
    tx_bytes = 5 + read_bytes;

    /* 请求发送 */
    tx_request_flag = true;
}

/**
 * @brief Modbus写单个寄存器
 * @note 无
 * @param rx_data: 接收数据指针
 * @retval 无
 */
void USER_Modbus_WriteSingleReg(uint8_t *rx_data)
{
    uint16_t write_addr = 0;   /* 写入地址 */
    uint16_t write_value = 0;  /* 写入寄存器值 */
    uint16_t check_result = 0; /* 校验计算结果 */

    /* 写入地址 (Modbus使用大端序) */
    write_addr = ((uint16_t)rx_data[2] << 8) | rx_data[3];

    /* 写入寄存器值 (Modbus使用大端序) */
    write_value = ((uint16_t)rx_data[4] << 8) | rx_data[5];

    /* 边界检查：防止写入越界 */
    if (write_addr >= RX_BUF_SIZE / 2)
    {
        *local_pStatus = MODBUS_STA_RX_REGISTER_OUT_OF_RANGE;
        return;
    }

    /* 写入寄存器 */
    local_pRegs[write_addr] = write_value;

    /* 设置帧头 */
    tx_buff[0] = local_id;                        /* 从机ID */
    tx_buff[1] = MODBUS_FC_WRITE_SINGLE_REGISTER; /* 功能码 */
    tx_buff[2] = rx_data[2];                      /* 写入地址高字节 */
    tx_buff[3] = rx_data[3];                      /* 写入地址低字节 */
    tx_buff[4] = rx_data[4];                      /* 寄存器值高字节 */
    tx_buff[5] = rx_data[5];                      /* 寄存器值低字节 */

    /* 设置校验 (Modbus CRC使用小端序：低位在前，高位在后) */
    check_result = _CRC16(tx_buff, 6);
    tx_buff[6] = (uint8_t)check_result;        /* CRC低位 */
    tx_buff[7] = (uint8_t)(check_result >> 8); /* CRC高位 */

    /* 设置发送字节数 */
    tx_bytes = 8;

    /* 设置发送请求标志 */
    tx_request_flag = true;
}

/**
 * @brief Modbus写多个寄存器
 * @note 无
 * @param rx_data: 接收数据指针
 * @retval 无
 */
void USER_Modbus_WriteRegs(uint8_t *rx_data)
{
    uint16_t write_addr = 0;   /* 写入地址 */
    uint16_t write_regs = 0;   /* 写入寄存器数量 */
    uint16_t write_bytes = 0;  /* 写入字节数 */
    uint16_t check_result = 0; /* 校验计算结果 */

    /* 写入地址 (Modbus使用大端序) */
    write_addr = ((uint16_t)rx_data[2] << 8) | rx_data[3];

    /* 写入寄存器数量 (Modbus使用大端序) */
    write_regs = ((uint16_t)rx_data[4] << 8) | rx_data[5];
    write_bytes = rx_data[6]; /* 写入字节数 */

    /* 边界检查：防止写入越界 */
    if (write_bytes > (RX_BUF_SIZE - 7) || write_regs != (write_bytes >> 1))
    {
        *local_pStatus = MODBUS_STA_RX_REGISTER_OUT_OF_RANGE;
        return;
    }

    /* 从接收数据中读取写入数据*/
    memcpy(&local_pRegs[write_addr], &rx_data[7], write_bytes);

    /* 设置帧头 */
    tx_buff[0] = local_id;                          /* 从机ID */
    tx_buff[1] = MODBUS_FC_WRITE_HOLDING_REGISTERS; /* 功能码 */
    tx_buff[2] = rx_data[2];                        /* 写入地址高字节 */
    tx_buff[3] = rx_data[3];                        /* 写入地址低字节 */
    tx_buff[4] = rx_data[4];                        /* 写入数量高字节 */
    tx_buff[5] = rx_data[5];                        /* 写入数量低字节 */

    /* 设置校验 (Modbus CRC使用小端序：低位在前，高位在后) */
    check_result = _CRC16(tx_buff, 6);
    tx_buff[6] = (uint8_t)check_result;        /* CRC低位 */
    tx_buff[7] = (uint8_t)(check_result >> 8); /* CRC高位 */

    /* 设置发送字节数 */
    tx_bytes = 8;

    /* 设置发送请求标志 */
    tx_request_flag = true;
}

/**
 * @brief Modbus读写多个寄存器
 * @note 无
 * @param rx_data: 接收数据指针
 * @retval 无
 */
void USER_Modbus_ReadWriteRegs(uint8_t *rx_data)
{
    uint16_t write_addr = 0;   /* 写入地址 */
    uint16_t write_regs = 0;   /* 写入寄存器数量 */
    uint16_t write_bytes = 0;  /* 写入字节数 */
    uint16_t read_addr = 0;    /* 读取地址 */
    uint16_t read_regs = 0;    /* 读取寄存器数量 */
    uint16_t read_bytes = 0;   /* 读取字节数 */
    uint16_t check_result = 0; /* 校验计算结果 */

    /* 读取地址 (Modbus使用大端序) */
    read_addr = ((uint16_t)rx_data[2] << 8) | rx_data[3];

    /* 读取寄存器数量 (Modbus使用大端序) */
    read_regs = ((uint16_t)rx_data[4] << 8) | rx_data[5];
    read_bytes = read_regs << 1; /* 转换为字节数 */

    /* 写入地址 (Modbus使用大端序) */
    write_addr = ((uint16_t)rx_data[6] << 8) | rx_data[7];

    /* 写入寄存器数量 (Modbus使用大端序) */
    write_regs = ((uint16_t)rx_data[8] << 8) | rx_data[9];
    write_bytes = rx_data[10]; /* 写入字节数 */

    /* 边界检查：防止缓冲区溢出 */
    if (read_bytes > (TX_BUF_SIZE - 5) ||
        write_bytes > (RX_BUF_SIZE - 11) ||
        write_regs != (write_bytes >> 1))
    {
        *local_pStatus = MODBUS_STA_RX_REGISTER_OUT_OF_RANGE;
        return;
    }

    /* 从接收数据中读取写入数据*/
    memcpy(&local_pRegs[write_addr], &rx_data[11], write_bytes);

    /* 设置帧头 */
    tx_buff[0] = local_id;                               /* 从机ID */
    tx_buff[1] = MODBUS_FC_READ_WRITE_HOLDING_REGISTERS; /* 功能码 */
    tx_buff[2] = (uint8_t)read_bytes;                    /* 读取字节数 */

    /* 从存储区读取数据*/
    // 注意：Modbus使用大端序，所以需要转换字节序
    for (uint16_t i = 0; i < read_bytes; i += 2) // 每次读取2个字节
    {
        tx_buff[3 + i] = (uint8_t)(local_pRegs[read_addr + (i >> 1)] >> 8);   // 高字节
        tx_buff[4 + i] = (uint8_t)(local_pRegs[read_addr + (i >> 1)] & 0xFF); // 低字节
    }

    /* 设置校验 (Modbus CRC使用小端序：低位在前，高位在后) */
    check_result = _CRC16(&tx_buff[0], 3 + read_bytes);
    tx_buff[3 + read_bytes] = (uint8_t)check_result;        /* CRC低位 */
    tx_buff[4 + read_bytes] = (uint8_t)(check_result >> 8); /* CRC高位 */

    /* 设置发送字节数 */
    tx_bytes = 5 + read_bytes;

    /* 设置发送请求标志 */
    tx_request_flag = true;
}

/**
 * @brief Modbus接收帧处理
 * @note 无
 * @param rx_data: 接收数据指针
 * @retval 是否处理成功
 */
void USER_Modbus_RxFrameProcess(uint8_t *rx_data)
{
    switch (rx_data[1])
    {
    case MODBUS_FC_READ_HOLDING_REGISTERS: // 读多个保持寄存器
        USER_Modbus_ReadRegs(rx_data);
        break;
    case MODBUS_FC_WRITE_HOLDING_REGISTERS: // 写多个保持寄存器
        USER_Modbus_WriteRegs(rx_data);
        break;
    case MODBUS_FC_READ_WRITE_HOLDING_REGISTERS: // 读写多个保持寄存器
        USER_Modbus_ReadWriteRegs(rx_data);
        break;
    default:
        break;
    }
}

/**
 * @brief Modbus通信初始化
 * @note 无
 * @param config: 串口配置结构体
 * @retval 无
 */
void _modbus_slave_init(Modbus_Config_StructTypeDef *config)
{
    // 串口接收帧超时次数
    rx_no_frame_count = 0;

    // 串口通道备份
    channel_bck = config->channel;

    // 记录本地ID
    local_id = config->local_id;

    // 记录指向可写寄存器的指针
    local_pRegs = config->local_pRegs;

    // 记录通信状态指针
    local_pStatus = config->local_pStatus;

    // 记录串口接收最大等待时间
    rx_max_await = config->rx_timeout;

    // 注册系统滴答定时器
    if (config->userSysTick)
    {
        // 注册循环通信处理函数
        USER_SysTick_RegisterCallback(USER_Modbus_Comm_Routine);
    }

    // 注册串口发送完成回调函数
    USER_UART_RegisterCallback(channel_bck, UART_INTERRUPT_DMA_DONE_TX, USER_Modbus_TxFrameFinish_Callback);

    // 注册串口接收完成回调函数
    USER_UART_RegisterCallback(channel_bck, UART_INTERRUPT_DMA_DONE_RX, USER_Modbus_RxFrameFinish_Callback);

    // 启动串口
    USER_UART_Init(channel_bck);

    // 启动串口接收超时倒计时
    rx_await_time_countdown = rx_max_await;

    // 使能串口中断接收
    USER_UART_Receive_DMA(channel_bck, &rx_buff[0], RX_BUF_SIZE);
}

/**
 * @brief Modbus从机初始化
 * @note 无
 * @param useSysTick: 是否使用SysTick定时器
 * @param pStorage: 数据的存储区指针
 * @param pStatus: 通信错误数据指针
 * @retval 无
 */
void USER_Modbus_Slave_Init(bool useSysTick, UART_Instance uart_channel, uint16_t *pStorage, uint8_t *pStatus)
{
    Modbus_Config_StructTypeDef config;

    // 设置串口配置
    config.channel = uart_channel;   // 使用指定的UART通道
    config.local_id = 1;             // 本地ID为1
    config.rx_timeout = 1;           // 串口接收超时时间1ms
    config.userSysTick = useSysTick; // 是否使用SysTick定时器
    config.local_pRegs = pStorage;   // 指向存储寄存器区域的指针
    config.local_pStatus = pStatus;  // 通信错误数据指针
    // 调用Modbus通信初始化函数
    _modbus_slave_init(&config);
}

// CRC高位表
static const uint8_t aucCRCHi[256] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40};

// CRC低位表
static const uint8_t aucCRCLo[256] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7,
    0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E,
    0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9,
    0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
    0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32,
    0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D,
    0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
    0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF,
    0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1,
    0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
    0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB,
    0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
    0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
    0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97,
    0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E,
    0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89,
    0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83,
    0x41, 0x81, 0x80, 0x40};
