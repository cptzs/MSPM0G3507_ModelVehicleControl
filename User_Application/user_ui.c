#include "user_ui.h"

/**
 * @file user_ui.c
 * @brief 本地操作界面：OLED 多页面显示、按键导航、比赛路线选择与启动倒计时。
 *
 * 本模块是唯一直接操作 OLED、LED、蜂鸣器的应用层模块。
 * 比赛路线选择、长按启动、倒计时显示、蜂鸣器提醒均由本模块统一管理。
 * race 模块不包含任何 UI 代码。
 */

/* ---- 页面标题 ---- */
static const char *const page_titles[DISPLAY_PAGE_COUNT] = {
    "Motor Control",
    "Encoder Data ",
    "Photo Sensors",
    "ADC Data     ",
    "LiDAR Sensors",
    "IMU Data     ",
    "Smart Camera ",
    "Servo Control",
    "Debug Info   ",
    "IMU Sum Data ",
    "Template Path"};

/* ---- 比赛启动交互状态 ---- */
#define UI_CHARGE_TIME_MS 2000u /* 长按蓄力时长：2 秒 */
#define UI_COUNTDOWN_MS 1000u   /* 蓄力完成后的倒计时：1 秒 */
#define UI_CIRCLE_CENTER_X 96u  /* 蓄力圆圆心 X */
#define UI_CIRCLE_CENTER_Y 28u  /* 蓄力圆圆心 Y */
#define UI_CIRCLE_RADIUS 14u    /* 蓄力圆半径 */

static bool ui_charging_active = false;            /* ENTER 长按蓄力是否进行中 */
static bool ui_countdown_active = false;           /* 倒计时是否正在进行 */
static uint8_t ui_pending_route = RACE_ROUTE_NONE; /* 待启动的路线编号 */
static uint16_t ui_countdown_remain_ms = 0u;       /* 倒计时剩余 ms（软件计时，不依赖 LED3） */

/**
 * @brief UI 模块周期任务。
 *
 * 由主循环每 5 ms 调用一次。优先处理蓄力/倒计时状态（接管整个屏幕）；
 * 无交互时执行正常页面导航与数据刷新。
 */
void USER_UI_Task(void)
{
    static DisplayPage_t current_page = PAGE_MOTOR; /* 当前显示页面 */
    static bool page_static_updated = false;        /* 页面固定内容是否已更新 */

    /* ---- 蓄力/倒计时优先：期间接管整个屏幕，禁止页面切换 ---- */

    /* 蓄力阶段：ENTER 被持续按住，圆形逐圈填充 */
    if (ui_charging_active)
    {
        /* 检查 ENTER 是否仍然按住 */
        if (button_press_time[ENTER] == 0u)
        {
            /* 提前松开：取消蓄力，恢复页面 */
            ui_charging_active = false;
            USER_OLED_CleanScreen();
            page_static_updated = false;
        }
        else if (button_press_time[ENTER] >= UI_CHARGE_TIME_MS)
        {
            /* 蓄力满 2 秒：进入倒计时阶段 */
            ui_charging_active = false;
            ui_countdown_active = true;
            ui_countdown_remain_ms = UI_COUNTDOWN_MS;
        }
        /* 蓄力未满且仍在按住：由 ShowTemplateDynamic 绘制动画 */
    }

    /* 倒计时阶段：圆形已满，下方显示毫秒倒计时 */
    if (ui_countdown_active)
    {
        if (ui_countdown_remain_ms > 0u)
        {
            /* 每 5 ms 周期递减软件倒计时 */
            if (ui_countdown_remain_ms >= 5u)
                ui_countdown_remain_ms -= 5u;
            else
                ui_countdown_remain_ms = 0u;
        }

        if (ui_countdown_remain_ms == 0u)
        {
            /* 倒计时结束：启动路线，退出交互模式 */
            USER_Race_RequestStart(ui_pending_route);
            ui_countdown_active = false;
            ui_pending_route = RACE_ROUTE_NONE;
            USER_OLED_CleanScreen();
            page_static_updated = false;
        }
        /* 倒计时未结束：由 ShowTemplateDynamic 刷新显示 */
    }

    /* 蓄力或倒计时期间，跳过正常页面导航 */
    if (ui_charging_active || ui_countdown_active)
    {
        /* 仍然调用当前页面绘制函数以刷新动态内容 */
        if (!page_static_updated)
        {
            USER_UI_ShowStaticContent(current_page);
            page_static_updated = true;
        }
        USER_UI_ShowDynamicContent(current_page);
        return;
    }

    /* ---- 正常页面导航 ---- */

    /* 检测 PREV 按钮，切换到上一页 */
    ButtonState_t prev_state = USER_LBB_Button_ReadState(PREV);
    if (prev_state == PRESSED)
    {
        current_page--;
        if (current_page < PAGE_MOTOR)
            current_page = PAGE_TEMPLATE; /* 循环到模板路径页 */
        USER_OLED_CleanScreen();
        page_static_updated = false;
    }

    /* 检测 NEXT 按钮，切换到下一页 */
    ButtonState_t next_state = USER_LBB_Button_ReadState(NEXT);
    if (next_state == PRESSED)
    {
        current_page++;
        if (current_page > PAGE_TEMPLATE)
            current_page = PAGE_MOTOR; /* 循环回电机页 */
        USER_OLED_CleanScreen();
        page_static_updated = false;
    }

    /* 页面切换后首次绘制静态内容 */
    if (!page_static_updated)
    {
        USER_UI_ShowStaticContent(current_page);
        page_static_updated = true;
    }

    /* 每周期刷新动态内容 */
    USER_UI_ShowDynamicContent(current_page);
}

