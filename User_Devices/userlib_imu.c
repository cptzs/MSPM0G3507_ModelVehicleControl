/**
 * @file userlib_imu.c
 * @brief 6 轴 IMU 传感器驱动 (UART 通信协议)。
 *
 * 通过 UART 与 IMU 模块通信，解析 33 字节数据帧获取加速度/角速度/姿态角。
 * 支持校准指令序列（解锁→设置参考→锁定）和角度归一化（跨越 ±180° 边界）。
 */

#include "userlib_imu.h"

#define IMU_Cmd_Amount 6    /* IMU指令集数量 */
#define IMU_Cmd_Length 5    /* IMU指令长度 */
#define IMU_Frame_Length 33 /* IMU帧长度 */
#define TX_MIN_GAP 10       /* 串口发送最小间隔时间 */
#define RX_MAX_AWAIT 8      /* 串口接收最大等待时间,单位ms */

#define RX_BUF_SIZE 64   /* 串口接收缓冲区大小 */
#define RX_MAX_ERROR 100 /* 串口接收最大错误次数 */

enum
{
    header1 = 0,
    fid_acc = 1,
    accxl = 2,
    accxh = 3,
    accyl = 4,
    accyh = 5,
    acczl = 6,
    acczh = 7,
    tl = 8,
    th = 9,
    sum1 = 10,
    header2 = 11,
    fid_gyro = 12,
    gyroxl = 13,
    gyroxh = 14,
    gyroyl = 15,
    gyroyh = 16,
    gyrozl = 17,
    gyrozh = 18,
    vl = 19,
    vh = 20,
    sum2 = 21,
    header3 = 22,
    fid_angle = 23,
    rolll = 24,
    rollh = 25,
    pitchl = 26,
    pitchh = 27,
    yawl = 28,
    yawh = 29,
    verl = 30,
    verh = 31,
    sum3 = 32,
} rx_frame_Strcture;

enum
{
    rx_header = 0x55,
    fcn_acc = 0x51,
    fcn_gyro = 0x52,
    fcn_angle = 0x53,
} rx_frame_data;

/**
 * @brief IMU配置结构体（内部使用）
 */
typedef struct
{
    IMU_Data_StructTypeDef *data_ptr; /* 指向IMU数据的指针 */
    UART_Instance uart_channel;       /* IMU串口通道 */
    bool initialized;                 /* 初始化标志 */

    /* 通信控制变量 */
    bool uart_busy;       /* 串口忙标志 */
    uint16_t comm_cd;     /* 串口发送间隔时间倒计时 */
    uint8_t tx_stage;     /* 串口发送阶段 */
    uint8_t tx_order;     /* 串口发送指令 */
    uint8_t tx_order_old; /* 串口发送指令 */
    bool order_finished;  /* 串口发送指令完成标志 */

    /* 接收控制变量 */
    uint8_t rx_buff[RX_BUF_SIZE]; /* 串口接收缓冲区 */
    bool recieved;                /* 串口接收完成标志 */
    uint16_t recieved_bytes;      /* 串口接收完成的字节数 */
    uint16_t rx_err_frame_count;  /* 串口接收帧错误次数 */
    uint16_t rx_await;            /* 串口接收等待时间,单位ms */
    uint16_t rx_timeout;          /* 串口接收超时时间,单位ms */
    uint16_t target_rx_bytes;     /* 串口接收目标字节数 */
} IMU_Config_Typedef;

/**
 * @brief IMU指令集
 */
static uint8_t IMU_CMD[IMU_Cmd_Amount][IMU_Cmd_Length] =
    {
        {0x00, 0x00, 0x00, 0x00, 0x00}, /* 无效指令 */
        {0xFF, 0xAA, 0x03, 0x0E, 0x00}, /* 请求数据 */
        {0xFF, 0xAA, 0x69, 0x88, 0xB5}, /* 解锁 */
        {0xFF, 0xAA, 0x00, 0x00, 0x00}, /* 保存并锁定 */
        {0xFF, 0xAA, 0x01, 0x08, 0x00}, /* 设置角度参考值 */
        {0xFF, 0xAA, 0x01, 0x04, 0x00}, /* 清除偏航角 */
};

/// @brief IMU配置实例
static IMU_Config_Typedef imu_config = {0};

