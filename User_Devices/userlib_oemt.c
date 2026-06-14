#include "userlib_oemt.h"

enum
{
    OEMT_AN_PIN_ADDR0 = 0,
    OEMT_AN_PIN_ADDR1,
    OEMT_AN_PIN_ADDR2,
};

#define OEMT_AN_DEFAULT_LOW 400U
#define OEMT_AN_DEFAULT_HIGH 1000U
#define OEMT_AN_AUTO_SAMPLE_COUNT 32U

/* 模拟复用模式运行状态 */
static GPTIMER_Regs *oemt_an_timer = NULL;
static volatile bool oemt_an_enable = false;
static volatile uint32_t oemt_an_scan_count = 0;
static volatile uint32_t oemt_an_scan_rate_real = 0;
static volatile uint16_t oemt_an_raw_data[OEMT_AN_COUNT];
static uint32_t oemt_an_raw_data_sum[OEMT_AN_COUNT];
static uint32_t oemt_an_raw_data_average[OEMT_AN_COUNT];

static volatile uint8_t oemt_an_scan_channel = 0;
static volatile uint16_t *oemt_an_result_data[OEMT_AN_COUNT];
static bool oemt_an_initialized[OEMT_AN_COUNT] = {false};

static GPIO_Regs *oemt_an_enable_port = NULL;
static uint32_t oemt_an_enable_pin = 0;
static volatile uint16_t *oemt_an_analog_data = NULL;
static GPIO_Regs *oemt_an_addr_port[3] = {NULL};
static uint32_t oemt_an_addr_pin[3] = {0};
static volatile uint16_t oemt_an_hysteresis_high[OEMT_AN_COUNT];
static volatile uint16_t oemt_an_hysteresis_low[OEMT_AN_COUNT];

/// @brief 切换模拟复用器通道。
/// @param idx 通道索引，范围 0-7。
/// @return true 表示切换成功。
static bool USER_OEMT_SetChannel(uint8_t idx)
{
    if (idx >= OEMT_AN_COUNT)
    {
        return false;
    }

    if ((idx & 0x01U) != 0U)
    {
        DL_GPIO_setPins(oemt_an_addr_port[OEMT_AN_PIN_ADDR0],
                        oemt_an_addr_pin[OEMT_AN_PIN_ADDR0]);
    }
    else
    {
        DL_GPIO_clearPins(oemt_an_addr_port[OEMT_AN_PIN_ADDR0],
                          oemt_an_addr_pin[OEMT_AN_PIN_ADDR0]);
    }

    if ((idx & 0x02U) != 0U)
    {
        DL_GPIO_setPins(oemt_an_addr_port[OEMT_AN_PIN_ADDR1],
                        oemt_an_addr_pin[OEMT_AN_PIN_ADDR1]);
    }
    else
    {
        DL_GPIO_clearPins(oemt_an_addr_port[OEMT_AN_PIN_ADDR1],
                          oemt_an_addr_pin[OEMT_AN_PIN_ADDR1]);
    }

    if ((idx & 0x04U) != 0U)
    {
        DL_GPIO_setPins(oemt_an_addr_port[OEMT_AN_PIN_ADDR2],
                        oemt_an_addr_pin[OEMT_AN_PIN_ADDR2]);
    }
    else
    {
        DL_GPIO_clearPins(oemt_an_addr_port[OEMT_AN_PIN_ADDR2],
                          oemt_an_addr_pin[OEMT_AN_PIN_ADDR2]);
    }

    return true;
}

/// @brief 读取当前模拟复用通道。
/// @return true 表示读取成功。
static bool USER_OEMT_ReadCurrentChannel(void)
{
    uint16_t adc_value;
    uint8_t channel = oemt_an_scan_channel;

    if (!oemt_an_enable || channel >= OEMT_AN_COUNT || !oemt_an_initialized[channel])
    {
        return false;
    }

    adc_value = *oemt_an_analog_data;
    oemt_an_raw_data[channel] = adc_value;

    if (adc_value >= oemt_an_hysteresis_high[channel])
    {
        *oemt_an_result_data[channel] = OEMT_LIGHT;
    }
    else if (adc_value <= oemt_an_hysteresis_low[channel])
    {
        *oemt_an_result_data[channel] = OEMT_DARK;
    }

    return true;
}

/// @brief 切换到下一路模拟复用通道。
/// @return true 表示切换成功。
static bool USER_OEMT_ChangeChannel(void)
{
    if (!oemt_an_enable)
    {
        return false;
    }

    oemt_an_scan_channel++;
    if (oemt_an_scan_channel >= OEMT_AN_COUNT)
    {
        oemt_an_scan_channel = 0;
    }

    return USER_OEMT_SetChannel(oemt_an_scan_channel);
}

