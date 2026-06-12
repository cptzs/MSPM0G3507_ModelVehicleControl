#include "userlib_lidar.h"

/*
激光雷达采用CAN通信协议，波特率500Kbps，CAN2.0b标准。
主机（本机）发送请求帧，接收方（lidar）响应数据帧。一共4个lidar设备，分别为0x201、0x202、0x203、0x204。
采用轮询发送方式发送ID为0x402的数据帧，每隔8ms发送一个请求，轮流查询四个从机，自动切换。
发送数据格式如下：
| 字节 | 0       | 1       | 2       | 3            | 4       | 5       | 6       | 7       |
| 名称 | 0xFF    | 0xFF    | 0xFF    |接收方ID低字节 | 0xFF    | 0xFF    | 0xFF    | 0xFF    |

接收数据格式如下：
| 字节 | 0       | 1       | 2       | 3       | 4       | 5       | 6       | 7       |
| 名称 | 距离低  | 距离中  | 距离高  | 状态字节 | 强度低  | 强度高  | 保留字段低 |保留字段高|
距离单位为mm，状态字节为LidarSt_t_Typedef枚举类型，强度单位为1Mcps。
距离数据为24位，低字节在前，高字节在后。强度数据为16位，低字节在前，高字节在后。
注意：当前激光雷达的有效量程，在硬件上限制为了6.5m，即6500mm。所以，距离高字节实际上被舍弃。
*/

/**
 * @brief 激光雷达配置结构体（内部使用）
 */
typedef struct
{
    Lidar_Data_Typedef *data_ptr; /* 指向激光雷达数据数组的指针 */
    bool initialized;             /* 初始化标志 */

    /* 通信控制变量 */
    uint16_t tx_countdown; /* 发送倒计时 */
    uint8_t slave_id;      /* 当前查询从机号 */

    /* 接收控制变量 */
    bool frame_received[LIDAR_COUNT_P1];             /* 接收到帧的标志 */
    bool data_ready[LIDAR_COUNT_P1];                 /* 数据就绪标志 */
    CAN_Msg_StructTypedef rx_buffer[LIDAR_COUNT_P1]; /* 接收缓存区 */

    /* CAN消息结构体 */
    CAN_Msg_StructTypedef tx_msg; /* 发送消息结构体 */
    CAN_Msg_StructTypedef rx_msg; /* 接收消息结构体 */
} Lidar_Config_Typedef;

/**
 * @brief 激光雷达配置实例
 */
static Lidar_Config_Typedef lidar_config = {0};

/**
 * @brief 解析激光雷达接收数据
 * @note 该函数需要在主流程中调用，调用间隔应为8ms
 */
void USER_LIDAR_Task(void)
{
    uint8_t i;
    uint8_t *l_data;

    if (!lidar_config.initialized)
    {
        return;
    }

    for (i = 1; i < LIDAR_COUNT_P1; i++)
    {
        /* 如果当前从机有新数据 */
        if (!lidar_config.data_ready[i])
        {
            continue; /* 跳过没有数据的从机 */
        }

        /* 使用指针提高访问效率 */
        l_data = lidar_config.rx_buffer[i].data;

        /* 解析缓存区中的数据 */
        lidar_config.data_ptr[i].id = lidar_config.rx_buffer[i].id;

        /* 解析距离数据：字节0(低) 字节1(中) 字节2(高) - 24位数据，低字节在前 */
        lidar_config.data_ptr[i].distance = ((uint32_t)l_data[0]) |
                                            (((uint32_t)l_data[1]) << 8) |
                                            (((uint32_t)l_data[2]) << 16);

        lidar_config.data_ptr[i].status = (LidarSt_t_Typedef)l_data[3];

        /* 解析强度数据：字节4(低) 字节5(高) - 16位数据，低字节在前 */
        lidar_config.data_ptr[i].strength = ((uint16_t)l_data[4]) |
                                            (((uint16_t)l_data[5]) << 8);

        /* 解析保留字段：字节6(低) 字节7(高) - 16位数据，低字节在前 */
        lidar_config.data_ptr[i].reserved = ((uint16_t)l_data[6]) |
                                            (((uint16_t)l_data[7]) << 8);

        /* 清除数据就绪标志 */
        lidar_config.data_ready[i] = false;
    }
}

/**
 * @brief SysTick回调函数处理激光雷达数据
 * @note 每8ms发送一次激光雷达查询请求，轮流查询四个从机
 * @note 该函数需要在SysTick中断中调用
 * @note slave_id在1到4之间循环
 */
void USER_SysTick_Callback_lidar_Process(void)
{
    if (!lidar_config.initialized)
    {
        return;
    }

    /* 每隔8ms处理一次CAN流程 */
    if (lidar_config.tx_countdown == 0)
    {
        /* 如果当前从机有接收到数据 */
        if (lidar_config.frame_received[lidar_config.slave_id])
        {
            /* 重置接收超时计数 */
            lidar_config.data_ptr[lidar_config.slave_id].timeout_count = 0;
            /* 重置接收标志 */
            lidar_config.frame_received[lidar_config.slave_id] = false;
        }
        /* 如果当前从机没有接收到数据 */
        else
        {
            /* 如果接收超时计数超过最大值 */
            if (lidar_config.data_ptr[lidar_config.slave_id].timeout_count > LIDAR_RX_TIMEOUT)
            {
                /* 设置状态为超时 */
                lidar_config.data_ptr[lidar_config.slave_id].status = LIDAR_STA_TIMEOUT;
            }
            /* 否则增加接收超时计数 */
            else
            {
                lidar_config.data_ptr[lidar_config.slave_id].timeout_count++;
            }
        }

        /* 切换到下一个从机ID */
        lidar_config.slave_id++;
        if (lidar_config.slave_id >= LIDAR_COUNT_P1)
            lidar_config.slave_id = 1; /* 从1开始，0是主机 */

        /* 填充动态数据：从机ID的低字节 */
        lidar_config.tx_msg.data[3] = lidar_config.slave_id; /* 当前从机ID */

        /* 发送CAN消息 */
        if (USER_CAN_SendMessage(&lidar_config.tx_msg))
        {
            /* 记录发送帧计数 */
            lidar_config.data_ptr[lidar_config.slave_id].tx_count++;
        }

        /* 重置发送倒计时 */
        lidar_config.tx_countdown = LIDAR_TX_INTERVAL;
    }
    /* 如果倒计时大于0，减少倒计时 */
    else if (lidar_config.tx_countdown > 0)
    {
        lidar_config.tx_countdown--;
    }
}

