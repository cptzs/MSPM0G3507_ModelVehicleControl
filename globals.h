/**
 * @file globals.h
 * @brief 全局变量外部声明与 ADC 通道枚举。
 *
 * 所有模块通过包含本头访问传感器数据、PID 控制器和 Modbus 寄存器。
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include "userlib_encoder.h"
#include "userlib_imu.h"
#include "userlib_adc.h"
#include "userlib_lidar.h"

#include "userapp_mcm.h"

#define MODBUS_REG_COUNT 128 // 定义Modbus寄存器数量

/// @brief ADC通道枚举
typedef enum
{
    ADC_CHANNEL_0_P27 = 0, // ADC通道0
    ADC_CHANNEL_1_P26,     // ADC通道1
    ADC_CHANNEL_2_P25,     // ADC通道2，暂定光电扫描口
    ADC_CHANNEL_3_P24,     // ADC通道3
    ADC_CHANNEL_4_P22,     // ADC通道4
    ADC_CHANNEL_11_TEMP,   // ADC通道11（温度传感器）
    ADC_CHANNEL_15_PWR,    // ADC通道15（电源电压）
} ADC_Channel_EnumTypeDef;

extern EncoderData_t_Typedef encoder_data[3];
extern IMU_Data_StructTypeDef imu_data;
extern PID_Control_Struct_TypeDef speed_pid[2];
extern PID_Control_Struct_TypeDef distance_pid;
extern PID_Control_Struct_TypeDef angle_pid;
extern uint16_t adc_data[7];
extern uint16_t adc_voltage[7];
extern uint16_t pot_position;
extern float cpu_core_temperature;
extern uint16_t power_voltage;
extern Lidar_Data_Typedef lidar_data[5];
extern uint16_t oemt_data[8];

extern uint16_t modbus_regs[MODBUS_REG_COUNT];
extern uint8_t modbus_status;

void USER_GlobalData_Init(void);
void USER_GlobalData_Task(void);

#endif
