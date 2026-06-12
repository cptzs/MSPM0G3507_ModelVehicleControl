#include "user_ui_internal.h"

/**
 * @file user_ui_page_sensors.c
 * @brief 传感器数据页面集合：Encoder / Photoelectric / ADC / LiDAR / Gyroscope(IMU)。
 *
 * 各页面采用逐行轮询刷新策略，按键交互通过注册表 on_key() 回调处理，
 * 动态刷新函数不再直接读取底层按键。
 */

/* ---- Encoder page state ---- */
static uint8_t s_encoder_update_line = 1u;
static uint8_t s_selected_encoder = 0u;

static const char *const s_encoder_status_str[5] = {
    "OK      ",
    "UNINIT  ",
    "OVERFLOW",
    "NO_PULSE",
    "UNKNOWN ",
};

static const char *USER_UI_EncoderStatusText(uint8_t status)
{
    if (status >= 5u)
    {
        status = 4u;
    }

    return s_encoder_status_str[status];
}

/**
 * @brief 绘制 Encoder 页面静态布局（3 路编码器速度/里程标签）。
 */
void USER_UI_ShowEncoderPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "MO: 000000  UNINIT   ", 21u);
    USER_OLED_putString(2u, 0u, "ML: 000000  UNINIT   ", 21u);
    USER_OLED_putString(3u, 0u, "MR: 000000  UNINIT   ", 21u);
    USER_OLED_putString(4u, 0u, "> SUMO:     000000   ", 21u);
    USER_OLED_putString(5u, 0u, "  SUML:     000000   ", 21u);
    USER_OLED_putString(6u, 0u, "  SUMR:     000000   ", 21u);
}

/**
 * @brief Encoder 页面动态刷新 — 逐行轮询更新 3 路编码器速度和累计里程。
 */
void USER_UI_ShowEncoderPageDynamic(void)
{
    s_encoder_update_line++;
    if (s_encoder_update_line > 7u)
    {
        s_encoder_update_line = 1u;
    }

    switch (s_encoder_update_line)
    {
    case 1u:
        USER_OLED_putI16(1u, 4u, (int16_t)encoder_data[0].speed, 6u);
        USER_OLED_putString(1u, 12u, USER_UI_EncoderStatusText((uint8_t)encoder_data[0].status), 8u);
        break;
    case 2u:
        USER_OLED_putI16(2u, 4u, (int16_t)encoder_data[1].speed, 6u);
        USER_OLED_putString(2u, 12u, USER_UI_EncoderStatusText((uint8_t)encoder_data[1].status), 8u);
        break;
    case 3u:
        USER_OLED_putI16(3u, 4u, (int16_t)encoder_data[2].speed, 6u);
        USER_OLED_putString(3u, 12u, USER_UI_EncoderStatusText((uint8_t)encoder_data[2].status), 8u);
        break;
    case 4u:
        USER_OLED_putI16(4u, 12u, (int16_t)encoder_data[0].sum_distance, 6u);
        break;
    case 5u:
        USER_OLED_putI16(5u, 12u, (int16_t)encoder_data[1].sum_distance, 6u);
        break;
    case 6u:
        USER_OLED_putI16(6u, 12u, (int16_t)encoder_data[2].sum_distance, 6u);
        break;
    default:
        break;
    }
}

/**
 * @brief Encoder 页面按键处理 — UP/DOWN 选择编码器，ENTER 清零累计里程。
 */
void USER_UI_EncoderOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    if ((event != USER_UI_KEY_EVENT_SHORT) && (event != USER_UI_KEY_EVENT_LONG_REPEAT))
    {
        return;
    }

    switch (key)
    {
    case UP:
        USER_OLED_putString((uint8_t)(s_selected_encoder + 4u), 0u, " ", 1u);
        if (s_selected_encoder == 0u)
        {
            s_selected_encoder = ENCODER_COUNT - 1u;
        }
        else
        {
            s_selected_encoder--;
        }
        USER_OLED_putString((uint8_t)(s_selected_encoder + 4u), 0u, ">", 1u);
        break;
    case DOWN:
        USER_OLED_putString((uint8_t)(s_selected_encoder + 4u), 0u, " ", 1u);
        if (s_selected_encoder == (ENCODER_COUNT - 1u))
        {
            s_selected_encoder = 0u;
        }
        else
        {
            s_selected_encoder++;
        }
        USER_OLED_putString((uint8_t)(s_selected_encoder + 4u), 0u, ">", 1u);
        break;
    case ENTER:
        if (event == USER_UI_KEY_EVENT_SHORT)
        {
            encoder_data[s_selected_encoder].sum_distance = 0;
        }
        break;
    default:
        break;
    }
}

