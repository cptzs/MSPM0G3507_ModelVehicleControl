/**
 * @file userlib_uart.c
 * @brief MSPM0 UART + DMA 串口通信驱动。
 *
 * 支持 4 路 UART 的 DMA 收发、中断回调注册、超时检测和接收字节数查询。
 */

#include "userlib_uart.h"

/* DMA 通道号需根据 SysConfig 实际 DMA 配置调整，以下为默认占位值 */
/* UART0 DMA */
#ifndef DMA_CH_UART0_TX_CHAN_ID
#define DMA_CH_UART0_TX_CHAN_ID 0 // UART0 TX DMA通道号
#endif
#ifndef DMA_CH_UART0_RX_CHAN_ID
#define DMA_CH_UART0_RX_CHAN_ID 1 // UART0 RX DMA通道号
#endif

// 适配UART1 DMA通道号
#ifndef DMA_CH_UART1_TX_CHAN_ID
#define DMA_CH_UART1_TX_CHAN_ID 0 // UART1 TX DMA通道号
#endif
#ifndef DMA_CH_UART1_RX_CHAN_ID
#define DMA_CH_UART1_RX_CHAN_ID 1 // UART1 RX DMA通道号
#endif

// 适配UART2 DMA通道号
#ifndef DMA_CH_UART2_TX_CHAN_ID
#define DMA_CH_UART2_TX_CHAN_ID 0 // UART2 TX DMA通道号
#endif
#ifndef DMA_CH_UART2_RX_CHAN_ID
#define DMA_CH_UART2_RX_CHAN_ID 1 // UART2 RX DMA通道号
#endif

// 适配UART3 DMA通道号
#ifndef DMA_CH_UART3_TX_CHAN_ID
#define DMA_CH_UART3_TX_CHAN_ID 0 // UART3 TX DMA通道号
#endif
#ifndef DMA_CH_UART3_RX_CHAN_ID
#define DMA_CH_UART3_RX_CHAN_ID 1 // UART3 RX DMA通道号
#endif

/// @brief UART中断处理函数指针数组
void (*uart_interrupt_handler[4][4])(void) = {0};

/// @brief UART寄存器结构体指针数组
UART_Regs *uart_regs_real[4] = {
    (UART_Regs *)UART0_BASE, // 使用基地址
    (UART_Regs *)UART1_BASE,
    (UART_Regs *)UART2_BASE,
    (UART_Regs *)UART3_BASE};

/// @brief UART中断号数组
uint32_t const uart_irq[4] = {
    UART0_INT_IRQn, // UART0中断号
    UART1_INT_IRQn, // UART1中断号
    UART2_INT_IRQn, // UART2中断号
    UART3_INT_IRQn  // UART3中断号
};

bool uart_enabled[4] = {false}; // 每个UART实例是否启用

uint8_t uart_tx_method[4] = {0}; // 每个UART实例的发送方法
uint8_t uart_rx_method[4] = {0}; // 每个UART实例的接收方法

bool uart_tx_in_progress[4] = {false}; // 每个UART实例的发送是否正在进行
uint16_t bytes_to_send[4] = {0};       // 每个UART实例的待发送字节数
uint16_t bytes_sent[4] = {0};          // 每个UART实例已发送的字节数
uint16_t bytes_sent_last[4] = {0};     // 每个UART实例上次发送的字节数
uint8_t *uart_tx_scr[4] = {0};         // 每个UART实例的待发送数据指针

bool uart_rx_in_progress[4] = {false}; // 每个UART实例的接收是否正在进行
uint16_t bytes_to_receive[4] = {0};    // 每个UART实例的待接收字节数
uint16_t bytes_received[4] = {0};      // 每个UART实例已接收的字节数
uint16_t bytes_received_last[4] = {0}; // 每个UART实例上次接收的字节数
uint8_t *uart_rx_dst[4] = {0};         // 每个UART实例的待接收数据指针

uint32_t uart_txdma_ch[4] = {0}; // 每个UART实例的TX_DMA通道号
uint32_t uart_rxdma_ch[4] = {0}; // 每个UART实例的RX_DMA通道号

/**
 * @brief 注册 UART 中断回调函数。
 * @param uart_inst UART 实例编号。
 * @param interrupt_type 中断类型 (TX_DONE / RX_DONE 等)。
 * @param handler 中断回调函数指针。
 */
void USER_UART_RegisterCallback(UART_Instance uart_inst, UART_Interrupt interrupt_type, void (*handler)(void))
{
    uart_interrupt_handler[uart_inst][interrupt_type] = handler;
}

/**
 * @brief 注销 UART 中断回调函数。
 * @param uart_inst UART 实例编号。
 * @param interrupt_type 中断类型。
 */
void USER_UART_UnregisterCallback(UART_Instance uart_inst, UART_Interrupt interrupt_type)
{
    uart_interrupt_handler[uart_inst][interrupt_type] = 0;
}

/**
 * @brief 初始化指定 UART 实例。
 * @details 记录 DMA 通道号、启用 UART 全局中断、标记实例为已启用。
 * @param uart_inst UART 实例编号。
 */
