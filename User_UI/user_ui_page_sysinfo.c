#include "user_ui_internal.h"

#include "user_firmware_info.h"
#include "user_os.h"
#include "userlib_oled.h"

static void USER_UI_SysInfoPutPercent(uint8_t row, uint8_t column, uint16_t permille)
{
    uint16_t percent_x10;

    percent_x10 = (permille > 1000u) ? 1000u : permille;
    USER_OLED_PutUI16(row, column, (uint16_t)(percent_x10 / 10u), 3u);
    USER_OLED_PutString(row, (uint8_t)(column + 3u), ".", 1u);
    USER_OLED_PutUI16(row, (uint8_t)(column + 4u), (uint16_t)(percent_x10 % 10u), 1u);
    USER_OLED_PutString(row, (uint8_t)(column + 5u), "%", 1u);
}

void USER_UI_ShowSysInfoPageStatic(void)
{
    USER_OLED_PutString(1u, 0u, "CPU:                ", 21u);
    USER_OLED_PutString(2u, 0u, "CLK:        MHz     ", 21u);
    USER_OLED_PutString(3u, 0u, "LOAD:000.0%          ", 21u);
    USER_OLED_PutString(4u, 0u, "RAM:000.0% 000/032K  ", 21u);
    USER_OLED_PutString(5u, 0u, "FLS:000.0% 000/128K  ", 21u);
    USER_OLED_PutString(6u, 0u, "FW:                 ", 21u);
}

void USER_UI_ShowSysInfoPageDynamic(void)
{
    uint32_t clock_mhz;
    uint32_t ram_used_kb;
    uint32_t flash_used_kb;

    clock_mhz = USER_Firmware_GetCpuClockHz() / 1000000u;
    ram_used_kb = (USER_Firmware_GetRamUsedBytes() + 1023u) / 1024u;
    flash_used_kb = (USER_Firmware_GetFlashUsedBytes() + 1023u) / 1024u;

    USER_OLED_PutString(1u, 5u, USER_Firmware_GetCpuModel(), 12u);
    USER_OLED_PutUI16(2u, 5u, (uint16_t)clock_mhz, 3u);
    USER_UI_SysInfoPutPercent(3u, 5u, USER_OS_GetCpuLoadPermille());

    USER_UI_SysInfoPutPercent(4u, 4u, USER_Firmware_GetRamUsedPermille());
    USER_OLED_PutUI16(4u, 11u, (uint16_t)ram_used_kb, 3u);

    USER_UI_SysInfoPutPercent(5u, 4u, USER_Firmware_GetFlashUsedPermille());
    USER_OLED_PutUI16(5u, 11u, (uint16_t)flash_used_kb, 3u);

    USER_OLED_PutString(6u, 4u, USER_Firmware_GetVersion(), 10u);
}