/* ---- Photoelectric page state ---- */
static uint8_t s_photo_update_line = 1u;
static uint8_t s_photo_selected_channel = 0u;
static uint8_t s_photo_selected_threshold = 0u;

/**
 * @brief 绘制 Photoelectric 页面静态布局（8 通道状态/原始值/阈值标签）。
 */
void USER_UI_ShowPhotoelectricPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "A:  0 0 0 0 0 0 0 0  ", 21u);
    USER_OLED_putString(2u, 0u, "0000 0000 0000 0000  ", 21u);
    USER_OLED_putString(3u, 0u, "0000 0000 0000 0000  ", 21u);
    USER_OLED_putString(4u, 0u, "Scan Rate:     000 Hz", 21u);
    USER_OLED_putString(5u, 0u, "Select CH:       0   ", 21u);
    USER_OLED_putString(6u, 0u, ">HIGH         0000   ", 21u);
    USER_OLED_putString(7u, 0u, " LOW          0000   ", 21u);
}

/**
 * @brief Photoelectric 页面动态刷新 — 逐行更新通道状态、原始 ADC 值、扫描率和阈值。
 */
void USER_UI_ShowPhotoelectricPageDynamic(void)
{
    s_photo_update_line++;
    if (s_photo_update_line > 7u)
    {
        s_photo_update_line = 1u;
    }

    switch (s_photo_update_line)
    {
    case 1u:
        USER_OLED_putUI16(1u, 4u, oemt_data[0], 1u);
        USER_OLED_putUI16(1u, 6u, oemt_data[1], 1u);
        USER_OLED_putUI16(1u, 8u, oemt_data[2], 1u);
        USER_OLED_putUI16(1u, 10u, oemt_data[3], 1u);
        USER_OLED_putUI16(1u, 12u, oemt_data[4], 1u);
        USER_OLED_putUI16(1u, 14u, oemt_data[5], 1u);
        USER_OLED_putUI16(1u, 16u, oemt_data[6], 1u);
        USER_OLED_putUI16(1u, 18u, oemt_data[7], 1u);
        break;
    case 2u:
        USER_OLED_putUI16(2u, 0u, USER_OEMT_AN_GetRawData(0), 4u);
        USER_OLED_putUI16(2u, 5u, USER_OEMT_AN_GetRawData(1), 4u);
        USER_OLED_putUI16(2u, 10u, USER_OEMT_AN_GetRawData(2), 4u);
        USER_OLED_putUI16(2u, 15u, USER_OEMT_AN_GetRawData(3), 4u);
        break;
    case 3u:
        USER_OLED_putUI16(3u, 0u, USER_OEMT_AN_GetRawData(4), 4u);
        USER_OLED_putUI16(3u, 5u, USER_OEMT_AN_GetRawData(5), 4u);
        USER_OLED_putUI16(3u, 10u, USER_OEMT_AN_GetRawData(6), 4u);
        USER_OLED_putUI16(3u, 15u, USER_OEMT_AN_GetRawData(7), 4u);
        break;
    case 4u:
        USER_OLED_putUI16(4u, 14u, USER_OEMT_AN_GetScanRate(), 4u);
        break;
    case 5u:
        USER_OLED_putUI16(5u, 17u, s_photo_selected_channel, 1u);
        break;
    case 6u:
        USER_OLED_putUI16(6u, 14u, USER_OEMT_AN_GetHysteresisHigh(s_photo_selected_channel), 4u);
        break;
    case 7u:
        USER_OLED_putUI16(7u, 14u, USER_OEMT_AN_GetHysteresisLow(s_photo_selected_channel), 4u);
        break;
    default:
        break;
    }
}

/**
 * @brief Photoelectric 页面按键处理 — LEFT/RIGHT 选通道，UP/DOWN 选阈值类型，ENTER/ESC 调整阈值。
 */