/// @brief 上次角度值（用于角度归一化）
float angleLast = 0;

/// @brief 角度环绕计数（用于处理跨越±180度边界的情况）
int32_t angleRoundCount = 0;

/// @brief IMU发送帧完成回调（DMA发送完成后由UART中断调用）
void USER_IMU_TxFrameFinish_Callback(void);

/// @brief IMU接收帧完成回调（DMA接收满后由UART中断调用）
void USER_IMU_RxFrameFinish_Callback(void);

/**
 * @brief IMU通信初始化函数
 * @param useSysTick 是否使用SysTick定时器
 * @param data_ptr 指向IMU数据结构的指针
 * @param uart_channel 串口通道
 * @retval 无
 */
void USER_IMU_Init(bool useSysTick, IMU_Data_StructTypeDef *data_ptr, UART_Instance uart_channel)
{
    /* 参数校验 */
    if (data_ptr == NULL)
    {
        return;
    }

    /* 配置IMU参数 */
    imu_config.data_ptr = data_ptr;
    imu_config.uart_channel = uart_channel;
    imu_config.initialized = true;

    /* 设置IMU串口接收超时时间 */
    imu_config.rx_timeout = RX_MAX_AWAIT;

    /* 设置IMU串口接收等待时间 */
    imu_config.rx_await = 0;

    /* 设置IMU串口接收目标字节数 */
    imu_config.target_rx_bytes = IMU_Frame_Length;

    /* 设置IMU串口发送间隔时间倒计时 */
    imu_config.comm_cd = TX_MIN_GAP;

    /* 设置IMU串口发送阶段 */
    imu_config.tx_stage = 0;

    /* 设置IMU串口发送指令 */
    imu_config.tx_order = IMU_ODR_READDATA;

    /* 设置IMU串口发送指令完成标志 */
    imu_config.order_finished = true;

    /* 设置IMU串口接收完成标志 */
    imu_config.recieved = false;

    /* 设置IMU串口接收错误次数 */
    imu_config.rx_err_frame_count = 0;

    /* 设置IMU串口接收完成的字节数 */
    imu_config.recieved_bytes = 0;

    /* 设置IMU数据结构体状态 */
    imu_config.data_ptr->status = IMU_STA_OK;

    /* 设置IMU数据结构体发送计数 */
    imu_config.data_ptr->tx_count = 0;

    /* 设置IMU数据结构体接收计数 */
    imu_config.data_ptr->rx_count = 0;

    /* 注册系统滴答定时器回调函数 */
    if (useSysTick)
    {
        USER_SysTick_RegisterCallback(USER_IMU_Comm_Routine);
    }

    /*注册串口DMA发送中断完成回调函数*/
    USER_UART_RegisterCallback(imu_config.uart_channel, UART_INTERRUPT_DMA_DONE_TX, USER_IMU_TxFrameFinish_Callback);

    /* 注册串口DMA接收中断完成回调函数 */
    USER_UART_RegisterCallback(imu_config.uart_channel, UART_INTERRUPT_DMA_DONE_RX, USER_IMU_RxFrameFinish_Callback);

    /* 初始化串口 */
    USER_UART_Init(imu_config.uart_channel);
}

/**
 * @brief 推送 IMU 指令至发送队列。
 * @param order 指令序号。
 */
void USER_IMU_SetCommand(uint8_t order)
{
    if (!imu_config.initialized)
    {
        return;
    }

    imu_config.tx_order = order;
    imu_config.order_finished = false;
}

/**
 * @brief 角度归一化函数
 * @details 将角度限制在[0, 360]度范围内，确保选择最短路径转向
 * @param angle 输入角度（度）
 * @retval 归一化后的角度（度）
 * @note 用于处理角度跨越±180度边界的情况
 */
float USER_IMU_NormalizeYaw(float angle)
{
    float err = angle - angleLast;

    if (err < -180)
        angleRoundCount++;
    else if (err > 180)
        angleRoundCount--;

    angleLast = angle;

    return angleRoundCount * 360 + angle;
}

/**
 * @brief 设置 IMU 指令（USER_IMU_SetCommand 的兼容性入口）。
 * @param order 指令枚举值 (IMU_ODR_READDATA / IMU_ODR_SETANGREF / IMU_ODR_SETYAWREF)。
 */