/**
 * @brief 显示指定页面的静态内容（标题、标签等）。
 * @param page 页面编号。
 */
void USER_UI_ShowStaticContent(DisplayPage_t page)
{
    /* 第 0 行：页面标题 */
    USER_OLED_putString(0, 0, "[", 1);
    USER_OLED_putUI16(0, 1, page, 2);
    USER_OLED_putString(0, 3, "] ", 2);
    USER_OLED_putString(0, 5, page_titles[page - 1], 13);

    switch (page)
    {
    case PAGE_MOTOR:
        USER_UI_ShowMotorStatic();
        break;
    case PAGE_ENCODER:
        USER_UI_ShowEncoderStatic();
        break;
    case PAGE_PHOTOELECTRIC:
        USER_UI_ShowPhotoelectricStatic();
        break;
    case PAGE_ADC:
        USER_UI_ShowAdcStatic();
        break;
    case PAGE_LIDAR:
        USER_UI_ShowLidarStatic();
        break;
    case PAGE_GYROSCOPE:
        USER_UI_ShowGyroscopeStatic();
        break;
    case PAGE_CAMERA:
        USER_UI_ShowCameraStatic();
        break;
    case PAGE_SERVO:
        USER_UI_ShowServoStatic();
        break;
    case PAGE_DEBUG:
        USER_UI_ShowDebugStatic();
        break;
    case PAGE_IMU_SUM:
        USER_UI_ShowIMUSumStatic();
        break;
    case PAGE_TEMPLATE:
        USER_UI_ShowTemplateStatic();
        break;
    default:
        break;
    }
}

/**
 * @brief 显示指定页面的动态内容（数据部分）。
 * @param page 页面编号。
 */
void USER_UI_ShowDynamicContent(DisplayPage_t page)
{
    switch (page)
    {
    case PAGE_MOTOR:
        USER_UI_ShowMotorDynamic();
        break;
    case PAGE_ENCODER:
        USER_UI_ShowEncoderDynamic();
        break;
    case PAGE_PHOTOELECTRIC:
        USER_UI_ShowPhotoelectricDynamic();
        break;
    case PAGE_ADC:
        USER_UI_ShowAdcDynamic();
        break;
    case PAGE_LIDAR:
        USER_UI_ShowLidarDynamic();
        break;
    case PAGE_GYROSCOPE:
        USER_UI_ShowGyroscopeDynamic();
        break;
    case PAGE_CAMERA:
        USER_UI_ShowCameraDynamic();
        break;
    case PAGE_SERVO:
        USER_UI_ShowServoDynamic();
        break;
    case PAGE_DEBUG:
        USER_UI_ShowDebugDynamic();
        break;
    case PAGE_IMU_SUM:
        USER_UI_ShowIMUSumDynamic();
        break;
    case PAGE_TEMPLATE:
        USER_UI_ShowTemplateDynamic();
        break;
    default:
        break;
    }
}

/* ================================================================
 *  第 1 页：电机控制
 * ================================================================ */

void USER_UI_ShowMotorStatic(void)
{
    USER_OLED_putString(1, 0, "MODE    00000   00000", 21); /* 左右电机模式 */
    USER_OLED_putString(2, 0, "REQU    00000   00000", 21); /* 左右目标速度 */
    USER_OLED_putString(3, 0, "REAL    00000   00000", 21); /* 左右实际速度 */
    USER_OLED_putString(4, 0, "ERR     00000   00000", 21); /* 左右误差 */
    USER_OLED_putString(5, 0, "ESUM    00000   00000", 21); /* 左右积分 */
    USER_OLED_putString(6, 0, "EDIV    00000   00000", 21); /* 左右微分 */
    USER_OLED_putString(7, 0, "PWM     00000   00000", 21); /* 左右PWM占空比 */
}

