/**
 ******************************************************************************
 * @file    userlib_oled.h
 * @brief   OLED 12864 驱动库头文件 - 适用于TI MSPM0G3507
 * @author  赵帅
 * @version 2.11b (优化版本)
 * @date    2025-07-06
 ******************************************************************************
 * @attention
 *
 * 适用范围：TI MSPM0G3507 (MSPM0 系列微控制器)
 * 支持其他具有SPI+DMA功能的ARM Cortex-M0+微控制器
 *
 * 硬件连接：
 * OLED模块    ->  MSPM0G3507引脚
 * D/C         ->  可配置GPIO (数据/命令选择)
 * NRST        ->  可配置GPIO (复位控制)
 * NSS/CS      ->  可配置GPIO (片选，可选)
 * MOSI        ->  SPI_MOSI引脚
 * SCLK        ->  SPI_SCLK引脚
 * VCC         ->  3.3V
 * GND         ->  GND
 *
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USERLIB_OLED_H__
#define __USERLIB_OLED_H__

/* Includes ------------------------------------------------------------------*/

// TI底层库头文件
#include "ti_msp_dl_config.h"

// C标准库头文件
#include <stdint.h>  //整数类型库函数
#include <stdbool.h> //布尔类型库函数
#include <string.h>  //字符串操作库函数
#include <stdlib.h>  //标准库函数
#include <math.h>    //数学库函数

// 用户外设接口头文件
#include "userlib_systick.h" // SysTick驱动接口
// 用户设备头文件

typedef enum OLED_Reverse
{
  OLED_Dir_Normal = 0, // 正常显示
  OLED_Dir_Reverse,    // 上下翻转显示
} OLED_Reverse_EnumTypeDef;

// 错误码枚举
typedef enum
{
  OLED_OK = 0,           // 操作成功
  OLED_ERROR = 1,        // 一般错误
  OLED_ERROR_PARAM = 2,  // 参数错误
  OLED_ERROR_INIT = 3,   // 初始化错误
  OLED_ERROR_TIMEOUT = 4 // 超时错误
} OLED_Status_t;

/***************************外部接口驱动函数声明****************************/

/*********************************工具函数*********************************/
/// @brief 初始化OLED
/// @param None
/// @return OLED_Status_t
/// @retval OLED_OK 初始化成功
/// @retval OLED_ERROR 初始化失败
OLED_Status_t USER_OLED_Init(void);

/// @brief 反初始化OLED
/// @param None
/// @return OLED_Status_t
/// @retval OLED_OK 反初始化成功
/// @retval OLED_ERROR 反初始化失败
OLED_Status_t USER_OLED_DeInit(void);

/// @brief OLED固定帧率刷新服务，由UI任务周期调用
/// @param None
void USER_OLED_Service(void);

/// @brief 清屏
/// @param None
void USER_OLED_CleanScreen(void);

/// @brief 清除指定行
/// @param row 行号(0-7)
void USER_OLED_CleanRow(uint8_t row);

/// @brief 设置对比度
/// @param contrast 对比度值(0-255)
void USER_OLED_SetContrast(uint8_t contrast);

/// @brief 设置显示开关
/// @param on true: 开启显示; false: 关闭显示
void USER_OLED_SetDisplayOn(bool on);

/// @brief 反转显示
/// @param invert true: 反转显示; false: 正常显示
void USER_OLED_InvertDisplay(bool invert);

/***************************字符串和文本显示函数****************************/
/// @brief 在指定位置显示字符串
/// @param row 行号(0-7)
/// @param column 列号(0-20)
/// @param str 字符串指针
/// @param length 字符串长度(最大21字符)
void USER_OLED_putString(uint8_t row, uint8_t column, const char *string, uint8_t length);

/// @brief 在指定位置显示字符
/// @param row 行号(0-7)
/// @param column 列号(0-20)
/// @param ch 字符
void USER_OLED_putChar(uint8_t row, uint8_t column, char ch);

/*******************************数值显示函数*******************************/
/// @brief 在指定位置以HEX格式显示16位无符号整数
/// @param row 行号(0-7)
/// @param column 列号(0-20)
/// @param number 16位整数 (0-65535)
/// @param length 显示长度 (1-4字符)
void USER_OLED_putX16(uint8_t row, uint8_t column, uint16_t number, uint8_t length);

