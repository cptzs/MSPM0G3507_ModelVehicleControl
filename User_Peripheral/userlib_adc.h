#ifndef USERLIB_ADC_H
#define USERLIB_ADC_H

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  //整数类型库函数
#include <stdbool.h> //布尔类型库函数
#include <string.h>  //字符串操作库函数
#include <stdlib.h>  //标准库函数
#include <math.h>    //数学库函数

#define ADC_REF_VOLTAGE 3300.0f // ADC参考电压，单位为mV

/* 温度传感器参数定义 */
#define TEMP_TRIM_REFERENCE 30.0f // 校准参考温度，单位°C
#define TEMP_SENSOR_TC (-1.8f)    // 温度系数，单位mV/°C

/// @brief ADC初始化函数
/// @param pbuffer 存储ADC采样结果的缓冲区
/// @param ch_count ADC通道数量
void USER_ADC_Init(uint16_t *pbuffer, uint16_t ch_count);

/// @brief ADC反初始化函数
/// @param None
void USER_ADC_DeInit(void);

/// @brief 获取内部温度值
/// @param rawdata ADC原始采样值
/// @return 转换后的温度值，单位°C
float USER_ADC_GetInnerTemperature(uint16_t rawdata);

/// @brief 获取电源电压值
/// @param rawdata ADC15通道的ADC原始采样值（经过VDD/3分压）
/// @return 转换后的VDD电压值，单位mV
uint16_t USER_ADC_GetPowerVoltage(uint16_t rawdata);

/// @brief 将ADC采样值转换为电压值
/// @param rawdata ADC原始采样值
uint16_t USER_ADC_ValueToVoltage(uint16_t rawdata);

#endif // USERLIB_ADC_H