void USER_UI_ShowMotorDynamic(void)
{
    static uint8_t update_line = 1;

    update_line++;
    if (update_line > 7)
        update_line = 1;

    switch (update_line)
    {
    case 1:
        USER_OLED_putUI16(1, 8, USER_Motor_GetMode(MOTOR_0_LEFT), 5);
        USER_OLED_putUI16(1, 16, USER_Motor_GetMode(MOTOR_1_RIGHT), 5);
        break;
    case 2:
        USER_OLED_putI16(2, 8, (int16_t)speed_pid[MOTOR_0_LEFT].target, 5);
        USER_OLED_putI16(2, 16, (int16_t)speed_pid[MOTOR_1_RIGHT].target, 5);
        break;
    case 3:
        USER_OLED_putI16(3, 8, (int16_t)speed_pid[MOTOR_0_LEFT].current, 5);
        USER_OLED_putI16(3, 16, (int16_t)speed_pid[MOTOR_1_RIGHT].current, 5);
        break;
    case 4:
        USER_OLED_putI16(4, 8, (int16_t)speed_pid[MOTOR_0_LEFT].error, 5);
        USER_OLED_putI16(4, 16, (int16_t)speed_pid[MOTOR_1_RIGHT].error, 5);
        break;
    case 5:
        USER_OLED_putI16(5, 8, (int16_t)speed_pid[MOTOR_0_LEFT].integral, 5);
        USER_OLED_putI16(5, 16, (int16_t)speed_pid[MOTOR_1_RIGHT].integral, 5);
        break;
    case 6:
        USER_OLED_putI16(6, 8, (int16_t)speed_pid[MOTOR_0_LEFT].derivative, 5);
        USER_OLED_putI16(6, 16, (int16_t)speed_pid[MOTOR_1_RIGHT].derivative, 5);
        break;
    case 7:
        USER_OLED_putI16(7, 8, (int16_t)speed_pid[MOTOR_0_LEFT].output, 5);
        USER_OLED_putI16(7, 16, (int16_t)speed_pid[MOTOR_1_RIGHT].output, 5);
        break;
    }
}

/* ================================================================
 *  第 2 页：编码器数据
 * ================================================================ */

void USER_UI_ShowEncoderStatic(void)
{
    USER_OLED_putString(1, 0, "MO: 000000  UNINIT   ", 21); /* 光编码器转速和状态 */
    USER_OLED_putString(2, 0, "ML: 000000  UNINIT   ", 21); /* 左轮转速和状态 */
    USER_OLED_putString(3, 0, "MR: 000000  UNINIT   ", 21); /* 右轮转速和状态 */
    USER_OLED_putString(4, 0, "> SUMO:     000000   ", 21); /* 行进累计和 */
    USER_OLED_putString(5, 0, "  SUML:     000000   ", 21); /* 左轮行进累计和 */
    USER_OLED_putString(6, 0, "  SUMR:     000000   ", 21); /* 右轮行进累计和 */
}

static const char *str_encoder_status[5] =
    {
        "OK      ",
        "UNINIT  ",
        "OVERFLOW",
        "NO_PULSE",
        "UNKNOWN ",
};

void USER_UI_ShowEncoderDynamic(void)
{
    static uint8_t update_line = 1;
    static uint8_t selected_encoder = 0;
    ButtonState_t Button_Status = RELEASED;

    Button_Status = USER_LBB_Button_ReadState(UP);
    if (Button_Status == PRESSED)
    {
        USER_OLED_putString(selected_encoder + 4, 0, " ", 1);
        if (selected_encoder == 0)
            selected_encoder = ENCODER_COUNT - 1;
        else
            selected_encoder--;
        USER_OLED_putString(selected_encoder + 4, 0, ">", 1);
    }

    Button_Status = USER_LBB_Button_ReadState(DOWN);
    if (Button_Status == PRESSED)
    {
        USER_OLED_putString(selected_encoder + 4, 0, " ", 1);
        if (selected_encoder == ENCODER_COUNT - 1)
            selected_encoder = 0;
        else
            selected_encoder++;
        USER_OLED_putString(selected_encoder + 4, 0, ">", 1);
    }

    Button_Status = USER_LBB_Button_ReadState(ENTER);
    if (Button_Status == PRESSED)
    {
        encoder_data[selected_encoder].sum_distance = 0;
    }

    update_line++;
    if (update_line > 7)
        update_line = 1;

    switch (update_line)
    {
    case 1:
        USER_OLED_putI16(1, 4, (uint16_t)encoder_data[0].speed, 6);
        USER_OLED_putString(1, 12, str_encoder_status[encoder_data[0].status], 8);
        break;
    case 2:
        USER_OLED_putI16(2, 4, (uint16_t)encoder_data[1].speed, 6);
        USER_OLED_putString(2, 12, str_encoder_status[encoder_data[1].status], 8);
        break;
    case 3:
        USER_OLED_putI16(3, 4, (uint16_t)encoder_data[2].speed, 6);
        USER_OLED_putString(3, 12, str_encoder_status[encoder_data[2].status], 8);
        break;
    case 4:
        USER_OLED_putI16(4, 12, (int16_t)encoder_data[0].sum_distance, 6);
        break;
    case 5:
        USER_OLED_putI16(5, 12, (int16_t)encoder_data[1].sum_distance, 6);
        break;
    case 6:
        USER_OLED_putI16(6, 12, (int16_t)encoder_data[2].sum_distance, 6);
        break;
    default:
        break;
    }
}