void USER_UI_PhotoelectricOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    uint16_t threshold;

    switch (key)
    {
    case LEFT:
        if ((event == USER_UI_KEY_EVENT_SHORT) || (event == USER_UI_KEY_EVENT_LONG_REPEAT))
        {
            s_photo_selected_channel = (s_photo_selected_channel == 0u) ? (uint8_t)(OEMT_AN_COUNT - 1u) : (uint8_t)(s_photo_selected_channel - 1u);
        }
        break;
    case RIGHT:
        if ((event == USER_UI_KEY_EVENT_SHORT) || (event == USER_UI_KEY_EVENT_LONG_REPEAT))
        {
            s_photo_selected_channel = (s_photo_selected_channel == (OEMT_AN_COUNT - 1u)) ? 0u : (uint8_t)(s_photo_selected_channel + 1u);
        }
        break;
    case UP:
    case DOWN:
        if ((event == USER_UI_KEY_EVENT_SHORT) || (event == USER_UI_KEY_EVENT_LONG_REPEAT))
        {
            USER_OLED_putString((uint8_t)(s_photo_selected_threshold + 6u), 0u, " ", 1u);
            s_photo_selected_threshold = (s_photo_selected_threshold == 0u) ? 1u : 0u;
            USER_OLED_putString((uint8_t)(s_photo_selected_threshold + 6u), 0u, ">", 1u);
        }
        break;
    case ENTER:
        if (event == USER_UI_KEY_EVENT_SHORT)
        {
            if (s_photo_selected_threshold == 1u)
            {
                USER_OEMT_AN_SetHysteresisLow(s_photo_selected_channel,
                                              (uint16_t)(USER_OEMT_AN_GetHysteresisLow(s_photo_selected_channel) + 50u));
            }
            else
            {
                USER_OEMT_AN_SetHysteresisHigh(s_photo_selected_channel,
                                               (uint16_t)(USER_OEMT_AN_GetHysteresisHigh(s_photo_selected_channel) + 50u));
            }
        }
        else if (event == USER_UI_KEY_EVENT_LONG)
        {
            USER_OEMT_AN_AutoSetHysteresisHigh();
        }
        break;
    case ESC:
        if (event == USER_UI_KEY_EVENT_SHORT)
        {
            if (s_photo_selected_threshold == 1u)
            {
                threshold = USER_OEMT_AN_GetHysteresisLow(s_photo_selected_channel);
                USER_OEMT_AN_SetHysteresisLow(s_photo_selected_channel, (threshold > 50u) ? (uint16_t)(threshold - 50u) : 0u);
            }
            else
            {
                threshold = USER_OEMT_AN_GetHysteresisHigh(s_photo_selected_channel);
                USER_OEMT_AN_SetHysteresisHigh(s_photo_selected_channel, (threshold > 50u) ? (uint16_t)(threshold - 50u) : 0u);
            }
        }
        else if (event == USER_UI_KEY_EVENT_LONG)
        {
            USER_OEMT_AN_AutoSetHysteresisLow();
        }
        break;
    default:
        break;
    }
}

/* ---- ADC page ---- */
static uint8_t s_adc_update_line = 1u;

/**
 * @brief 绘制 ADC 页面静态布局（7 通道原始值/电压/温度标签）。
 */
void USER_UI_ShowAdcPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "P27:  0000    0000 mV", 21u);
    USER_OLED_putString(2u, 0u, "P26:  0000    0000 mV", 21u);
    USER_OLED_putString(3u, 0u, "P25:  0000    0000 mV", 21u);
    USER_OLED_putString(4u, 0u, "P24:  0000    0000 mV", 21u);
    USER_OLED_putString(5u, 0u, "POT:  0000     000  %", 21u);
    USER_OLED_putString(6u, 0u, "TMP:  0000          C", 21u);
    USER_OLED_putString(7u, 0u, "VDD:  0000    0000 mV", 21u);
}

/**
 * @brief ADC 页面动态刷新 — 逐行更新各通道原始值和换算电压/温度/电位器。
 */