void USER_IMU_SetOrder(uint8_t order)
{
    USER_IMU_SetCommand(order);
}

/**
 * @brief 设置IMU角度参考值
 */
void USER_IMU_SetAngleRef(void)
{
    if (!imu_config.initialized)
    {
        return;
    }

    switch (imu_config.tx_stage)
    {
    case 0:
    {
        /* 发送解锁指令 */
        USER_UART_Transmit_DMA(imu_config.uart_channel, &IMU_CMD[IMU_ODR_UNLOCK][0], IMU_Cmd_Length);
        imu_config.comm_cd = 300;
        imu_config.tx_stage = 1;
        break;
    }
    case 1:
    {
        if (imu_config.comm_cd == 0)
        {
            /* 发送设置角度参考值指令 */
            USER_UART_Transmit_DMA(imu_config.uart_channel, &IMU_CMD[IMU_ODR_SETANGREF][0], IMU_Cmd_Length);
            imu_config.comm_cd = 300;
            imu_config.tx_stage = 2;
        }
        break;
    }
    case 2:
    {
        if (imu_config.comm_cd == 0)
        {
            imu_config.comm_cd = 300;
            imu_config.tx_stage = 3;
        }
        break;
    }
    case 3:
    {
        if (imu_config.comm_cd == 0)
        {
            /* 发送保存并锁定指令 */
            USER_UART_Transmit_DMA(imu_config.uart_channel, &IMU_CMD[IMU_ODR_LOCK][0], IMU_Cmd_Length);
            imu_config.comm_cd = 300;
            imu_config.tx_stage = 4;
        }
        break;
    }
    case 4:
    {
        if (imu_config.comm_cd == 0)
        {
            imu_config.comm_cd = 200;
            imu_config.tx_stage = 0;
            imu_config.order_finished = true;
        }
        break;
    }
    }
}

/**
 * @brief 清除IMU偏航角参考值
 */
void USER_IMU_SetYawRef(void)
{
    if (!imu_config.initialized)
    {
        return;
    }

    switch (imu_config.tx_stage)
    {
    case 0:
    {
        /* 发送解锁指令 */
        USER_UART_Transmit_DMA(imu_config.uart_channel, &IMU_CMD[IMU_ODR_UNLOCK][0], IMU_Cmd_Length);
        imu_config.comm_cd = 300;
        imu_config.tx_stage = 1;
        break;
    }
    case 1:
    {
        if (imu_config.comm_cd == 0)
        {
            /* 发送清除偏航角指令 */
            USER_UART_Transmit_DMA(imu_config.uart_channel, &IMU_CMD[IMU_ODR_SETYAWREF][0], IMU_Cmd_Length);
            imu_config.comm_cd = 300;
            imu_config.tx_stage = 2;
        }
        break;
    }
    case 2:
    {
        if (imu_config.comm_cd == 0)
        {
            imu_config.comm_cd = 300;
            imu_config.tx_stage = 3;
        }
        break;
    }
    case 3:
    {
        if (imu_config.comm_cd == 0)
        {
            /* 发送保存并锁定指令 */
            USER_UART_Transmit_DMA(imu_config.uart_channel, &IMU_CMD[IMU_ODR_LOCK][0], IMU_Cmd_Length);
            imu_config.comm_cd = 300;
            imu_config.tx_stage = 4;
        }
        break;
    }
    case 4:
    {
        if (imu_config.comm_cd == 0)
        {
            imu_config.comm_cd = 100;
            imu_config.tx_stage = 0;
            angleLast = 0;
            angleRoundCount = 0;
            imu_config.order_finished = true;
        }
        break;
    }
    }
}

/**
 * @brief 串口接收数据预检查
 * @param rx_data 接收数据指针
 * @retval 串口接收帧检查状态
 */
IMU_Status_EnumTypeDef USER_IMU_RxFramePreCheck(uint8_t *rx_data)
{
    /* 参数校验 */
    if (!imu_config.initialized || rx_data == NULL)
    {
        return IMU_STA_ERROR;
    }

    /* 检查帧头 */
    if (rx_data[header1] == rx_header && rx_data[header2] == rx_header && rx_data[header3] == rx_header)
    {
        /* 检查功能码 */
        if (rx_data[fid_acc] == fcn_acc && rx_data[fid_gyro] == fcn_gyro && rx_data[fid_angle] == fcn_angle)
        {
            return IMU_STA_OK;
        }
        else
        {
            return IMU_STA_RX_FORMAT_ERROR;
        }
    }
    else
    {
        return IMU_STA_RX_FORMAT_ERROR;
    }
}