/// @brief 统计模拟复用模式实际扫描频率的 SysTick 回调。
static void USER_OEMT_SysTickCallback(void)
{
    oemt_an_scan_rate_real = oemt_an_scan_count * 1000U / OEMT_AN_COUNT;
    oemt_an_scan_count = 0;
}

/// @brief 使能模拟复用光电扫描。
void USER_OEMT_Enable(void)
{
    if (oemt_an_enable_port != NULL && oemt_an_enable_pin != 0U)
    {
        DL_GPIO_clearPins(oemt_an_enable_port, oemt_an_enable_pin);
    }

    oemt_an_enable = true;
    USER_OEMT_SetChannel(oemt_an_scan_channel);
    __NVIC_EnableIRQ(TIMER_OEMT_INST_INT_IRQN);
    DL_Timer_startCounter(oemt_an_timer);
}

/// @brief 禁用模拟复用光电扫描。
void USER_OEMT_Disable(void)
{
    if (oemt_an_enable_port != NULL && oemt_an_enable_pin != 0U)
    {
        DL_GPIO_setPins(oemt_an_enable_port, oemt_an_enable_pin);
    }

    oemt_an_enable = false;
    __NVIC_DisableIRQ(TIMER_OEMT_INST_INT_IRQN);
    DL_Timer_stopCounter(oemt_an_timer);
}

/// @brief 获取指定通道的 ADC 原始值。
/// @param idx 通道索引，范围 0-7。
/// @return ADC 原始值，通道越界时返回 0。
uint16_t USER_OEMT_GetRawData(uint8_t idx)
{
    if (idx >= OEMT_AN_COUNT)
    {
        return 0;
    }

    return oemt_an_raw_data[idx];
}

/// @brief 获取实际扫描频率。
/// @return 单通道等效扫描频率，单位 Hz。
uint32_t USER_OEMT_GetScanRate(void)
{
    return oemt_an_scan_rate_real;
}

/// @brief 设置模拟量高滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @param threshold 高滞回阈值。
void USER_OEMT_SetHysteresisHigh(uint8_t idx, uint16_t threshold)
{
    if (idx >= OEMT_AN_COUNT)
    {
        return;
    }

    if (threshold > oemt_an_hysteresis_low[idx])
    {
        oemt_an_hysteresis_high[idx] = threshold;
    }
    else if (oemt_an_hysteresis_low[idx] < UINT16_MAX)
    {
        oemt_an_hysteresis_high[idx] = oemt_an_hysteresis_low[idx] + 1U;
    }
}

/// @brief 设置模拟量低滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @param threshold 低滞回阈值。
void USER_OEMT_SetHysteresisLow(uint8_t idx, uint16_t threshold)
{
    if (idx >= OEMT_AN_COUNT)
    {
        return;
    }

    if (threshold < oemt_an_hysteresis_high[idx])
    {
        oemt_an_hysteresis_low[idx] = threshold;
    }
    else if (oemt_an_hysteresis_high[idx] > 0U)
    {
        oemt_an_hysteresis_low[idx] = oemt_an_hysteresis_high[idx] - 1U;
    }
}

/// @brief 获取模拟量高滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @return 高滞回阈值，通道越界时返回 0。
uint16_t USER_OEMT_GetHysteresisHigh(uint8_t idx)
{
    if (idx >= OEMT_AN_COUNT)
    {
        return 0;
    }

    return oemt_an_hysteresis_high[idx];
}

/// @brief 获取模拟量低滞回阈值。
/// @param idx 通道索引，范围 0-7。
/// @return 低滞回阈值，通道越界时返回 0。
uint16_t USER_OEMT_GetHysteresisLow(uint8_t idx)
{
    if (idx >= OEMT_AN_COUNT)
    {
        return 0;
    }

    return oemt_an_hysteresis_low[idx];
}