void USER_UI_ShowAdcPageDynamic(void)
{
    s_adc_update_line++;
    if (s_adc_update_line > 7u)
    {
        s_adc_update_line = 1u;
    }

    switch (s_adc_update_line)
    {
    case 1u:
        USER_OLED_putUI16(1u, 6u, adc_data[ADC_CHANNEL_0_P27], 4u);
        USER_OLED_putUI16(1u, 14u, adc_voltage[ADC_CHANNEL_0_P27], 4u);
        break;
    case 2u:
        USER_OLED_putUI16(2u, 6u, adc_data[ADC_CHANNEL_1_P26], 4u);
        USER_OLED_putUI16(2u, 14u, adc_voltage[ADC_CHANNEL_1_P26], 4u);
        break;
    case 3u:
        USER_OLED_putUI16(3u, 6u, adc_data[ADC_CHANNEL_2_P25], 4u);
        USER_OLED_putUI16(3u, 14u, adc_voltage[ADC_CHANNEL_2_P25], 4u);
        break;
    case 4u:
        USER_OLED_putUI16(4u, 6u, adc_data[ADC_CHANNEL_3_P24], 4u);
        USER_OLED_putUI16(4u, 14u, adc_voltage[ADC_CHANNEL_3_P24], 4u);
        break;
    case 5u:
        USER_OLED_putUI16(5u, 6u, adc_data[ADC_CHANNEL_4_P22], 4u);
        USER_OLED_putUI16(5u, 15u, pot_position, 3u);
        break;
    case 6u:
        USER_OLED_putUI16(6u, 6u, adc_data[ADC_CHANNEL_11_TEMP], 4u);
        USER_OLED_putFloat(6u, 12u, cpu_core_temperature, 4u, 1u);
        break;
    case 7u:
        USER_OLED_putUI16(7u, 6u, adc_data[ADC_CHANNEL_15_PWR], 4u);
        USER_OLED_putUI16(7u, 14u, power_voltage, 4u);
        break;
    default:
        break;
    }
}

/* ---- LiDAR page state ---- */
static uint8_t s_lidar_update_line = 1u;
static uint8_t s_selected_lidar = 1u;

/**
 * @brief 绘制 LiDAR 页面静态布局（4 路距离/状态 + 选中传感器详情标签）。
 */
void USER_UI_ShowLidarPageStatic(void)
{
    USER_OLED_putString(1u, 0u, "L1: 00000  STA  000  ", 21u);
    USER_OLED_putString(2u, 0u, "L2: 00000  STA  000  ", 21u);
    USER_OLED_putString(3u, 0u, "L3: 00000  STA  000  ", 21u);
    USER_OLED_putString(4u, 0u, "L4: 00000  STA  000  ", 21u);
    USER_OLED_putString(5u, 0u, "Select: LIDAR 1      ", 21u);
    USER_OLED_putString(6u, 0u, "TX      0  RX:     0 ", 21u);
    USER_OLED_putString(7u, 0u, "TO:     0  RF:     0 ", 21u);
}

/**
 * @brief LiDAR 页面动态刷新 — 逐行更新 4 路距离和选中传感器的 TX/RX/超时统计。
 */
void USER_UI_ShowLidarPageDynamic(void)
{
    s_lidar_update_line++;
    if (s_lidar_update_line > 7u)
    {
        s_lidar_update_line = 1u;
    }

    switch (s_lidar_update_line)
    {
    case 1u:
        USER_OLED_putUI16(1u, 4u, (uint16_t)lidar_data[1].distance, 5u);
        USER_OLED_putUI16(1u, 16u, (uint16_t)lidar_data[1].status, 3u);
        break;
    case 2u:
        USER_OLED_putUI16(2u, 4u, (uint16_t)lidar_data[2].distance, 5u);
        USER_OLED_putUI16(2u, 16u, (uint16_t)lidar_data[2].status, 3u);
        break;
    case 3u:
        USER_OLED_putUI16(3u, 4u, (uint16_t)lidar_data[3].distance, 5u);
        USER_OLED_putUI16(3u, 16u, (uint16_t)lidar_data[3].status, 3u);
        break;
    case 4u:
        USER_OLED_putUI16(4u, 4u, (uint16_t)lidar_data[4].distance, 5u);
        USER_OLED_putUI16(4u, 16u, (uint16_t)lidar_data[4].status, 3u);
        break;
    case 5u:
        USER_OLED_putUI16(5u, 14u, (uint16_t)s_selected_lidar, 1u);
        break;
    case 6u:
        USER_OLED_putUI16(6u, 4u, lidar_data[s_selected_lidar].tx_count, 5u);
        USER_OLED_putUI16(6u, 16u, lidar_data[s_selected_lidar].rx_count, 5u);
        break;
    case 7u:
        USER_OLED_putUI16(7u, 4u, lidar_data[s_selected_lidar].timeout_count, 5u);
        USER_OLED_putUI16(7u, 16u, lidar_data[s_selected_lidar].strength, 5u);
        break;
    default:
        break;
    }
}