void USER_UART_Init(UART_Instance uart_inst)
{
    // 记录DMA通道号
    switch (uart_inst)
    {
    case UART_0:
        uart_txdma_ch[uart_inst] = DMA_CH_UART0_TX_CHAN_ID;
        uart_rxdma_ch[uart_inst] = DMA_CH_UART0_RX_CHAN_ID;
        break;
    case UART_1:
        uart_txdma_ch[uart_inst] = DMA_CH_UART1_TX_CHAN_ID;
        uart_rxdma_ch[uart_inst] = DMA_CH_UART1_RX_CHAN_ID;
        break;
    case UART_2:
        uart_txdma_ch[uart_inst] = DMA_CH_UART2_TX_CHAN_ID;
        uart_rxdma_ch[uart_inst] = DMA_CH_UART2_RX_CHAN_ID;
        break;
    case UART_3:
        uart_txdma_ch[uart_inst] = DMA_CH_UART3_TX_CHAN_ID;
        uart_rxdma_ch[uart_inst] = DMA_CH_UART3_RX_CHAN_ID;
        break;
    default:
        break;
    }

    // 启用UART总中断
    NVIC_EnableIRQ(uart_irq[uart_inst]);

    // 标记当前UART实例已启用
    uart_enabled[uart_inst] = true;
}

/// @brief 停止UART实例工作
void USER_UART_Deinit(UART_Instance uart_inst)
{
    // 禁用UART总中断
    NVIC_DisableIRQ(uart_irq[uart_inst]);

    // 重置状态和计数器
    uart_tx_in_progress[uart_inst] = false;
    uart_rx_in_progress[uart_inst] = false;
    bytes_to_send[uart_inst] = 0;
    bytes_sent[uart_inst] = 0;
    bytes_to_receive[uart_inst] = 0;
    bytes_received[uart_inst] = 0;

    // 标记UART实例未启用
    uart_enabled[uart_inst] = false;
}

/// @brief 使用堵塞方式发送数据到UART
/// @param uart_inst UART实例
/// @param tx_data 待发送数据指针
/// @param length 待发送数据长度
/// @param timeout_ms 超时时间（毫秒），如果为0则表示不启用超时
/// @return
bool USER_UART_Transmit(UART_Instance uart_inst, uint8_t *tx_data, uint16_t length, uint32_t timeout_ms)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];
    uint32_t uart_tx_timeout_moment = 0;  // 发送超时计时点
    bool uart_tx_timeout_enabled = false; // 是否启用发送超时

    // 如果当前正在发送数据，则返回失败
    if (uart_tx_in_progress[uart_inst])
    {
        return false;
    }
    // 如果长度为0，则不发送数据
    if (length == 0)
    {
        return false;
    }
    // 设置发送状态为进行中
    uart_tx_in_progress[uart_inst] = true;
    // 设置发送方式为堵塞
    uart_tx_method[uart_inst] = UART_METHOD_POLLING;
    // 取消UART发送中断使能
    DL_UART_disableInterrupt(uart, DL_UART_MAIN_INTERRUPT_TX);
    // 设置要发送的数据指针
    uart_tx_scr[uart_inst] = tx_data;
    // 设置待发送字节数
    bytes_to_send[uart_inst] = length;
    // 重置已发送字节数
    bytes_sent[uart_inst] = 0;
    // 如果启用发送超时
    if (timeout_ms > 0)
    {
        // 启用发送超时
        uart_tx_timeout_enabled = true;
        // 设置发送超时时间
        uart_tx_timeout_moment = sysTick + timeout_ms;
    }
    else
    {
        // 禁用发送超时
        uart_tx_timeout_enabled = false;
    }

    // 如果已发送字节数小于待发送字节数
    while (bytes_sent[uart_inst] < bytes_to_send[uart_inst])
    {
        // 如果TX FIFO已满，等待空闲
        do
        {
            // 如果启用发送超时，且当前时间超过超时时间
            if (uart_tx_timeout_enabled && sysTick > uart_tx_timeout_moment)
            {
                // 重置发送状态
                uart_tx_in_progress[uart_inst] = false;
                // 重置发送方法
                uart_tx_method[uart_inst] = UART_METHOD_NONE;
                // 返回发送失败
                return false;
            }
        } while (DL_UART_isTXFIFOFull(uart) == true);
        // 发送数据到UART
        DL_UART_transmitData(uart, *uart_tx_scr[uart_inst]);
        // 更新已发送字节数
        bytes_sent[uart_inst]++;
        // 更新发送数据指针
        uart_tx_scr[uart_inst]++;
    }
    // 发送完成，重置发送状态
    uart_tx_in_progress[uart_inst] = false;
    // 重置发送方法
    uart_tx_method[uart_inst] = UART_METHOD_NONE;
    // 返回发送成功
    return true;
}