/* ================================================================
 *  第 3 页：光电传感器
 * ================================================================ */

void USER_UI_ShowPhotoelectricStatic(void)
{
    USER_OLED_putString(1, 0, "A:  0 0 0 0 0 0 0 0  ", 21);
    USER_OLED_putString(2, 0, "0000 0000 0000 0000  ", 21);
    USER_OLED_putString(3, 0, "0000 0000 0000 0000  ", 21);
    USER_OLED_putString(4, 0, "Scan Rate:     000 Hz", 21);
    USER_OLED_putString(5, 0, "Select CH:       0   ", 21);
    USER_OLED_putString(6, 0, " HIGH         0000   ", 21);
    USER_OLED_putString(7, 0, " LOW          0000   ", 21);
}

void USER_UI_ShowPhotoelectricDynamic(void)
{
    static uint8_t update_line = 1;
    static uint8_t selected_channel = 0;
    static uint8_t selected_threshold = 0;
    ButtonState_t button_status = RELEASED;

    button_status = USER_LBB_Button_ReadState(LEFT);
    if (button_status == PRESSED)
        selected_channel = (selected_channel == 0) ? (OEMT_AN_COUNT - 1) : (selected_channel - 1);

    button_status = USER_LBB_Button_ReadState(RIGHT);
    if (button_status == PRESSED)
        selected_channel = (selected_channel == OEMT_AN_COUNT - 1) ? 0 : (selected_channel + 1);

    button_status = USER_LBB_Button_ReadState(UP);
    if (button_status == PRESSED)
    {
        USER_OLED_putString(selected_threshold + 6, 0, " ", 1);
        selected_threshold = (selected_threshold == 0) ? 1 : 0;
        USER_OLED_putString(selected_threshold + 6, 0, ">", 1);
    }

    button_status = USER_LBB_Button_ReadState(DOWN);
    if (button_status == PRESSED)
    {
        USER_OLED_putString(selected_threshold + 6, 0, " ", 1);
        selected_threshold = (selected_threshold == 0) ? 1 : 0;
        USER_OLED_putString(selected_threshold + 6, 0, ">", 1);
    }

    button_status = USER_LBB_Button_ReadState(ENTER);
    if (button_status == PRESSED)
    {
        if (selected_threshold == 1)
            USER_OEMT_AN_SetHysteresisLow(selected_channel, USER_OEMT_AN_GetHysteresisLow(selected_channel) + 50);
        else
            USER_OEMT_AN_SetHysteresisHigh(selected_channel, USER_OEMT_AN_GetHysteresisHigh(selected_channel) + 50);
    }
    if (button_status == LONG_PRESSED)
        USER_OEMT_AN_AutoSetHysteresisHigh();

    button_status = USER_LBB_Button_ReadState(ESC);
    if (button_status == PRESSED)
    {
        if (selected_threshold == 1)
        {
            uint16_t low = USER_OEMT_AN_GetHysteresisLow(selected_channel);
            USER_OEMT_AN_SetHysteresisLow(selected_channel, (low > 50) ? (low - 50) : 0);
        }
        else
        {
            uint16_t high = USER_OEMT_AN_GetHysteresisHigh(selected_channel);
            USER_OEMT_AN_SetHysteresisHigh(selected_channel, (high > 50) ? (high - 50) : 0);
        }
    }
    if (button_status == LONG_PRESSED)
        USER_OEMT_AN_AutoSetHysteresisLow();

    update_line++;
    if (update_line > 7)
        update_line = 1;

    switch (update_line)
    {
    case 1:
        USER_OLED_putUI16(1, 4, oemt_data[0], 1);
        USER_OLED_putUI16(1, 6, oemt_data[1], 1);
        USER_OLED_putUI16(1, 8, oemt_data[2], 1);
        USER_OLED_putUI16(1, 10, oemt_data[3], 1);
        USER_OLED_putUI16(1, 12, oemt_data[4], 1);
        USER_OLED_putUI16(1, 14, oemt_data[5], 1);
        USER_OLED_putUI16(1, 16, oemt_data[6], 1);
        USER_OLED_putUI16(1, 18, oemt_data[7], 1);
        break;
    case 2:
        USER_OLED_putUI16(2, 0, USER_OEMT_AN_GetRawData(0), 4);
        USER_OLED_putUI16(2, 5, USER_OEMT_AN_GetRawData(1), 4);
        USER_OLED_putUI16(2, 10, USER_OEMT_AN_GetRawData(2), 4);
        USER_OLED_putUI16(2, 15, USER_OEMT_AN_GetRawData(3), 4);
        break;
    case 3:
        USER_OLED_putUI16(3, 0, USER_OEMT_AN_GetRawData(4), 4);
        USER_OLED_putUI16(3, 5, USER_OEMT_AN_GetRawData(5), 4);
        USER_OLED_putUI16(3, 10, USER_OEMT_AN_GetRawData(6), 4);
        USER_OLED_putUI16(3, 15, USER_OEMT_AN_GetRawData(7), 4);
        break;
    case 4:
        USER_OLED_putUI16(4, 14, USER_OEMT_AN_GetScanRate(), 4);
        break;
    case 5:
        USER_OLED_putUI16(5, 17, selected_channel, 1);
        break;
    case 6:
        USER_OLED_putUI16(6, 14, USER_OEMT_AN_GetHysteresisHigh(selected_channel), 4);
        break;
    case 7:
        USER_OLED_putUI16(7, 14, USER_OEMT_AN_GetHysteresisLow(selected_channel), 4);
        break;
    default:
        break;
    }
}

