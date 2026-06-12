#ifndef USERLIB_OEMT_DG_H
#define USERLIB_OEMT_DG_H

#include "ti_msp_dl_config.h"
#include "userlib_sys.h"

#include <stdbool.h>
#include <stdint.h>

#define OEMT_DG_COUNT 8

#ifndef OEMT_DARK
#define OEMT_DARK 0
#endif

#ifndef OEMT_LIGHT
#define OEMT_LIGHT 1
#endif

/// @brief 初始化数字 8 路光电传感器模块。
/// @param result_data 光电状态输出缓存，长度至少为 OEMT_DG_COUNT。
/// @return true 表示初始化成功。
bool USER_OEMT_DG_Init(uint16_t *result_data);

/// @brief 使能数字 8 路光电扫描。
void USER_OEMT_DG_Enable(void);

/// @brief 禁用数字 8 路光电扫描。
void USER_OEMT_DG_Disable(void);

/// @brief 获取指定通道的数字原始值。
/// @param idx 通道索引，范围 0-7。
/// @return 0 或 1，通道越界时返回 0。
uint16_t USER_OEMT_DG_GetRawData(uint8_t idx);

/// @brief 获取实际扫描频率。
/// @return 单通道等效扫描频率，单位 Hz。
uint32_t USER_OEMT_DG_GetScanRate(void);

#endif /* USERLIB_OEMT_DG_H */