/// @brief 使用中断方式发送数据到UART
/// @param uart_inst UART实例
/// @param tx_data 待发送数据指针
/// @param length 待发送数据长度
/// @return true: 发送启动成功, false: 发送忙或参数无效
bool USER_UART_Transmit_IT(UART_Instance uart_inst, uint8_t *tx_data, uint16_t length)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];

    // 如果当前正在发送数据，则返回失败
    if (uart_tx_in_progress[uart_inst])
    {
        return false;
    }
    // 如果长度为0，则不发送数据
    if (length == 0)
    {
        return false;
    }
    // 如果TX FIFO已满
    if (DL_UART_isBusy(uart) == true)
    {
        // 返回失败
        return false;
    }
    // 设置发送状态为进行中
    uart_tx_in_progress[uart_inst] = true;
    // 设置发送方式为中断
    uart_tx_method[uart_inst] = UART_METHOD_INTERRUPT;
    // 设置要发送的数据指针
    uart_tx_scr[uart_inst] = tx_data;
    // 设置待发送字节数
    bytes_to_send[uart_inst] = length;
    // 重置已发送字节数
    bytes_sent[uart_inst] = 0;
    // 启用UART发送中断使能
    DL_UART_enableInterrupt(uart, DL_UART_MAIN_INTERRUPT_TX);
    // 发送数据到UART
    DL_UART_transmitData(uart, *uart_tx_scr[uart_inst]);
    // 返回成功
    return true;
}

/// @brief 使用DMA发送数据到UART
/// @param uart_inst UART实例
/// @param tx_data 待发送数据指针
/// @param length 待发送数据长度
/// @return 发送是否成功
bool USER_UART_Transmit_DMA(UART_Instance uart_inst, uint8_t *tx_data, uint16_t length)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];
    // 如果当前正在发送数据，则返回失败
    if (uart_tx_in_progress[uart_inst])
    {
        return false;
    }
    // 如果长度为0，则不发送数据
    if (length == 0)
    {
        return false;
    }
    // 设置发送状态为进行中
    uart_tx_in_progress[uart_inst] = true;
    // 设置发送方式为DMA
    uart_tx_method[uart_inst] = UART_METHOD_DMA;
    // 设置要发送的数据指针
    uart_tx_scr[uart_inst] = tx_data;
    // 设置待发送字节数
    bytes_to_send[uart_inst] = length;
    // 重置已发送字节数
    bytes_sent[uart_inst] = 0;
    // 禁用UART发送中断
    DL_UART_disableInterrupt(uart, DL_UART_MAIN_INTERRUPT_TX);
    // 启用DMA发送完成中断
    DL_UART_enableInterrupt(uart, DL_UART_MAIN_INTERRUPT_DMA_DONE_TX);
    // 设置DMA源地址和目的地址
    DL_DMA_setSrcAddr(DMA, uart_txdma_ch[uart_inst], (uint32_t)tx_data);
    DL_DMA_setDestAddr(DMA, uart_txdma_ch[uart_inst], (uint32_t)(&uart->TXDATA));
    // 设置DMA传输大小
    DL_DMA_setTransferSize(DMA, uart_txdma_ch[uart_inst], length);
    // 启用DMA通道
    DL_DMA_enableChannel(DMA, uart_txdma_ch[uart_inst]);
    return true;
}

/// @brief 中止UART发送
/// @param uart_inst UART实例
/// @return 是否成功
bool USER_UART_Abort_Transmit(UART_Instance uart_inst)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];

    // 如果当前正在发送数据
    if (uart_tx_in_progress[uart_inst])
    {
        // 禁用DMA通道
        DL_DMA_disableChannel(DMA, uart_txdma_ch[uart_inst]);
        // 禁用UART发送中断
        DL_UART_disableInterrupt(uart, DL_UART_INTERRUPT_TX);
        // 禁用DMA发送完成中断
        DL_UART_disableInterrupt(uart, DL_UART_IIDX_DMA_DONE_TX);
        // 清除发送中断标志位
        DL_UART_clearInterruptStatus(uart, DL_UART_INTERRUPT_TX);
        // 清除DMA发送完成中断标志位
        DL_UART_clearInterruptStatus(uart, DL_UART_IIDX_DMA_DONE_TX);

        // 重置发送状态
        uart_tx_in_progress[uart_inst] = false;
        // 重置发送方法
        uart_tx_method[uart_inst] = UART_METHOD_NONE;
        return true;
    }
    return false;
}