/* ================================================================
 *  第 4 页：ADC 数据
 * ================================================================ */

void USER_UI_ShowAdcStatic(void)
{
    USER_OLED_putString(1, 0, "P27:  0000    0000 mV", 21);
    USER_OLED_putString(2, 0, "P26:  0000    0000 mV", 21);
    USER_OLED_putString(3, 0, "P25:  0000    0000 mV", 21);
    USER_OLED_putString(4, 0, "P24:  0000    0000 mV", 21);
    USER_OLED_putString(5, 0, "POT:  0000     000  %", 21);
    USER_OLED_putString(6, 0, "TMP:  0000          C", 21);
    USER_OLED_putString(7, 0, "VDD:  0000    0000 mV", 21);
}

void USER_UI_ShowAdcDynamic(void)
{
    static uint8_t update_line = 1;

    update_line++;
    if (update_line > 7)
        update_line = 1;

    switch (update_line)
    {
    case 1:
        USER_OLED_putUI16(1, 6, adc_data[ADC_CHANNEL_0_P27], 4);
        USER_OLED_putUI16(1, 14, adc_voltage[ADC_CHANNEL_0_P27], 4);
        break;
    case 2:
        USER_OLED_putUI16(2, 6, adc_data[ADC_CHANNEL_1_P26], 4);
        USER_OLED_putUI16(2, 14, adc_voltage[ADC_CHANNEL_1_P26], 4);
        break;
    case 3:
        USER_OLED_putUI16(3, 6, adc_data[ADC_CHANNEL_2_P25], 4);
        USER_OLED_putUI16(3, 14, adc_voltage[ADC_CHANNEL_2_P25], 4);
        break;
    case 4:
        USER_OLED_putUI16(4, 6, adc_data[ADC_CHANNEL_3_P24], 4);
        USER_OLED_putUI16(4, 14, adc_voltage[ADC_CHANNEL_3_P24], 4);
        break;
    case 5:
        USER_OLED_putUI16(5, 6, adc_data[ADC_CHANNEL_4_P22], 4);
        USER_OLED_putUI16(5, 15, pot_position, 3);
        break;
    case 6:
        USER_OLED_putUI16(6, 6, adc_data[ADC_CHANNEL_11_TEMP], 4);
        USER_OLED_putFloat(6, 12, cpu_core_temperature, 4, 1);
        break;
    case 7:
        USER_OLED_putUI16(7, 6, adc_data[ADC_CHANNEL_15_PWR], 4);
        USER_OLED_putUI16(7, 14, power_voltage, 4);
        break;
    }
}

/* ================================================================
 *  第 5 页：LiDAR 传感器
 * ================================================================ */

void USER_UI_ShowLidarStatic(void)
{
    USER_OLED_putString(1, 0, "L1: 00000  STA  000  ", 21);
    USER_OLED_putString(2, 0, "L2: 00000  STA  000  ", 21);
    USER_OLED_putString(3, 0, "L3: 00000  STA  000  ", 21);
    USER_OLED_putString(4, 0, "L4: 00000  STA  000  ", 21);
    USER_OLED_putString(5, 0, "Select: LIDAR 1      ", 21);
    USER_OLED_putString(6, 0, "TX      0  RX:     0 ", 21);
    USER_OLED_putString(7, 0, "TO:     0  RF:     0 ", 21);
}

