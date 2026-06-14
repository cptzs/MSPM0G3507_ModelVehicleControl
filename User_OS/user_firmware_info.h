#ifndef FIRMWARE_INFO_H
#define FIRMWARE_INFO_H

#include <stdint.h>

#define USER_FIRMWARE_VERSION "v1.0.8"
#define USER_FIRMWARE_CPU_MODEL "MSPM0G3507"

const char *USER_Firmware_GetVersion(void);
const char *USER_Firmware_GetCpuModel(void);
uint32_t USER_Firmware_GetCpuClockHz(void);
uint32_t USER_Firmware_GetRamTotalBytes(void);
uint32_t USER_Firmware_GetRamUsedBytes(void);
uint32_t USER_Firmware_GetFlashTotalBytes(void);
uint32_t USER_Firmware_GetFlashUsedBytes(void);
uint16_t USER_Firmware_GetRamUsedPermille(void);
uint16_t USER_Firmware_GetFlashUsedPermille(void);

#endif /* FIRMWARE_INFO_H */