/// @brief 使用堵塞方式从UART接收数据
/// @param uart_inst UART实例
/// @param rx_data 接收数据缓冲区指针
/// @param length 待接收数据长度
/// @param timeout_ms 超时时间（毫秒），0表示不启用超时
/// @return true: 接收成功, false: 接收忙或超时
bool USER_UART_Receive(UART_Instance uart_inst, uint8_t *rx_data, uint16_t length, uint32_t timeout_ms)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];
    uint8_t uart_rx_temp[4] = {0};       // 临时缓冲区，用于清除之前接收的FIFO数据
    uint32_t uart_rx_timeout_moment = 0; // 接收超时时刻
    bool uart_enable_rx_timeout = false; // 是否启用接收超时

    // 如果当前正在接收数据，则返回失败
    if (uart_rx_in_progress[uart_inst])
    {
        return false;
    }
    // 如果长度为0，则不接收数据
    if (length == 0)
    {
        return false;
    }
    // 设置接收状态为进行中
    uart_rx_in_progress[uart_inst] = true;
    // 设置接收方式为堵塞
    uart_rx_method[uart_inst] = UART_METHOD_POLLING;
    // 取消UART接收中断使能
    DL_UART_disableInterrupt(uart, DL_UART_MAIN_INTERRUPT_RX);
    // 设置要接收的数据指针
    uart_rx_dst[uart_inst] = rx_data;
    // 设置待接收字节数
    bytes_to_receive[uart_inst] = length;
    // 重置已接收字节数
    bytes_received[uart_inst] = 0;
    // 如果接收超时不为0
    if (timeout_ms != 0)
    {
        // 启用接收超时
        uart_enable_rx_timeout = true;
        // 设置超时时间
        uart_rx_timeout_moment = timeout_ms + sysTick;
    }
    // 清除接收FIFO缓冲区现有数据
    DL_UART_drainRXFIFO(uart, uart_rx_temp, 4);

    // 如果已接收字节数小于待接收字节数
    while (bytes_received[uart_inst] < bytes_to_receive[uart_inst])
    {
        // 如果接收FIFO为空，等待数据
        do
        {
            // 如果接收超时计数器达到超时时间
            if (uart_enable_rx_timeout && (sysTick >= uart_rx_timeout_moment))
            {
                // 重置接收状态
                uart_rx_in_progress[uart_inst] = false;
                // 重置接收方法
                uart_rx_method[uart_inst] = UART_METHOD_NONE;
                // 接收超时，返回失败
                return false;
            }
        } while (DL_UART_isRXFIFOEmpty(uart) == true);
        // 从UART接收数据并写入目标缓冲区
        *uart_rx_dst[uart_inst] = DL_UART_receiveData(uart);
        // 更新缓冲区指针
        uart_rx_dst[uart_inst]++;
        // 接收字节数更新
        bytes_received[uart_inst]++;
    }
    // 接收完成，重置接收状态
    uart_rx_in_progress[uart_inst] = false;
    // 重置接收方法
    uart_rx_method[uart_inst] = UART_METHOD_NONE;
    // 返回接收成功
    return true;
}

/// @brief 使用中断方式从UART接收数据
/// @param uart_inst UART实例
/// @param rx_data 接收数据缓冲区指针
/// @param length 待接收数据长度
/// @return true: 接收启动成功, false: 接收忙或参数无效
bool USER_UART_Receive_IT(UART_Instance uart_inst, uint8_t *rx_data, uint16_t length)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];
    uint8_t uart_rx_temp[4] = {0}; // 临时缓冲区，用于清除之前接收的FIFO数据

    // 如果当前正在接收数据，则返回失败
    if (uart_rx_in_progress[uart_inst])
    {
        return false;
    }
    // 如果长度为0，则不接收数据
    if (length == 0)
    {
        return false;
    }
    // 设置接收状态为进行中
    uart_rx_in_progress[uart_inst] = true;
    // 设置接收方式为中断
    uart_rx_method[uart_inst] = UART_METHOD_INTERRUPT;
    // 设置要接收的数据指针
    uart_rx_dst[uart_inst] = rx_data;
    // 设置待接收字节数
    bytes_to_receive[uart_inst] = length;
    // 重置已接收字节数
    bytes_received[uart_inst] = 0;
    // 清除接收FIFO缓冲区现有数据
    DL_UART_drainRXFIFO(uart, uart_rx_temp, 4);
    // 启用UART接收中断使能
    DL_UART_enableInterrupt(uart, DL_UART_MAIN_INTERRUPT_RX);

    return true;
}

/// @brief 使用DMA从UART接收数据
/// @param uart_inst UART实例
/// @param rx_data 接收数据缓冲区指针
/// @param length 待接收数据长度
/// @return true: DMA接收启动成功, false: 接收忙或参数无效
bool USER_UART_Receive_DMA(UART_Instance uart_inst, uint8_t *rx_data, uint16_t length)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];
    uint8_t uart_rx_temp[4] = {0}; // 临时缓冲区，用于清除之前接收的FIFO数据

    // 如果当前正在接收数据，则返回失败
    if (uart_rx_in_progress[uart_inst])
    {
        return false;
    }
    // 如果长度为0，则不接收数据
    if (length == 0)
    {
        return false;
    }
    // 设置接收状态为进行中
    uart_rx_in_progress[uart_inst] = true;
    // 设置接收方式为DMA
    uart_rx_method[uart_inst] = UART_METHOD_DMA;
    // 设置要接收的数据指针
    uart_rx_dst[uart_inst] = rx_data;
    // 设置待接收字节数
    bytes_to_receive[uart_inst] = length;
    // 重置已接收字节数
    bytes_received[uart_inst] = 0;
    // 清除接收FIFO缓冲区现有数据
    DL_UART_drainRXFIFO(uart, uart_rx_temp, 4);
    // 禁用UART接收中断
    DL_UART_disableInterrupt(uart, DL_UART_MAIN_INTERRUPT_RX);
    // 启用DMA接收完成中断
    DL_UART_enableInterrupt(uart, DL_UART_MAIN_INTERRUPT_DMA_DONE_RX);
    // 设置DMA源地址和目的地址
    DL_DMA_setSrcAddr(DMA, uart_rxdma_ch[uart_inst], (uint32_t)(&uart->RXDATA));
    DL_DMA_setDestAddr(DMA, uart_rxdma_ch[uart_inst], (uint32_t)rx_data);
    // 设置DMA传输大小
    DL_DMA_setTransferSize(DMA, uart_rxdma_ch[uart_inst], length);
    // 启用DMA通道
    DL_DMA_enableChannel(DMA, uart_rxdma_ch[uart_inst]);

    return true;
}