void USER_UI_ShowLidarDynamic(void)
{
    static uint8_t selected_lidar = 1;
    static uint8_t update_line = 1;
    ButtonState_t button_status = RELEASED;

    update_line++;
    if (update_line > 7)
        update_line = 1;

    switch (update_line)
    {
    case 1:
        USER_OLED_putUI16(1, 4, (uint16_t)lidar_data[1].distance, 5);
        USER_OLED_putUI16(1, 16, (uint16_t)lidar_data[1].status, 3);
        break;
    case 2:
        USER_OLED_putUI16(2, 4, (uint16_t)lidar_data[2].distance, 5);
        USER_OLED_putUI16(2, 16, (uint16_t)lidar_data[2].status, 3);
        break;
    case 3:
        USER_OLED_putUI16(3, 4, (uint16_t)lidar_data[3].distance, 5);
        USER_OLED_putUI16(3, 16, (uint16_t)lidar_data[3].status, 3);
        break;
    case 4:
        USER_OLED_putUI16(4, 4, (uint16_t)lidar_data[4].distance, 5);
        USER_OLED_putUI16(4, 16, (uint16_t)lidar_data[4].status, 3);
        break;
    case 5:
        button_status = USER_LBB_Button_ReadState(UP);
        if (button_status == PRESSED)
        {
            selected_lidar++;
            if (selected_lidar > 4)
                selected_lidar = 1;
        }

        button_status = USER_LBB_Button_ReadState(DOWN);
        if (button_status == PRESSED)
        {
            selected_lidar--;
            if (selected_lidar < 1)
                selected_lidar = 4;
        }

        USER_OLED_putUI16(5, 14, (uint16_t)selected_lidar, 1);
        break;
    case 6:
        USER_OLED_putUI16(6, 4, lidar_data[selected_lidar].tx_count, 5);
        USER_OLED_putUI16(6, 16, lidar_data[selected_lidar].rx_count, 5);
        break;
    case 7:
        USER_OLED_putUI16(7, 4, lidar_data[selected_lidar].timeout_count, 5);
        USER_OLED_putUI16(7, 16, lidar_data[selected_lidar].strength, 5);
        break;
    default:
        break;
    }
}

/* ================================================================
 *  第 6 页：IMU 陀螺仪数据
 * ================================================================ */

void USER_UI_ShowGyroscopeStatic(void)
{
    USER_OLED_putString(1, 0, "      X      Y      Z", 21);
    USER_OLED_putString(2, 0, "a 00000  00000  00000", 21);
    USER_OLED_putString(3, 0, "w 00000  00000  00000", 21);
    USER_OLED_putString(4, 0, "o 00000  00000  00000", 21);
    USER_OLED_putString(5, 0, "Temperature     00000", 21);
    USER_OLED_putString(6, 0, "Status          00000", 21);
    USER_OLED_putString(7, 0, "TX : 00000 RX : 00000", 21);
}

void USER_UI_ShowGyroscopeDynamic(void)
{
    static uint8_t update_line = 1;
    ButtonState_t Button_Status = RELEASED;

    update_line++;
    if (update_line > 7)
        update_line = 1;

    Button_Status = USER_LBB_Button_ReadState(ENTER);
    if (Button_Status == PRESSED)
    {
        USER_IMU_SetOrder(IMU_ODR_SETANGREF);
    }
    Button_Status = USER_LBB_Button_ReadState(ESC);
    if (Button_Status == PRESSED)
    {
        USER_IMU_SetOrder(IMU_ODR_SETYAWREF);
    }

    switch (update_line)
    {
    case 2:
        USER_OLED_putFloat(2, 2, imu_data.accx, 4, 1);
        USER_OLED_putFloat(2, 8, imu_data.accy, 4, 1);
        USER_OLED_putFloat(2, 15, imu_data.accz, 4, 1);
        break;
    case 3:
        USER_OLED_putFloat(3, 2, imu_data.gyrox, 4, 1);
        USER_OLED_putFloat(3, 8, imu_data.gyroy, 4, 1);
        USER_OLED_putFloat(3, 15, imu_data.gyroz, 4, 1);
        break;
    case 4:
        USER_OLED_putFloat(4, 2, imu_data.roll, 4, 1);
        USER_OLED_putFloat(4, 8, imu_data.pitch, 4, 1);
        USER_OLED_putFloat(4, 15, imu_data.yaw, 4, 1);
        break;
    case 5:
        USER_OLED_putFloat(5, 15, imu_data.temperature, 3, 2);
        break;
    case 6:
        USER_OLED_putUI16(6, 16, (uint16_t)imu_data.status, 5);
        break;
    case 7:
        USER_OLED_putUI16(7, 5, imu_data.tx_count, 5);
        USER_OLED_putUI16(7, 16, imu_data.rx_count, 5);
        break;
    default:
        break;
    }
}

/* ================================================================
 *  第 7 页：智能相机（占位）
 * ================================================================ */

void USER_UI_ShowCameraStatic(void)
{
}

void USER_UI_ShowCameraDynamic(void)
{
}

/* ================================================================
 *  第 8 页：舵机控制
 * ================================================================ */