/**
 * @brief 串口接收数据处理
 * @param rx_data 接收数据指针
 * @retval 串口接收帧检查状态
 */
IMU_Status_EnumTypeDef USER_IMU_RxFrameProcess(uint8_t *rx_data)
{
    IMU_Status_EnumTypeDef status;

    /* 参数校验 */
    if (!imu_config.initialized || rx_data == NULL)
    {
        return IMU_STA_ERROR;
    }

    /* 预检查 */
    status = USER_IMU_RxFramePreCheck(rx_data);
    if (status != IMU_STA_OK)
    {
        return status;
    }

    /* 解析加速度数据 */
    imu_config.data_ptr->accx = 0.004785f * (int16_t)((rx_data[accxh] << 8) | rx_data[accxl]);
    imu_config.data_ptr->accy = 0.004785f * (int16_t)((rx_data[accyh] << 8) | rx_data[accyl]);
    imu_config.data_ptr->accz = 0.004785f * (int16_t)((rx_data[acczh] << 8) | rx_data[acczl]);

    /* 解析陀螺仪数据 */
    imu_config.data_ptr->gyrox = 0.061035f * (int16_t)((rx_data[gyroxh] << 8) | rx_data[gyroxl]);
    imu_config.data_ptr->gyroy = 0.061035f * (int16_t)((rx_data[gyroyh] << 8) | rx_data[gyroyl]);
    imu_config.data_ptr->gyroz = 0.061035f * (int16_t)((rx_data[gyrozh] << 8) | rx_data[gyrozl]);

    /* 解析角度数据 */
    imu_config.data_ptr->roll = 0.005493164f * (int16_t)((rx_data[rollh] << 8) | rx_data[rolll]);
    imu_config.data_ptr->pitch = 0.005493164f * (int16_t)((rx_data[pitchh] << 8) | rx_data[pitchl]);
    imu_config.data_ptr->yaw = USER_IMU_NormalizeYaw(0.005493164f * (int16_t)((rx_data[yawh] << 8) | rx_data[yawl]));

    /* 解析温度数据 */
    imu_config.data_ptr->temperature = 0.01f * (float)((int16_t)((rx_data[th] << 8) | rx_data[tl]));

    /* 进行累加和计算 */
    // 加速度积分误差太大，没有实际意义
    //  imu_config.data_ptr->sum_accx += imu_config.data_ptr->accx;
    //  imu_config.data_ptr->sum_accy += imu_config.data_ptr->accy;
    //  imu_config.data_ptr->sum_accz += imu_config.data_ptr->accz;
    imu_config.data_ptr->sum_gyrox += imu_config.data_ptr->gyrox / 100.0f;
    imu_config.data_ptr->sum_gyroy += imu_config.data_ptr->gyroy / 100.0f;
    imu_config.data_ptr->sum_gyroz += imu_config.data_ptr->gyroz / 100.0f;

    return IMU_STA_OK;
}

/**
 * @brief IMU 通信状态机 (每 1ms 由 SysTick 回调调用)。
 * @details 管理 UART 收发时序：发送请求帧 → 等待 DMA 接收完成 → 帧预检查 → 数据解析。
 *          接收超时自动复位，连续错误超过阈值后标记超时状态。
 */