/// @brief 中止UART接收
/// @param uart_inst UART实例
/// @return 是否成功
bool USER_UART_Abort_Receive(UART_Instance uart_inst)
{
    UART_Regs *uart;
    uart = uart_regs_real[uart_inst];

    // 如果当前正在接收数据
    if (uart_rx_in_progress[uart_inst])
    {
        // 禁用DMA通道
        DL_DMA_disableChannel(DMA, uart_rxdma_ch[uart_inst]);
        // 禁用UART接收中断
        DL_UART_disableInterrupt(uart, DL_UART_MAIN_INTERRUPT_RX);
        // 禁用DMA接收完成中断
        DL_UART_disableInterrupt(uart, DL_UART_IIDX_DMA_DONE_RX);
        // 清除接收中断标志位
        DL_UART_clearInterruptStatus(uart, DL_UART_MAIN_INTERRUPT_RX);
        // 清除DMA接收完成中断标志位
        DL_UART_clearInterruptStatus(uart, DL_UART_IIDX_DMA_DONE_RX);

        // 重置接收状态
        uart_rx_in_progress[uart_inst] = false;
        // 重置接收方法
        uart_rx_method[uart_inst] = UART_METHOD_NONE;
        return true;
    }
    return false;
}

/// @brief 获取UART实例已发送的字节数
/// @param uart_inst UART实例
uint16_t USER_UART_GetSentBytes(UART_Instance uart_inst)
{
    // 返回已发送的字节数
    return bytes_sent[uart_inst];
}

/// @brief 获取UART实例DMA已发送的字节数
/// @param uart_inst UART实例
uint16_t USER_UART_GetSentBytes_DMA(UART_Instance uart_inst)
{
    // 返回已发送的字节数
    return bytes_to_send[uart_inst] - DL_DMA_getTransferSize(DMA, uart_txdma_ch[uart_inst]);
}

/// @brief 获取UART实例已接收的字节数
/// @param uart_inst UART实例
uint16_t USER_UART_GetReceivedBytes(UART_Instance uart_inst)
{
    // 返回已接收的字节数
    return bytes_received[uart_inst];
}

/// @brief 获取UART实例已DMA接收的字节数
/// @param uart_inst UART实例
uint16_t USER_UART_GetReceivedBytes_DMA(UART_Instance uart_inst)
{
    // 返回已接收的字节数
    return bytes_to_receive[uart_inst] - DL_DMA_getTransferSize(DMA, uart_rxdma_ch[uart_inst]);
}

