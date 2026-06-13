#ifndef USERLIB_OEMT_H
#define USERLIB_OEMT_H

#include "ti_msp_dl_config.h"
#include "userlib_systick.h"

#include <stdbool.h>
#include <stdint.h>

#define OEMT_AN_COUNT 8

#ifndef OEMT_DARK
#define OEMT_DARK 0
#endif

#ifndef OEMT_LIGHT
#define OEMT_LIGHT 1
#endif

/// @brief 初始化模拟复用光电传感器模块。
/// @param result_data 光电状态输出缓存，长度至少为 OEMT_AN_COUNT。
/// @param analog_raw_data ADC 原始值指针。
/// @return true 表示初始化成功。
bool USER_OEMT_AN_Init(uint16_t *result_data, uint16_t *analog_raw_data);

/// @brief 使能模拟复用光电扫描。
void USER_OEMT_AN_Enable(void);

/// @brief 禁用模拟复用光电扫描。
void USER_OEMT_AN_Disable(void);

/// @brief 获取指定通道的 ADC 原始值。
/// @param idx 通道索引，范围 0-7。
/// @return ADC 原始值，通道越界时返回 0。
uint16_t USER_OEMT_AN_GetRawData(uint8_t idx);

/// @brief 获取实际扫描频率。
/// @return 单通道等效扫描频率，单位 Hz。
uint32_t USER_OEMT_AN_GetScanRate(void);

/// @brief 设置模拟量高滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @param threshold 高滞回阈值。
void USER_OEMT_AN_SetHysteresisHigh(uint8_t idx, uint16_t threshold);

/// @brief 设置模拟量低滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @param threshold 低滞回阈值。
void USER_OEMT_AN_SetHysteresisLow(uint8_t idx, uint16_t threshold);

/// @brief 获取模拟量高滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @return 高滞回阈值，通道越界时返回 0。
uint16_t USER_OEMT_AN_GetHysteresisHigh(uint8_t idx);

/// @brief 获取模拟量低滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @return 低滞回阈值，通道越界时返回 0。
uint16_t USER_OEMT_AN_GetHysteresisLow(uint8_t idx);

/// @brief 自动设置模拟量高滞回阈值。
/// @note 会采集 32 次当前原始数据，期间产生约 320 ms 阻塞。
void USER_OEMT_AN_AutoSetHysteresisHigh(void);

/// @brief 自动设置模拟量低滞回阈值。
/// @note 会采集 32 次当前原始数据，期间产生约 320 ms 阻塞。
void USER_OEMT_AN_AutoSetHysteresisLow(void);

#endif /* USERLIB_OEMT_H */