void USER_IMU_Comm_Routine(void)
{
    if (!imu_config.initialized)
    {
        return;
    }

    /* ----------------接收处理流程---------------------- */

    /* 如果串口接收完成 */
    if (imu_config.recieved)
    {
        /* 串口接收完成标志清零 */
        imu_config.recieved = false;

        /* 串口忙标志清零 */
        imu_config.uart_busy = false;

        /* 串口接收错误次数清零 */
        imu_config.rx_err_frame_count = 0;

        /* 串口接收数据预检查,同时更新串口状态 */
        imu_config.data_ptr->status = USER_IMU_RxFramePreCheck(imu_config.rx_buff);

        /* 如果串口接收帧无错误 */
        if (imu_config.data_ptr->status == IMU_STA_OK)
        {
            /* 串口接收数据处理 */
            USER_IMU_RxFrameProcess(imu_config.rx_buff);

            /* 串口接收计数+1 */
            imu_config.data_ptr->rx_count++;
        }
    }
    /* 如果未接收完成，且串口忙 */
    else if (imu_config.uart_busy)
    {
        /* 如果串口接收等待时间小于串口接收超时时间 */
        if (imu_config.rx_await < imu_config.rx_timeout)
        {
            /* 串口接收等待时间+1 */
            imu_config.rx_await++;
        }
        else
        {
            /* 停止串口接收 */
            USER_UART_Abort_Receive(imu_config.uart_channel);

            /* 记录串口接收到的字节数 */
            imu_config.recieved_bytes = USER_UART_GetReceivedBytes_DMA(imu_config.uart_channel);

            /* 串口接收完成标志清零 */
            imu_config.recieved = false;

            /* 串口忙标志清零 */
            imu_config.uart_busy = false;

            /* 串口接收错误次数统计 */
            if (imu_config.rx_err_frame_count < RX_MAX_ERROR)
            {
                /* 串口接收错误次数+1 */
                imu_config.rx_err_frame_count++;
            }
            else
            {
                /* 记录串口状态为接收超时 */
                imu_config.data_ptr->status = IMU_STA_RX_TIMEOUT;
            }
        }
    }

    /* ----------------发送处理流程---------------------- */
    /* 进行串口发送空闲倒计时 */
    if (imu_config.comm_cd > 0)
    {
        imu_config.comm_cd--;
    }

    if (!imu_config.uart_busy)
    {
        /* 如果串口发送间隔时间倒计时结束 */
        if (imu_config.comm_cd == 0)
        {
            if (imu_config.order_finished == true)
            {
                imu_config.tx_order_old = IMU_ODR_READDATA;
            }
            else
            {
                if (imu_config.tx_order_old == IMU_ODR_READDATA)
                    imu_config.tx_order_old = imu_config.tx_order;
            }

            switch (imu_config.tx_order_old)
            {
            case IMU_ODR_READDATA:
            {
                /* 串口忙 */
                imu_config.uart_busy = true;

                /* 初始化串口帧间隔倒计时 */
                imu_config.comm_cd = TX_MIN_GAP;

                /* 发送查询帧 */
                USER_UART_Transmit_DMA(imu_config.uart_channel, &IMU_CMD[IMU_ODR_READDATA][0], IMU_Cmd_Length);
                break;
            }

            case IMU_ODR_SETANGREF:
            {
                /* 串口忙 */
                imu_config.uart_busy = true;
                USER_IMU_SetAngleRef();

                break;
            }

            case IMU_ODR_SETYAWREF:
            {
                /* 串口忙 */
                imu_config.uart_busy = true;
                USER_IMU_SetYawRef();

                break;
            }
            }
        }
    }
}

/**
 * @brief IMU UART发送完成回调函数
 * @retval 无
 */
void USER_IMU_TxFrameFinish_Callback(void)
{
    if (!imu_config.initialized)
    {
        return;
    }
    // 串口发送计数+1
    imu_config.data_ptr->tx_count++;

    // 如果串口发送指令为请求数据
    if (imu_config.tx_order_old == IMU_ODR_READDATA)
    {
        // 设置串口接收倒计时长度
        imu_config.rx_await = 0;

        // 设置串口接收目标字节数
        imu_config.target_rx_bytes = IMU_Frame_Length;

        // 开启串口接收
        USER_UART_Receive_DMA(imu_config.uart_channel, imu_config.rx_buff, imu_config.target_rx_bytes);
    }
    // 如果串口发送指令不是请求数据
    else if (imu_config.tx_order_old != IMU_ODR_READDATA)
    {
        imu_config.target_rx_bytes = 0;
        imu_config.uart_busy = false;
    }
}

/**
 * @brief IMU UART接收完成回调函数
 * @retval 无
 */
void USER_IMU_RxFrameFinish_Callback(void)
{
    if (!imu_config.initialized)
    {
        return;
    }

    /* 设置接收完成标志 */
    imu_config.recieved = true;
    imu_config.recieved_bytes = imu_config.target_rx_bytes;
}