/// @brief UART0中断服务函数
/// @param 无
void UART0_IRQHandler(void)
{
    // 读取UART0的中断状态
    switch (DL_UART_getPendingInterrupt(UART0))
    {
        // 处理UART接收中断
    case DL_UART_IIDX_RX:
    {
        // 清除接收中断标志位
        DL_UART_clearInterruptStatus(UART0, DL_UART_IIDX_RX);
        // 如果正在接收数据且接收方法为中断
        if (uart_rx_in_progress[UART_0] && uart_rx_method[UART_0] == UART_METHOD_INTERRUPT)
        {
            // 写入接收数据到目标缓冲区
            *(uart_rx_dst[UART_0]) = DL_UART_receiveData(UART0);
            // 更新缓冲区指针
            uart_rx_dst[UART_0]++;
            // 接收字节数更新
            bytes_received[UART_0]++;

            // 如果接收完成
            if (bytes_received[UART_0] >= bytes_to_receive[UART_0])
            {
                // 接收完成
                uart_rx_in_progress[UART_0] = false;       // 重置接收状态
                uart_rx_method[UART_0] = UART_METHOD_NONE; // 重置接收方法
                // 调用注册的接收完成回调函数
                if (uart_interrupt_handler[UART_0][UART_INTERRUPT_DONE_RX] != 0)
                {
                    uart_interrupt_handler[UART_0][UART_INTERRUPT_DONE_RX]();
                }
            }
        }
        break;
    }
    // 处理UART发送中断
    case DL_UART_IIDX_TX:
    {
        // 清除发送中断标志位
        DL_UART_clearInterruptStatus(UART0, DL_UART_IIDX_TX);
        // 如果正在发送数据且发送方法为中断
        if (uart_tx_in_progress[UART_0] && uart_tx_method[UART_0] == UART_METHOD_INTERRUPT)
        {
            // 已发送字节数更新
            bytes_sent[UART_0]++;
            // 如果还有待发送字节
            if (bytes_to_send[UART_0] > bytes_sent[UART_0])
            {
                // 更新发送缓冲区指针
                uart_tx_scr[UART_0]++;
                // 从发送缓冲区读取数据并发送
                DL_UART_transmitData(UART0, *(uart_tx_scr[UART_0]));
            }
            else
            {
                // 发送完成
                uart_tx_in_progress[UART_0] = false;       // 重置发送状态
                uart_tx_method[UART_0] = UART_METHOD_NONE; // 重置发送方法
                // 调用注册的发送完成回调函数
                if (uart_interrupt_handler[UART_0][UART_INTERRUPT_DONE_TX] != 0)
                {
                    uart_interrupt_handler[UART_0][UART_INTERRUPT_DONE_TX]();
                }
            }
        }
        break;
    }
    // 处理UART DMA发送完成中断
    case DL_UART_IIDX_DMA_DONE_TX:
    {
        // 清除正在发送标志
        uart_tx_in_progress[UART_0] = false;
        // 清除发送方法
        uart_tx_method[UART_0] = UART_METHOD_NONE;
        // 清除DMA发送完成中断标志位
        DL_UART_clearInterruptStatus(UART0, DL_UART_IIDX_DMA_DONE_TX);
        // 调用注册的DMA发送完成回调函数
        if (uart_interrupt_handler[UART_0][UART_INTERRUPT_DMA_DONE_TX] != 0)
        {
            uart_interrupt_handler[UART_0][UART_INTERRUPT_DMA_DONE_TX]();
        }
        break;
    }
    // 处理UART DMA接收完成中断
    case DL_UART_IIDX_DMA_DONE_RX:
    {
        // 清除正在接收标志
        uart_rx_in_progress[UART_0] = false;
        // 清除接收方法
        uart_rx_method[UART_0] = UART_METHOD_NONE;
        // 清除DMA接收完成中断标志位
        DL_UART_clearInterruptStatus(UART0, DL_UART_IIDX_DMA_DONE_RX);
        // 调用注册的DMA接收完成回调函数
        if (uart_interrupt_handler[UART_0][UART_INTERRUPT_DMA_DONE_RX] != 0)
        {
            uart_interrupt_handler[UART_0][UART_INTERRUPT_DMA_DONE_RX]();
        }
        break;
    }
    default:
        break;
    }
}

/// @brief UART1中断服务函数
/// @param 无
void UART1_IRQHandler(void)
{
    // 读取UART1的中断状态
    switch (DL_UART_getPendingInterrupt(UART1))
    {
        // 处理UART接收中断
    case DL_UART_IIDX_RX:
    {
        // 清除接收中断标志位
        DL_UART_clearInterruptStatus(UART1, DL_UART_IIDX_RX);
        // 如果正在接收数据且接收方法为中断
        if (uart_rx_in_progress[UART_1] && uart_rx_method[UART_1] == UART_METHOD_INTERRUPT)
        {
            // 写入接收数据到目标缓冲区
            *(uart_rx_dst[UART_1]) = DL_UART_receiveData(UART1);
            // 更新缓冲区指针
            uart_rx_dst[UART_1]++;
            // 接收字节数更新
            bytes_received[UART_1]++;

            // 如果接收完成
            if (bytes_received[UART_1] >= bytes_to_receive[UART_1])
            {
                // 接收完成
                uart_rx_in_progress[UART_1] = false;       // 重置接收状态
                uart_rx_method[UART_1] = UART_METHOD_NONE; // 重置接收方法
                // 调用注册的接收完成回调函数
                if (uart_interrupt_handler[UART_1][UART_INTERRUPT_DONE_RX] != 0)
                {
                    uart_interrupt_handler[UART_1][UART_INTERRUPT_DONE_RX]();
                }
            }
        }
        break;
    }
    // 处理UART发送中断
    case DL_UART_IIDX_TX:
    {
        // 清除发送中断标志位
        DL_UART_clearInterruptStatus(UART0, DL_UART_IIDX_TX);
        // 如果正在发送数据且发送方法为中断
        if (uart_tx_in_progress[UART_1] && uart_tx_method[UART_1] == UART_METHOD_INTERRUPT)
        {
            // 已发送字节数更新
            bytes_sent[UART_1]++;
            // 如果还有待发送字节
            if (bytes_to_send[UART_1] > bytes_sent[UART_1])
            {
                // 更新发送缓冲区指针
                uart_tx_scr[UART_1]++;
                // 从发送缓冲区读取数据并发送
                DL_UART_transmitData(UART1, *(uart_tx_scr[UART_1]));
            }
            else
            {
                // 发送完成
                uart_tx_in_progress[UART_1] = false;       // 重置发送状态
                uart_tx_method[UART_1] = UART_METHOD_NONE; // 重置发送方法
                // 调用注册的发送完成回调函数
                if (uart_interrupt_handler[UART_1][UART_INTERRUPT_DONE_TX] != 0)
                {
                    uart_interrupt_handler[UART_1][UART_INTERRUPT_DONE_TX]();
                }
            }
        }
        break;
    }
    // 处理UART DMA发送完成中断
    case DL_UART_IIDX_DMA_DONE_TX:
    {
        // 清除正在发送标志
        uart_tx_in_progress[UART_1] = false;
        // 清除发送方法
        uart_tx_method[UART_1] = UART_METHOD_NONE;
        // 清除DMA发送完成中断标志位
        DL_UART_clearInterruptStatus(UART1, DL_UART_IIDX_DMA_DONE_TX);
        // 调用注册的DMA发送完成回调函数
        if (uart_interrupt_handler[UART_1][UART_INTERRUPT_DMA_DONE_TX] != 0)
        {
            uart_interrupt_handler[UART_1][UART_INTERRUPT_DMA_DONE_TX]();
        }
        break;
    }
    // 处理UART DMA接收完成中断
    case DL_UART_IIDX_DMA_DONE_RX:
    {
        // 清除正在接收标志
        uart_rx_in_progress[UART_1] = false;
        // 清除接收方法
        uart_rx_method[UART_1] = UART_METHOD_NONE;
        // 清除DMA接收完成中断标志位
        DL_UART_clearInterruptStatus(UART1, DL_UART_IIDX_DMA_DONE_RX);
        // 调用注册的DMA接收完成回调函数
        if (uart_interrupt_handler[UART_1][UART_INTERRUPT_DMA_DONE_RX] != 0)
        {
            uart_interrupt_handler[UART_1][UART_INTERRUPT_DMA_DONE_RX]();
        }
        break;
    }
    default:
        break;
    }
}