/**
 * @brief CAN接收FIFO 0新消息处理函数
 * @details 当接收FIFO 0有新消息时调用此函数（快速缓存，最小中断时间）
 */
void CAN_RxFifo0NewMessageHandler(void)
{
    uint8_t slave_id;

    if (!lidar_config.initialized)
    {
        return;
    }

    /* 读取接收到的CAN消息 */
    if (USER_CAN_ReadMessage(&lidar_config.rx_msg) != true)
    {
        return; /* 如果读取失败，直接返回 */
    }

    /* 只处理ID为0x201-0x204的消息 */
    if (lidar_config.rx_msg.id < 0x201 || lidar_config.rx_msg.id > 0x204)
    {
        return; /* 如果ID不在有效范围，直接返回 */
    }

    /* 提取从机ID（1-4） */
    slave_id = lidar_config.rx_msg.id - 0x200;

    /* 快速缓存数据到缓冲区（避免复杂解析操作） */
    memcpy(&lidar_config.rx_buffer[slave_id], &lidar_config.rx_msg, sizeof(CAN_Msg_StructTypedef));

    /* 设置数据就绪标志，让主程序处理数据解析 */
    lidar_config.data_ready[slave_id] = true;

    /* 设置接收帧标志，让SysTick更新从机状态 */
    lidar_config.frame_received[slave_id] = true;

    /* 记录接收帧计数 */
    lidar_config.data_ptr[slave_id].rx_count++;
}

/**
 * @brief 激光雷达模块初始化
 * @param data_ptr 外部数据结构指针数组
 * @details 初始化CAN通信和数据结构，与外部数据建立连接
 */
void USER_lidar_Init(Lidar_Data_Typedef *data_ptr)
{
    uint16_t i;

    /* 检查参数有效性 */
    if (data_ptr == NULL)
    {
        return;
    }

    /* 初始化内部配置结构体 */
    memset(&lidar_config, 0, sizeof(lidar_config));

    /* 设置外部数据指针 */
    lidar_config.data_ptr = data_ptr;

    /* 初始化发送倒计时和从机ID */
    lidar_config.tx_countdown = 8; /* 初始化倒计时 */
    lidar_config.slave_id = 1;     /* 初始化从机ID */

    /* 初始化lidar接收超时计数 */
    for (i = 1; i < LIDAR_COUNT_P1; i++)
    {
        lidar_config.data_ptr[i].timeout_count = 0;
        lidar_config.data_ptr[i].rx_count = 0; /* 初始化接收计数 */
        lidar_config.data_ptr[i].tx_count = 0; /* 初始化发送计数 */
    }

    /* 初始化lidar接收数据数组 */
    for (i = 1; i < LIDAR_COUNT_P1; i++)
    {
        lidar_config.data_ptr[i].id = 0x200 + i;               /* 初始化ID */
        lidar_config.data_ptr[i].distance = 0;                 /* 初始化距离 */
        lidar_config.data_ptr[i].status = LIDAR_STA_NO_TARGET; /* 初始化状态为无目标 */
        lidar_config.data_ptr[i].strength = 0;                 /* 初始化强度 */
        lidar_config.data_ptr[i].reserved = 0;                 /* 初始化保留字段 */
    }

    /* 设置CAN发送消息ID */
    lidar_config.tx_msg.id = 0x402;

    /* 设置发送数据长度 */
    lidar_config.tx_msg.dlc = 8;

    /* 填充发送数据 */
    lidar_config.tx_msg.data[0] = 0xff; /* 固定值 */
    lidar_config.tx_msg.data[1] = 0xff; /* 固定值 */
    lidar_config.tx_msg.data[2] = 0xff; /* 固定值 */
    lidar_config.tx_msg.data[3] = 1;    /* 当前从机ID */
    lidar_config.tx_msg.data[4] = 0xff; /* 固定值 */
    lidar_config.tx_msg.data[5] = 0xff; /* 固定值 */
    lidar_config.tx_msg.data[6] = 0xff; /* 固定值 */
    lidar_config.tx_msg.data[7] = 0xff; /* 固定值 */

    /* 初始化CAN模块 */
    if (!USER_CAN_Init())
    {
        /* 初始化失败，处理错误 */
        return;
    }

    /* 注册CAN接收消息回调函数 */
    USER_CAN_RegisterCallback(CAN_INTR_RF0N, CAN_RxFifo0NewMessageHandler);

    /* 注册SysTick回调函数 */
    USER_SYSTICK_RegisterCallback(USER_SysTick_Callback_lidar_Process);

    /* 标记初始化完成 */
    lidar_config.initialized = true;
}