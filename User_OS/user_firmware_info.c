#include "user_firmware_info.h"

#include "ti_msp_dl_config.h"

#include <stdint.h>

#pragma language = extended
#pragma segment = "ALIGNED_ROM"
#pragma segment = "CSTACK"
#pragma segment = "HEAP"

#define USER_FIRMWARE_FLASH_START 0x00000000u
#define USER_FIRMWARE_FLASH_TOTAL (128u * 1024u)
#define USER_FIRMWARE_RAM_START 0x20200000u
#define USER_FIRMWARE_RAM_TOTAL (32u * 1024u)

const char *USER_Firmware_GetVersion(void)
{
    return USER_FIRMWARE_VERSION;
}

const char *USER_Firmware_GetCpuModel(void)
{
    return USER_FIRMWARE_CPU_MODEL;
}

uint32_t USER_Firmware_GetCpuClockHz(void)
{
    return CPUCLK_FREQ;
}

uint32_t USER_Firmware_GetRamTotalBytes(void)
{
    return USER_FIRMWARE_RAM_TOTAL;
}

uint32_t USER_Firmware_GetFlashTotalBytes(void)
{
    return USER_FIRMWARE_FLASH_TOTAL;
}

uint32_t USER_Firmware_GetFlashUsedBytes(void)
{
    uint32_t image_end = (uint32_t)__sfe("ALIGNED_ROM");

    if (image_end <= USER_FIRMWARE_FLASH_START)
    {
        return 0u;
    }

    return image_end - USER_FIRMWARE_FLASH_START;
}

uint32_t USER_Firmware_GetRamUsedBytes(void)
{
    uint32_t msp = __get_MSP();
    uint32_t cstack_start = (uint32_t)__sfb("CSTACK");
    uint32_t cstack_end = (uint32_t)__sfe("CSTACK");
    uint32_t heap_start = (uint32_t)__sfb("HEAP");
    uint32_t heap_end = (uint32_t)__sfe("HEAP");
    uint32_t static_used;
    uint32_t heap_reserved;
    uint32_t stack_used;
    uint32_t total_used;

    if ((cstack_start < USER_FIRMWARE_RAM_START) ||
        (cstack_end < cstack_start) ||
        (heap_end < heap_start))
    {
        return USER_FIRMWARE_RAM_TOTAL;
    }

    static_used = cstack_start - USER_FIRMWARE_RAM_START;
    heap_reserved = heap_end - heap_start;

    if (msp < cstack_start)
    {
        stack_used = cstack_end - cstack_start;
    }
    else if (msp > cstack_end)
    {
        stack_used = 0u;
    }
    else
    {
        stack_used = cstack_end - msp;
    }

    total_used = static_used + heap_reserved + stack_used;
    return (total_used > USER_FIRMWARE_RAM_TOTAL) ? USER_FIRMWARE_RAM_TOTAL : total_used;
}

uint16_t USER_Firmware_GetRamUsedPermille(void)
{
    return (uint16_t)((USER_Firmware_GetRamUsedBytes() * 1000u) /
                      USER_FIRMWARE_RAM_TOTAL);
}

uint16_t USER_Firmware_GetFlashUsedPermille(void)
{
    return (uint16_t)((USER_Firmware_GetFlashUsedBytes() * 1000u) /
                      USER_FIRMWARE_FLASH_TOTAL);
}