/// @brief UART2中断服务函数
/// @param 无
void UART2_IRQHandler(void)
{
    // 读取UART2的中断状态
    switch (DL_UART_getPendingInterrupt(UART2))
    {
        // 处理UART接收中断
    case DL_UART_IIDX_RX:
    {
        // 清除接收中断标志位
        DL_UART_clearInterruptStatus(UART2, DL_UART_IIDX_RX);
        // 如果正在接收数据且接收方法为中断
        if (uart_rx_in_progress[UART_2] && uart_rx_method[UART_2] == UART_METHOD_INTERRUPT)
        {
            // 写入接收数据到目标缓冲区
            *(uart_rx_dst[UART_2]) = DL_UART_receiveData(UART2);
            // 更新缓冲区指针
            uart_rx_dst[UART_2]++;
            // 接收字节数更新
            bytes_received[UART_2]++;

            // 如果接收完成
            if (bytes_received[UART_2] >= bytes_to_receive[UART_2])
            {
                // 接收完成
                uart_rx_in_progress[UART_2] = false;       // 重置接收状态
                uart_rx_method[UART_2] = UART_METHOD_NONE; // 重置接收方法
                // 调用注册的接收完成回调函数
                if (uart_interrupt_handler[UART_2][UART_INTERRUPT_DONE_RX] != 0)
                {
                    uart_interrupt_handler[UART_2][UART_INTERRUPT_DONE_RX]();
                }
            }
        }
        break;
    }
    // 处理UART发送中断
    case DL_UART_IIDX_TX:
    {
        // 清除发送中断标志位
        DL_UART_clearInterruptStatus(UART0, DL_UART_IIDX_TX);
        // 如果正在发送数据且发送方法为中断
        if (uart_tx_in_progress[UART_2] && uart_tx_method[UART_2] == UART_METHOD_INTERRUPT)
        {
            // 已发送字节数更新
            bytes_sent[UART_2]++;
            // 如果还有待发送字节
            if (bytes_to_send[UART_2] > bytes_sent[UART_2])
            {
                // 更新发送缓冲区指针
                uart_tx_scr[UART_2]++;
                // 从发送缓冲区读取数据并发送
                DL_UART_transmitData(UART2, *(uart_tx_scr[UART_2]));
            }
            else
            {
                // 发送完成
                uart_tx_in_progress[UART_2] = false;       // 重置发送状态
                uart_tx_method[UART_2] = UART_METHOD_NONE; // 重置发送方法
                // 调用注册的发送完成回调函数
                if (uart_interrupt_handler[UART_2][UART_INTERRUPT_DONE_TX] != 0)
                {
                    uart_interrupt_handler[UART_2][UART_INTERRUPT_DONE_TX]();
                }
            }
        }
        break;
    }
    // 处理UART DMA发送完成中断
    case DL_UART_IIDX_DMA_DONE_TX:
    {
        // 清除正在发送标志
        uart_tx_in_progress[UART_2] = false;
        // 清除发送方法
        uart_tx_method[UART_2] = UART_METHOD_NONE;
        // 清除DMA发送完成中断标志位
        DL_UART_clearInterruptStatus(UART2, DL_UART_IIDX_DMA_DONE_TX);
        // 调用注册的DMA发送完成回调函数
        if (uart_interrupt_handler[UART_2][UART_INTERRUPT_DMA_DONE_TX] != 0)
        {
            uart_interrupt_handler[UART_2][UART_INTERRUPT_DMA_DONE_TX]();
        }
        break;
    }
    // 处理UART DMA接收完成中断
    case DL_UART_IIDX_DMA_DONE_RX:
    {
        // 清除正在接收标志
        uart_rx_in_progress[UART_2] = false;
        // 清除接收方法
        uart_rx_method[UART_2] = UART_METHOD_NONE;
        // 清除DMA接收完成中断标志位
        DL_UART_clearInterruptStatus(UART2, DL_UART_IIDX_DMA_DONE_RX);
        // 调用注册的DMA接收完成回调函数
        if (uart_interrupt_handler[UART_2][UART_INTERRUPT_DMA_DONE_RX] != 0)
        {
            uart_interrupt_handler[UART_2][UART_INTERRUPT_DMA_DONE_RX]();
        }
        break;
    }
    default:
        break;
    }
}