/// @brief 在指定位置以十进制格式显示16位无符号整数
/// @param row 行号(0-7)
/// @param column 列号(0-20)
/// @param number 16位无符号整数 (0-65535)
/// @param length 显示长度 (1-5字符)
void USER_OLED_putUI16(uint8_t row, uint8_t column, uint16_t number, uint8_t length);

/// @brief 在指定位置以十进制格式显示16位有符号整数
/// @param row 行号(0-7)
/// @param column 列号(0-20)
/// @param number 16位有符号整数 (-32768到32767)
/// @param length 显示长度 (2-6字符，包括符号位)
void USER_OLED_putI16(uint8_t row, uint8_t column, int16_t number, uint8_t length);

/// @brief 在指定位置以浮点数格式显示数值
/// @param row 行号(0-7)
/// @param column 列号(0-20)
/// @param number 浮点数
/// @param int_length 整数部分长度 (1-5字符)
/// @param float_length 小数部分长度 (1-5字符)
void USER_OLED_putFloat(uint8_t row, uint8_t column, float number, uint8_t int_length, uint8_t float_length);

/*******************************图形显示函数*******************************/
/// @brief 在指定位置绘制点
/// @param x X坐标 (0-127)
/// @param y Y坐标 (0-63)
void USER_OLED_SetPoint(uint8_t x, uint8_t y);

/// @brief 在指定位置清除点
/// @param x X坐标 (0-127)
/// @param y Y坐标 (0-63)
void USER_OLED_ResetPoint(uint8_t x, uint8_t y);

/// @brief 绘制进度条
/// @param row 行号(0-7)
/// @param percent 百分比(0-100)
void USER_OLED_DrawBar(uint8_t row, uint8_t percent);

/// @brief 绘制水平线
/// @param x1 起点X坐标 (0-127)
/// @param x2 终点X坐标 (0-127)
/// @param y Y坐标 (0-63)
void USER_OLED_DrawHLine(uint8_t x1, uint8_t x2, uint8_t y);

/// @brief 绘制垂直线
/// @param x X坐标 (0-127)
/// @param y1 起点Y坐标 (0-63)
/// @param y2 终点Y坐标 (0-63)
void USER_OLED_DrawVLine(uint8_t x, uint8_t y1, uint8_t y2);

/// @brief 绘制直线
/// @param x1 起点X坐标 (0-127)
/// @param y1 起点Y坐标 (0-63)
/// @param x2 终点X坐标 (0-127)
/// @param y2 终点Y坐标 (0-63)
void USER_OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/// @brief 绘制虚线
/// @param x1 起点X坐标 (0-127)
/// @param y1 起点Y坐标 (0-63)
/// @param x2 终点X坐标 (0-127)
/// @param y2 终点Y坐标 (0-63)
/// @param dash_length 虚线段长度 (建议值: 2-8)
/// @param gap_length 间隙长度 (建议值: 2-8)
void USER_OLED_DrawDashedLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t dash_length, uint8_t gap_length);

/// @brief 绘制矩形
/// @param x1 左上角X坐标 (0-127)
/// @param y1 左上角Y坐标 (0-63)
/// @param x2 右下角X坐标 (0-127)
/// @param y2 右下角Y坐标 (0-63)
/// @param fill true: 填充; false: 不填充
void USER_OLED_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool fill);

/// @brief 绘制圆形
/// @param x0 圆心X坐标 (0-127)
/// @param y0 圆心Y坐标 (0-63)
/// @param radius 半径 (1-63)
/// @param fill true: 填充; false: 不填充
void USER_OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t radius, bool fill);

/// @brief 绘制圆弧
/// @param x0 圆心X坐标 (0-127)
/// @param y0 圆心Y坐标 (0-63)
/// @param radius 半径 (1-63)
/// @param start_angle 起始角度（度，0度为向右，逆时针增加，0-359）
/// @param end_angle 结束角度（度，0度为向右，逆时针增加，0-359）
void USER_OLED_DrawArc(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t start_angle, uint16_t end_angle);

/*******************************波形显示函数*******************************/
/// @brief 更新波形数据
/// @param value 新增的波形值 (0-63)
void USER_OLED_UpdateWave(uint8_t value);

/// @brief 清除波形数据
/// @param None
void USER_OLED_ClearWave(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#endif /*__USERLIB_OLED_H__*/

/************************ (C) COPYRIGHT TI *****END OF FILE****/