void USER_UI_ShowServoStatic(void)
{
    USER_OLED_putString(1, 0, "        SVO1     SVO2", 21);
    USER_OLED_putString(2, 0, " ANG   00000    00000", 21);
    USER_OLED_putString(3, 0, " WDH   00000    00000", 21);
    USER_OLED_putString(4, 0, "fERR       00.000    ", 21);
}

void USER_UI_ShowServoDynamic(void)
{
    static uint8_t update_line = 1;

    update_line++;
    if (update_line > 7)
        update_line = 1;

    switch (update_line)
    {
    case 2:
        USER_OLED_putI16(2, 7, USER_SERVO_GetAngle(SERVO_0), 5);
        USER_OLED_putI16(2, 16, USER_SERVO_GetAngle(SERVO_1), 5);
        break;
    case 3:
        USER_OLED_putUI16(3, 7, USER_SERVO_GetWidth(SERVO_0), 5);
        USER_OLED_putUI16(3, 16, USER_SERVO_GetWidth(SERVO_1), 5);
        break;
    case 4:
        USER_OLED_putFloat(4, 10, USER_SERVO_GetFError(), 2, 4);
        break;
    default:
        break;
    }
}

/* ================================================================
 *  第 9 页：调试信息（占位）
 * ================================================================ */

void USER_UI_ShowDebugStatic(void)
{
}

void USER_UI_ShowDebugDynamic(void)
{
}

/* ================================================================
 *  第 10 页：IMU 累计和数据
 * ================================================================ */

void USER_UI_ShowIMUSumStatic(void)
{
    USER_OLED_putString(1, 0, "      X      Y      Z", 21);
    USER_OLED_putString(2, 0, "aSum:                ", 21);
    USER_OLED_putString(3, 0, "  00000  00000  00000", 21);
    USER_OLED_putString(4, 0, "wSum:                ", 21);
    USER_OLED_putString(5, 0, "  00000  00000  00000", 21);
    USER_OLED_putString(6, 0, "Reset: ENTER         ", 21);
}

void USER_UI_ShowIMUSumDynamic(void)
{
    static uint8_t update_line = 1;
    ButtonState_t Button_Status = RELEASED;

    update_line++;
    if (update_line > 7)
        update_line = 1;

    Button_Status = USER_LBB_Button_ReadState(ENTER);
    if (Button_Status == PRESSED)
    {
        imu_data.sum_accx = 0;
        imu_data.sum_accy = 0;
        imu_data.sum_accz = 0;
        imu_data.sum_gyrox = 0;
        imu_data.sum_gyroy = 0;
        imu_data.sum_gyroz = 0;
    }

    switch (update_line)
    {
    case 3:
        USER_OLED_putFloat(3, 2, imu_data.sum_accx, 4, 1);
        USER_OLED_putFloat(3, 8, imu_data.sum_accy, 4, 1);
        USER_OLED_putFloat(3, 15, imu_data.sum_accz, 4, 1);
        break;
    case 5:
        USER_OLED_putFloat(5, 2, imu_data.sum_gyrox, 4, 1);
        USER_OLED_putFloat(5, 8, imu_data.sum_gyroy, 4, 1);
        USER_OLED_putFloat(5, 15, imu_data.sum_gyroz, 4, 1);
        break;
    default:
        break;
    }
}

/* ================================================================
 *  第 11 页：比赛路线（模板路径）
 *
 *  布局：
 *    第 0 行    : "Route 00" 路线编号
 *    左半屏     : 路径示意图（根据选中路线绘制）
 *    右半屏上方 : 蓄力圆形（长按 ENTER 时从外向内填充，2 秒满）
 *    右半屏下方 : 倒计时毫秒数（蓄力满后显示，1 秒倒计时）
 *
 *  交互流程：
 *    IDLE → 按下 ENTER → CHARGING（圆形逐圈填充 2s）
 *         → 提前松开 → IDLE（取消）
 *         → 2s 满 → COUNTDOWN（1s 倒计时）→ 启动路线
 * ================================================================ */

/**
 * @brief 绘制模板路线的路径示意图（左半屏）。
 *
 * 模板路线：直行 → U 型掉头 → 直行返回 → U 型掉头回原位。
 * 使用线条和圆弧在屏幕左侧 (x:0~63, y:10~63) 绘制俯视轨迹。
 */