/// @brief UART3中断服务函数
/// @param 无
void UART3_IRQHandler(void)
{
    // 读取UART3的中断状态
    switch (DL_UART_getPendingInterrupt(UART3))
    {
        // 处理UART接收中断
    case DL_UART_IIDX_RX:
    {
        // 清除接收中断标志位
        DL_UART_clearInterruptStatus(UART3, DL_UART_IIDX_RX);
        // 如果正在接收数据且接收方法为中断
        if (uart_rx_in_progress[UART_3] && uart_rx_method[UART_3] == UART_METHOD_INTERRUPT)
        {
            // 写入接收数据到目标缓冲区
            *(uart_rx_dst[UART_3]) = DL_UART_receiveData(UART3);
            // 更新缓冲区指针
            uart_rx_dst[UART_3]++;
            // 接收字节数更新
            bytes_received[UART_3]++;

            // 如果接收完成
            if (bytes_received[UART_3] >= bytes_to_receive[UART_3])
            {
                // 接收完成
                uart_rx_in_progress[UART_3] = false;       // 重置接收状态
                uart_rx_method[UART_3] = UART_METHOD_NONE; // 重置接收方法
                // 调用注册的接收完成回调函数
                if (uart_interrupt_handler[UART_3][UART_INTERRUPT_DONE_RX] != 0)
                {
                    uart_interrupt_handler[UART_3][UART_INTERRUPT_DONE_RX]();
                }
            }
        }
        break;
    }
    // 处理UART发送中断
    case DL_UART_IIDX_TX:
    {
        // 清除发送中断标志位
        DL_UART_clearInterruptStatus(UART3, DL_UART_IIDX_TX);
        // 如果正在发送数据且发送方法为中断
        if (uart_tx_in_progress[UART_3] && uart_tx_method[UART_3] == UART_METHOD_INTERRUPT)
        {
            // 已发送字节数更新
            bytes_sent[UART_3]++;
            // 如果还有待发送字节
            if (bytes_to_send[UART_3] > bytes_sent[UART_3])
            {
                // 更新发送缓冲区指针
                uart_tx_scr[UART_3]++;
                // 从发送缓冲区读取数据并发送
                DL_UART_transmitData(UART3, *(uart_tx_scr[UART_3]));
            }
            else
            {
                // 发送完成
                uart_tx_in_progress[UART_3] = false;       // 重置发送状态
                uart_tx_method[UART_3] = UART_METHOD_NONE; // 重置发送方法
                // 调用注册的发送完成回调函数
                if (uart_interrupt_handler[UART_3][UART_INTERRUPT_DONE_TX] != 0)
                {
                    uart_interrupt_handler[UART_3][UART_INTERRUPT_DONE_TX]();
                }
            }
        }
        break;
    }
    // 处理UART DMA发送完成中断
    case DL_UART_IIDX_DMA_DONE_TX:
    {
        // 清除正在发送标志
        uart_tx_in_progress[UART_3] = false;
        // 清除发送方法
        uart_tx_method[UART_3] = UART_METHOD_NONE;
        // 清除DMA发送完成中断标志位
        DL_UART_clearInterruptStatus(UART3, DL_UART_IIDX_DMA_DONE_TX);
        // 调用注册的DMA发送完成回调函数
        if (uart_interrupt_handler[UART_3][UART_INTERRUPT_DMA_DONE_TX] != 0)
        {
            uart_interrupt_handler[UART_3][UART_INTERRUPT_DMA_DONE_TX]();
        }
        break;
    }
    // 处理UART DMA接收完成中断
    case DL_UART_IIDX_DMA_DONE_RX:
    {
        // 清除正在接收标志
        uart_rx_in_progress[UART_3] = false;
        // 清除接收方法
        uart_rx_method[UART_3] = UART_METHOD_NONE;
        // 清除DMA接收完成中断标志位
        DL_UART_clearInterruptStatus(UART3, DL_UART_IIDX_DMA_DONE_RX);
        // 调用注册的DMA接收完成回调函数
        if (uart_interrupt_handler[UART_3][UART_INTERRUPT_DMA_DONE_RX] != 0)
        {
            uart_interrupt_handler[UART_3][UART_INTERRUPT_DMA_DONE_RX]();
        }
        break;
    }
    default:
        break;
    }
}
