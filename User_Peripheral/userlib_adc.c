#include "userlib_adc.h"

uint16_t *adc_data_buffer = 0; // ADC采样结果缓冲区
uint16_t channel_count = 0;    // ADC通道数量

/// @brief ADC初始化函数
/// @param data_buffer 存储ADC采样结果的缓冲区
/// @param channel_count ADC通道数量
void USER_ADC_Init(uint16_t *pbuffer, uint16_t ch_count)
{
    // 设置ADC采样结果缓冲区
    adc_data_buffer = pbuffer;
    // 设置ADC通道数量
    channel_count = ch_count;

    // 启用ADC中断
    NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);
    // 启用ADC转换
    DL_ADC12_startConversion(ADC12_0_INST);
}

/// @brief ADC反初始化函数
/// @param None
void USER_ADC_DeInit(void)
{

    // 禁用ADC中断
    NVIC_DisableIRQ(ADC12_0_INST_INT_IRQN);
    // 停止ADC转换
    DL_ADC12_stopConversion(ADC12_0_INST);

    // 清除ADC采样结果缓冲区
    adc_data_buffer = 0;
    channel_count = 0;
}

/// @brief 获取内部温度值
/// @param rawdata ADC原始采样值
/// @return 转换后的温度值，单位°C
float USER_ADC_GetInnerTemperature(uint16_t rawdata)
{
    float vsample;     /* 当前ADC结果转换的电压值，单位mV */
    float vtrim;       /* 校准电压值，单位mV */
    float temperature; /* 计算得到的温度值，单位°C */
    uint32_t trim_raw; /* 校准时的ADC原始值 */

    /* 将当前ADC原始值转换为电压值(mV)
     * 注意：温度传感器使用内部1.4V参考电压，不可变更 */
    vsample = ((float)rawdata * 1400.0f) / 4096.0f;

    /* 获取工厂校准时的ADC原始值 */
    trim_raw = DL_FactoryRegion_getTemperatureVoltage();

    /* 将校准ADC原始值转换为电压值(mV)
     * 注意：校准值是基于12位ADC和1.4V内部参考电压 */
    vtrim = ((float)trim_raw * 1400.0f) / 4096.0f;

    /* 根据正确公式计算温度：TSAMPLE = TSTRIM + (VSAMPLE - VTRIM) / TSc
     * 其中：
     * TSc = -1.8mV/°C (温度系数，负值表示温度升高时电压下降)
     * VSAMPLE = 当前采样电压
     * VTRIM = 校准电压
     * TSTRIM = 30°C (校准参考温度)
     */
    temperature = TEMP_TRIM_REFERENCE + (vsample - vtrim) / TEMP_SENSOR_TC;

    return temperature;
}

/// @brief 获取电源电压值
/// @param rawdata ADC原始采样值
/// @return 转换后的电压值，单位mv
uint16_t USER_ADC_GetPowerVoltage(uint16_t rawdata)
{
    float voltage_measured; /* ADC测量到的电压值，单位mV */
    uint16_t vdd_voltage;   /* 计算得到的VDD电压值，单位mV */
    /* 使用外部参考电压进行计算 */
    voltage_measured = ((float)rawdata * ADC_REF_VOLTAGE) / 4096.0f;
    /* 由于ADC15测量的是VDD/3，所以实际VDD电压需要乘以3 */
    vdd_voltage = (uint16_t)(voltage_measured * 3.0f);
    return vdd_voltage;
}

/// @brief 将ADC采样值转换为电压值
/// @param rawdata ADC原始采样值
/// @return 转换后的电压值，单位mv
uint16_t USER_ADC_ValueToVoltage(uint16_t rawdata)
{
    return (uint16_t)((float)rawdata * (ADC_REF_VOLTAGE / 4096.0f)); // 假设ADC分辨率为12位，参考电压为3.3V
}

/// @brief ADC中断处理函数
/// @param None
void ADC12_0_INST_IRQHandler(void)
{

    uint16_t i;
    // 获取ADC中断状态
    DL_ADC12_IIDX pendingInterrupt = DL_ADC12_getPendingInterrupt(ADC12_0_INST);

    if (pendingInterrupt == DL_ADC12_IIDX_MEM0_RESULT_LOADED + channel_count - 1)
    {
        // 处理ADC采样结果
        for (i = 0; i < channel_count; i++)
        {
            adc_data_buffer[i] = DL_ADC12_getMemResult(ADC12_0_INST, i);
        }
    }
}

// 注：ADC12模块全部中断源
//  DL_ADC12_IIDX_WINDOW_COMP_LOW
//  DL_ADC12_IIDX_INIFG
//  DL_ADC12_IIDX_DMA_DONE
//  DL_ADC12_IIDX_UNDERFLOW
//  DL_ADC12_IIDX_MEM0_RESULT_LOADED
//  DL_ADC12_IIDX_MEM1_RESULT_LOADED
//  DL_ADC12_IIDX_MEM2_RESULT_LOADED
//  DL_ADC12_IIDX_MEM3_RESULT_LOADED
//  DL_ADC12_IIDX_MEM4_RESULT_LOADED
//  DL_ADC12_IIDX_MEM5_RESULT_LOADED
//  DL_ADC12_IIDX_MEM6_RESULT_LOADED
//  DL_ADC12_IIDX_MEM7_RESULT_LOADED
//  DL_ADC12_IIDX_MEM8_RESULT_LOADED
//  DL_ADC12_IIDX_MEM9_RESULT_LOADED
//  DL_ADC12_IIDX_MEM10_RESULT_LOADED
//  DL_ADC12_IIDX_MEM11_RESULT_LOADED
