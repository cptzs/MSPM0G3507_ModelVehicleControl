/**
 * @file globals.c
 * @brief 全局变量定义与全局数据处理任务。
 *
 * 集中保存所有传感器数据、PID 实例、ADC 结果、Modbus 寄存器的运行时存储。
 * USER_GlobalData_Task() 每 1ms 执行 ADC→电压/温度换算和编码器清零逻辑。
 */

#include "globals.h"

/// @brief 编码器数据数组。
EncoderData_t_Typedef encoder_data[3];

/// @brief IMU 数据结构体。
IMU_Data_StructTypeDef imu_data = {0};

/// @brief 速度 PID 控制器数组。
PID_Control_Struct_TypeDef speed_pid[2] = {0};

/// @brief 距离 PID 控制器。
PID_Control_Struct_TypeDef distance_pid = {0};

/// @brief 角度 PID 控制器。
PID_Control_Struct_TypeDef angle_pid = {0};

/// @brief ADC 原始采样数据。
uint16_t adc_data[7] = {0};

/// @brief ADC 电压换算结果，单位 mV。
uint16_t adc_voltage[7] = {0};

/// @brief 电位器位置，单位百分比。
uint16_t pot_position = 0;

/// @brief CPU 核心温度。
float cpu_core_temperature = 0.0f;

/// @brief 电源电压。
uint16_t power_voltage = 0;

/// @brief LiDAR 数据数组；下标 1..4 对应 4 个 LiDAR 节点。
Lidar_Data_Typedef lidar_data[5] = {0};

/// @brief 光电传感器比较结果。
uint16_t oemt_data[8] = {0};

/// @brief Modbus 保持寄存器存储区。
uint16_t modbus_regs[MODBUS_REG_COUNT] = {0};

/// @brief Modbus 通信状态。
uint8_t modbus_status = 0;

/// @brief 全局数据处理任务
/// @param 无
/// @return 无
/// 该函数在主循环中调用，用于处理全局数据
void USER_GlobalData_Task(void)
{
    uint8_t i;

    /* 处理 ADC 数据。内部温度和电源电压通道使用特殊参考源，不用通用电压换算。 */
    for (i = 0; i < 4; i++)
    {
        adc_voltage[i] = USER_ADC_ValueToVoltage(adc_data[i]);
    }

    pot_position = adc_data[ADC_CHANNEL_4_P22] * 100 / 2048;
    if (pot_position > 100)
    {
        pot_position = 100;
    }

    cpu_core_temperature = USER_ADC_GetInnerTemperature(adc_data[ADC_CHANNEL_11_TEMP]);
    power_voltage = USER_ADC_GetPowerVoltage(adc_data[ADC_CHANNEL_15_PWR]);
}

/// @brief 初始化全局数据。
void USER_GlobalData_Init(void)
{
    uint8_t i;

    for (i = 0; i < 7; i++)
    {
        adc_data[i] = 0;
    }

    for (i = 0; i < 8; i++)
    {
        oemt_data[i] = 0;
    }

    for (i = 0; i < MODBUS_REG_COUNT; i++)
    {
        modbus_regs[i] = 0;
    }
    modbus_status = 0;

    for (i = 0; i < 5; i++)
    {
        lidar_data[i].id = 0x200 + i;
        lidar_data[i].distance = 0;
        lidar_data[i].status = 0;
        lidar_data[i].strength = 0;
        lidar_data[i].reserved = 0;
        lidar_data[i].rx_count = 0;
        lidar_data[i].tx_count = 0;
        lidar_data[i].timeout_count = 0;
    }

    for (i = 0; i < 3; i++)
    {
        encoder_data[i].speed = 0;
        encoder_data[i].status = 0;
        encoder_data[i].direction = 0;
        encoder_data[i].sum_distance = 0;
    }

    imu_data.accx = 0;
    imu_data.accy = 0;
    imu_data.accz = 0;
    imu_data.gyrox = 0;
    imu_data.gyroy = 0;
    imu_data.gyroz = 0;
    imu_data.roll = 0;
    imu_data.pitch = 0;
    imu_data.yaw = 0;
    imu_data.temperature = 0.0f;
    imu_data.status = 0;
    imu_data.tx_count = 0;
    imu_data.rx_count = 0;

    for (i = 0; i < 2; i++)
    {
        speed_pid[i].kp = 1.8f;
        speed_pid[i].ki = 0.5f;
        speed_pid[i].kd = 0.1f;
        speed_pid[i].target = 0;
        speed_pid[i].current = 0;
        speed_pid[i].error = 0;
        speed_pid[i].integral = 0;
        speed_pid[i].derivative = 0;
        speed_pid[i].output = 0;
        speed_pid[i].delta_output = 0;

        speed_pid[i].last_error = 0;
        speed_pid[i].last_last_error = 0;
        speed_pid[i].last_output = 0;

        speed_pid[i].deadzone = 0;
        speed_pid[i].vi_enable = false;

        speed_pid[i].max_integral = 2000;
        speed_pid[i].min_integral = -2000;
        speed_pid[i].integral_depart = 500;
        speed_pid[i].max_output = 1000;
        speed_pid[i].min_output = -1000;
        speed_pid[i].max_delta_output = 300;
        speed_pid[i].min_delta_output = -300;
    }

    distance_pid.kp = 0.055f;
    distance_pid.ki = 0.015f;
    distance_pid.kd = 0.005f;
    distance_pid.target = 0;
    distance_pid.current = 0;
    distance_pid.output = 0;
    distance_pid.max_output = 0;
    distance_pid.min_output = -0;
    distance_pid.deadzone = 0;
    distance_pid.integral_depart = 500;
    distance_pid.max_integral = 2000;
    distance_pid.min_integral = -2000;

    angle_pid.kp = 0.5f;
    angle_pid.ki = 0.1f;
    angle_pid.kd = 0.05f;
    angle_pid.target = 0;
    angle_pid.current = 0;
    angle_pid.output = 0;
    angle_pid.max_output = 100;
    angle_pid.min_output = -100;
    angle_pid.max_integral = 100;
    angle_pid.min_integral = -100;
    angle_pid.integral_depart = 10;
    angle_pid.deadzone = 2;
}
