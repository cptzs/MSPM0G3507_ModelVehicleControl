#include "userlib_oemt_dg.h"
#include "user_config.h"

#if OEMT_SENSOR_MODE == OEMT_SENSOR_MODE_DG

/* 数字 8 路模式运行状态 */
static GPTIMER_Regs *oemt_dg_timer = NULL;
static volatile bool oemt_dg_enable = false;
static volatile uint32_t oemt_dg_scan_count = 0;
static volatile uint32_t oemt_dg_scan_rate_real = 0;
static volatile uint16_t oemt_dg_raw_data[OEMT_DG_COUNT];
static volatile uint8_t oemt_dg_scan_channel = 0;
static volatile uint16_t *oemt_dg_result_data[OEMT_DG_COUNT];
static GPIO_Regs *oemt_dg_data_port[OEMT_DG_COUNT];
static uint32_t oemt_dg_data_pin[OEMT_DG_COUNT];

#if OEMT_SENSOR_MODE == OEMT_SENSOR_MODE_DG
/// @brief 读取当前数字输入通道。
/// @return true 表示读取成功。
static bool USER_OEMT_DG_ReadCurrentChannel(void)
{
    uint32_t pin_state;
    uint8_t channel = oemt_dg_scan_channel;

    if (!oemt_dg_enable || channel >= OEMT_DG_COUNT)
    {
        return false;
    }

    pin_state = DL_GPIO_readPins(oemt_dg_data_port[channel],
                                 oemt_dg_data_pin[channel]);
    pin_state = (pin_state & oemt_dg_data_pin[channel]) ? 1U : 0U;
    oemt_dg_raw_data[channel] = (uint16_t)pin_state;
    *oemt_dg_result_data[channel] = (pin_state == 1U) ? OEMT_LIGHT : OEMT_DARK;

    return true;
}

/// @brief 切换到下一路数字输入通道。
/// @return true 表示切换成功。
static bool USER_OEMT_DG_ChangeChannel(void)
{
    if (!oemt_dg_enable)
    {
        return false;
    }

    oemt_dg_scan_channel++;
    if (oemt_dg_scan_channel >= OEMT_DG_COUNT)
    {
        oemt_dg_scan_channel = 0;
    }

    return true;
}
#endif

/// @brief 统计数字 8 路模式实际扫描频率的 SysTick 回调。
static void USER_SysTickCallback_OEMT_DG(void)
{
    oemt_dg_scan_rate_real = oemt_dg_scan_count * 1000U / OEMT_DG_COUNT;
    oemt_dg_scan_count = 0;
}

/// @brief 使能数字 8 路光电扫描。
void USER_OEMT_DG_Enable(void)
{
    oemt_dg_enable = true;
    __NVIC_EnableIRQ(TIMER_OEMT_INST_INT_IRQN);
    DL_Timer_startCounter(oemt_dg_timer);
}

/// @brief 禁用数字 8 路光电扫描。
void USER_OEMT_DG_Disable(void)
{
    oemt_dg_enable = false;
    __NVIC_DisableIRQ(TIMER_OEMT_INST_INT_IRQN);
    DL_Timer_stopCounter(oemt_dg_timer);
}

/// @brief 获取指定通道的数字原始值。
/// @param idx 通道索引，范围 0-7。
/// @return 0 或 1，通道越界时返回 0。
uint16_t USER_OEMT_DG_GetRawData(uint8_t idx)
{
    if (idx >= OEMT_DG_COUNT)
    {
        return 0;
    }

    return oemt_dg_raw_data[idx];
}

/// @brief 获取实际扫描频率。
/// @return 单通道等效扫描频率，单位 Hz。
uint32_t USER_OEMT_DG_GetScanRate(void)
{
    return oemt_dg_scan_rate_real;
}

/// @brief 初始化数字 8 路光电传感器模块。
/// @param result_data 光电状态输出缓存，长度至少为 OEMT_DG_COUNT。
/// @return true 表示初始化成功。
bool USER_OEMT_DG_Init(uint16_t *result_data)
{
    uint8_t i;
    static const uint32_t digital_pins[OEMT_DG_COUNT] = {
        DL_GPIO_PIN_28,
        DL_GPIO_PIN_29,
        DL_GPIO_PIN_30,
        DL_GPIO_PIN_31,
        DL_GPIO_PIN_24,
        DL_GPIO_PIN_25,
        DL_GPIO_PIN_26,
        DL_GPIO_PIN_27,
    };
    static const uint32_t digital_iomux[OEMT_DG_COUNT] = {
        IOMUX_PINCM3,
        IOMUX_PINCM4,
        IOMUX_PINCM5,
        IOMUX_PINCM6,
        IOMUX_PINCM54,
        IOMUX_PINCM55,
        IOMUX_PINCM59,
        IOMUX_PINCM60,
    };

    if (result_data == NULL)
    {
        return false;
    }

    oemt_dg_timer = TIMER_OEMT_INST;
    oemt_dg_scan_channel = 0;
    oemt_dg_scan_count = 0;
    oemt_dg_scan_rate_real = 0;

    for (i = 0; i < OEMT_DG_COUNT; i++)
    {
        DL_GPIO_initDigitalInputFeatures(digital_iomux[i],
                                         DL_GPIO_INVERSION_DISABLE,
                                         DL_GPIO_RESISTOR_NONE,
                                         DL_GPIO_HYSTERESIS_DISABLE,
                                         DL_GPIO_WAKEUP_DISABLE);

        oemt_dg_result_data[i] = &result_data[i];
        oemt_dg_data_port[i] = OEMT_PORT;
        oemt_dg_data_pin[i] = digital_pins[i];
        oemt_dg_raw_data[i] = 0;
    }

    DL_Timer_enableInterrupt(oemt_dg_timer, DL_TIMERG_INTERRUPT_LOAD_EVENT);
    USER_SYSTICK_RegisterCallback(USER_SysTickCallback_OEMT_DG);
    USER_OEMT_DG_Enable();

    return true;
}

#if OEMT_SENSOR_MODE == OEMT_SENSOR_MODE_DG
/// @brief 数字式 OEMT 扫描定时器中断。
void TIMER_OEMT_INST_IRQHandler(void)
{
    DL_TIMER_IIDX itsource = DL_Timer_getPendingInterrupt(oemt_dg_timer);

    if (itsource == DL_TIMER_IIDX_LOAD)
    {
        DL_Timer_clearInterruptStatus(oemt_dg_timer, GPTIMER_CPU_INT_IMASK_L_MASK);
    }
    else
    {
        return;
    }

    USER_OEMT_DG_ReadCurrentChannel();
    USER_OEMT_DG_ChangeChannel();
    oemt_dg_scan_count++;
}
#endif

#else

bool USER_OEMT_DG_Init(uint16_t *result_data)
{
    (void)result_data;
    return false;
}

void USER_OEMT_DG_Enable(void)
{
}

void USER_OEMT_DG_Disable(void)
{
}

uint16_t USER_OEMT_DG_GetRawData(uint8_t idx)
{
    (void)idx;
    return 0;
}

uint32_t USER_OEMT_DG_GetScanRate(void)
{
    return 0;
}

#endif