/// @brief 自动设置模拟量高滞回阈值。
/// @note 会采集 32 次当前原始数据，期间产生约 320 ms 阻塞。
void USER_OEMT_AutoSetHysteresisHigh(void)
{
    uint8_t i;
    uint8_t j;

    for (i = 0; i < OEMT_AN_COUNT; i++)
    {
        oemt_an_raw_data_sum[i] = 0;
    }

    for (j = 0; j < OEMT_AN_AUTO_SAMPLE_COUNT; j++)
    {
        for (i = 0; i < OEMT_AN_COUNT; i++)
        {
            oemt_an_raw_data_sum[i] += oemt_an_raw_data[i];
        }
        delay_ms(10);
    }

    for (i = 0; i < OEMT_AN_COUNT; i++)
    {
        uint16_t candidate;

        oemt_an_raw_data_average[i] = oemt_an_raw_data_sum[i] / OEMT_AN_AUTO_SAMPLE_COUNT;
        candidate = (uint16_t)(oemt_an_raw_data_average[i] * 0.7f);
        USER_OEMT_SetHysteresisHigh(i, candidate);
    }
}

/// @brief 自动设置模拟量低滞回阈值。
/// @note 会采集 32 次当前原始数据，期间产生约 320 ms 阻塞。
void USER_OEMT_AutoSetHysteresisLow(void)
{
    uint8_t i;
    uint8_t j;

    for (i = 0; i < OEMT_AN_COUNT; i++)
    {
        oemt_an_raw_data_sum[i] = 0;
    }

    for (j = 0; j < OEMT_AN_AUTO_SAMPLE_COUNT; j++)
    {
        for (i = 0; i < OEMT_AN_COUNT; i++)
        {
            oemt_an_raw_data_sum[i] += oemt_an_raw_data[i];
        }
        delay_ms(10);
    }

    for (i = 0; i < OEMT_AN_COUNT; i++)
    {
        uint16_t candidate;

        oemt_an_raw_data_average[i] = oemt_an_raw_data_sum[i] / OEMT_AN_AUTO_SAMPLE_COUNT;
        candidate = (uint16_t)(oemt_an_raw_data_average[i] * 2.5f);
        USER_OEMT_SetHysteresisLow(i, candidate);
    }
}

/// @brief 初始化模拟复用光电传感器模块。
/// @param result_data 光电状态输出缓存，长度至少为 OEMT_AN_COUNT。
/// @param analog_raw_data ADC 原始值指针。
/// @return true 表示初始化成功。
bool USER_OEMT_Init(uint16_t *result_data, uint16_t *analog_raw_data)
{
    uint8_t i;

    if (result_data == NULL || analog_raw_data == NULL)
    {
        return false;
    }

    oemt_an_timer = TIMER_OEMT_INST;
    oemt_an_enable_port = OEMT_PORT;
    oemt_an_enable_pin = OEMT_EN_PIN;
    oemt_an_analog_data = analog_raw_data;
    oemt_an_addr_port[OEMT_AN_PIN_ADDR0] = OEMT_PORT;
    oemt_an_addr_pin[OEMT_AN_PIN_ADDR0] = OEMT_ADDR0_PIN;
    oemt_an_addr_port[OEMT_AN_PIN_ADDR1] = OEMT_PORT;
    oemt_an_addr_pin[OEMT_AN_PIN_ADDR1] = OEMT_ADDR1_PIN;
    oemt_an_addr_port[OEMT_AN_PIN_ADDR2] = OEMT_PORT;
    oemt_an_addr_pin[OEMT_AN_PIN_ADDR2] = OEMT_ADDR2_PIN;
    oemt_an_scan_channel = 0;
    oemt_an_scan_count = 0;
    oemt_an_scan_rate_real = 0;

    for (i = 0; i < OEMT_AN_COUNT; i++)
    {
        oemt_an_result_data[i] = &result_data[i];
        oemt_an_raw_data[i] = 0;
        oemt_an_hysteresis_low[i] = OEMT_AN_DEFAULT_LOW;
        oemt_an_hysteresis_high[i] = OEMT_AN_DEFAULT_HIGH;
        oemt_an_initialized[i] = true;
    }

    DL_Timer_enableInterrupt(oemt_an_timer, DL_TIMERG_INTERRUPT_LOAD_EVENT);
    USER_OEMT_SetChannel(0);
    USER_SysTick_RegisterCallback(USER_OEMT_SysTickCallback);
    USER_OEMT_Enable();

    return true;
}

/// @brief 模拟式 OEMT 扫描定时器中断。
void TIMER_OEMT_INST_IRQHandler(void)
{
    DL_TIMER_IIDX itsource = DL_Timer_getPendingInterrupt(oemt_an_timer);

    if (itsource == DL_TIMER_IIDX_LOAD)
    {
        DL_Timer_clearInterruptStatus(oemt_an_timer, GPTIMER_CPU_INT_IMASK_L_MASK);
    }
    else
    {
        return;
    }

    USER_OEMT_ReadCurrentChannel();
    USER_OEMT_ChangeChannel();
    oemt_an_scan_count++;
}