static void USER_UI_DrawTemplatePath(void)
{
    /* 左车道竖线：前进路径 (x=20, y=18~30) */
    USER_OLED_DrawLine(20, 18, 20, 30);
    /* 右车道竖线：返回路径 (x=35, y=18~30) */
    USER_OLED_DrawLine(35, 18, 35, 30);

    /* 顶部 U 型掉头 —— 从前进车道转到返回车道 */
    USER_OLED_DrawArc(27, 18, 8, 0, 180);

    /* 底部 U 型掉头 —— 从返回车道转回前进车道 */
    USER_OLED_DrawArc(27, 30, 8, 180, 360);

    /* 前进方向箭头（左车道中间） */
    USER_OLED_DrawLine(18, 22, 20, 20);
    USER_OLED_DrawLine(22, 22, 20, 20);

    /* 返回方向箭头（右车道中间） */
    USER_OLED_DrawLine(33, 26, 35, 28);
    USER_OLED_DrawLine(37, 26, 35, 28);

    /* 底部标注：起点/终点 */
    USER_OLED_putString(7, 0, "S/F", 3);
}

/**
 * @brief 绘制蓄力进度圆（右半屏上方）。
 *
 * 使用同心填充圆实现"从外向内填充"效果：
 * - 始终绘制最外圈轮廓（radius = Rmax）
 * - 随着进度增加，从 Rmax 向圆心逐层填充实心圆
 * - 进度 100% 时整个圆被填满
 *
 * @param progress 蓄力进度 0~100。
 */
static void USER_UI_DrawChargeCircle(uint8_t progress)
{
    uint8_t r;

    /* 限制进度范围 */
    if (progress > 100u)
        progress = 100u;

    /* 从外向内逐层填充：填充半径从 Rmax 递减到 Rmax*(1 - progress/100) */
    uint8_t inner_r = (uint8_t)(((uint16_t)UI_CIRCLE_RADIUS * (100u - progress)) / 100u);

    for (r = UI_CIRCLE_RADIUS; r > inner_r; r--)
    {
        USER_OLED_DrawCircle(UI_CIRCLE_CENTER_X, UI_CIRCLE_CENTER_Y, r, true);
    }

    /* 绘制最外圈轮廓线，确保边界清晰 */
    if (progress < 100u)
    {
        USER_OLED_DrawCircle(UI_CIRCLE_CENTER_X, UI_CIRCLE_CENTER_Y, UI_CIRCLE_RADIUS, false);
    }
}

void USER_UI_ShowTemplateStatic(void)
{
    /* 第 0 行：路线编号 */
    USER_OLED_putString(0, 0, "Route ", 6);
    USER_OLED_putUI16(0, 6, RACE_ROUTE_TEMPLATE, 2);

    /* 左半屏：路径示意图 */
    USER_UI_DrawTemplatePath();
}

void USER_UI_ShowTemplateDynamic(void)
{
    uint16_t press_time = button_press_time[ENTER]; /* 当前 ENTER 连续按下时长 */

    /* ======== 状态 1：IDLE —— 等待按下 ENTER ======== */
    if (!ui_charging_active && !ui_countdown_active)
    {
        /* 右下角操作提示 */
        USER_OLED_putString(7, 0, "Hold ENTER 2s to run", 21);

        /* ENTER 刚按下：进入蓄力状态 */
        if (press_time > 0u)
        {
            ui_charging_active = true;
            ui_pending_route = RACE_ROUTE_TEMPLATE;

            /* 重绘静态内容以清除提示文字 */
            USER_OLED_CleanScreen();
            USER_UI_ShowTemplateStatic();
        }
        return;
    }

    /* ======== 状态 2：CHARGING —— ENTER 按住中，圆形逐圈填充 ======== */
    if (ui_charging_active)
    {
        uint8_t progress;

        if (press_time >= UI_CHARGE_TIME_MS)
        {
            progress = 100u;
        }
        else
        {
            progress = (uint8_t)(((uint32_t)press_time * 100u) / UI_CHARGE_TIME_MS);
        }

        /* 绘制蓄力圆 */
        USER_UI_DrawChargeCircle(progress);

        /* 圆下方显示蓄力进度百分比 */
        USER_OLED_putUI16(6, 16, (uint16_t)progress, 3);
        USER_OLED_putString(6, 19, "%", 1);

        /* 状态切换由 USER_UI_Task() 检测 press_time >= UI_CHARGE_TIME_MS 完成 */
    }

    /* ======== 状态 3：COUNTDOWN —— 蓄力满，倒计时 ======== */
    if (ui_countdown_active)
    {
        /* 绘制满圆 */
        USER_UI_DrawChargeCircle(100u);

        /* 圆下方显示倒计时毫秒数 */
        USER_OLED_putString(5, 16, "GO!", 3);
        USER_OLED_putUI16(6, 15, ui_countdown_remain_ms, 4);
        USER_OLED_putString(6, 19, "ms", 2);

        /* 蜂鸣器提示（仅倒计时开始时触发一次） */
        static bool buzzer_triggered = false;
        if (!buzzer_triggered)
        {
            USER_LBB_Buzzer_On(100);
            buzzer_triggered = true;
        }
        if (ui_countdown_remain_ms == 0u)
        {
            buzzer_triggered = false;
        }
    }
}