/**
 * @brief LiDAR 页面按键处理 — UP/DOWN 切换选中的 LiDAR 传感器编号。
 */
void USER_UI_LidarOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    if ((event != USER_UI_KEY_EVENT_SHORT) && (event != USER_UI_KEY_EVENT_LONG_REPEAT))
    {
        return;
    }

    switch (key)
    {
    case UP:
        s_selected_lidar++;
        if (s_selected_lidar > 4u)
        {
            s_selected_lidar = 1u;
        }
        break;
    case DOWN:
        if (s_selected_lidar <= 1u)
        {
            s_selected_lidar = 4u;
        }
        else
        {
            s_selected_lidar--;
        }
        break;
    default:
        break;
    }
}

/* ---- Gyroscope / IMU page ---- */
static uint8_t s_gyro_update_line = 1u;

/**
 * @brief 绘制 Gyroscope/IMU 页面静态布局（加速度/角速度/姿态角/温度/状态标签）。
 */
void USER_UI_ShowGyroscopePageStatic(void)
{
    USER_OLED_putString(1u, 0u, "      X      Y      Z", 21u);
    USER_OLED_putString(2u, 0u, "a 00000  00000  00000", 21u);
    USER_OLED_putString(3u, 0u, "w 00000  00000  00000", 21u);
    USER_OLED_putString(4u, 0u, "o 00000  00000  00000", 21u);
    USER_OLED_putString(5u, 0u, "Temperature     00000", 21u);
    USER_OLED_putString(6u, 0u, "Status          00000", 21u);
    USER_OLED_putString(7u, 0u, "TX : 00000 RX : 00000", 21u);
}

/**
 * @brief Gyroscope/IMU 页面动态刷新 — 逐行更新三轴加速度/角速度/姿态角和通信统计。
 */
void USER_UI_ShowGyroscopePageDynamic(void)
{
    s_gyro_update_line++;
    if (s_gyro_update_line > 7u)
    {
        s_gyro_update_line = 1u;
    }

    switch (s_gyro_update_line)
    {
    case 2u:
        USER_OLED_putFloat(2u, 2u, imu_data.accx, 4u, 1u);
        USER_OLED_putFloat(2u, 8u, imu_data.accy, 4u, 1u);
        USER_OLED_putFloat(2u, 15u, imu_data.accz, 4u, 1u);
        break;
    case 3u:
        USER_OLED_putFloat(3u, 2u, imu_data.gyrox, 4u, 1u);
        USER_OLED_putFloat(3u, 8u, imu_data.gyroy, 4u, 1u);
        USER_OLED_putFloat(3u, 15u, imu_data.gyroz, 4u, 1u);
        break;
    case 4u:
        USER_OLED_putFloat(4u, 2u, imu_data.roll, 4u, 1u);
        USER_OLED_putFloat(4u, 8u, imu_data.pitch, 4u, 1u);
        USER_OLED_putFloat(4u, 15u, imu_data.yaw, 4u, 1u);
        break;
    case 5u:
        USER_OLED_putFloat(5u, 15u, imu_data.temperature, 3u, 2u);
        break;
    case 6u:
        USER_OLED_putUI16(6u, 16u, (uint16_t)imu_data.status, 5u);
        break;
    case 7u:
        USER_OLED_putUI16(7u, 5u, imu_data.tx_count, 5u);
        USER_OLED_putUI16(7u, 16u, imu_data.rx_count, 5u);
        break;
    default:
        break;
    }
}

/**
 * @brief Gyroscope 页面按键处理 — ENTER 设角度参考，ESC 设偏航参考。
 */
void USER_UI_GyroscopeOnKey(Button_t key, USER_UI_KeyEvent_t event)
{
    if (event != USER_UI_KEY_EVENT_SHORT)
    {
        return;
    }

    switch (key)
    {
    case ENTER:
        USER_IMU_SetOrder(IMU_ODR_SETANGREF);
        break;
    case ESC:
        USER_IMU_SetOrder(IMU_ODR_SETYAWREF);
        break;
    default:
        break;
    }
}
