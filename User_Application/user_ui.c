#include "user_ui.h"
#include "user_os.h"

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
    "Threads      ",
    "IMU Sum Data ",
    "Template Path"};

/* ---- 比赛启动交互状态 ---- */
#define UI_CHARGE_TIME_MS 2000u /* 长按蓄力时长：2 秒 */
#define UI_COUNTDOWN_MS 2000u   /* 蓄力完成后的稳定倒计时：2 秒，小车离手稳定 */
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

    /* 倒计时阶段：下方显示毫秒倒计时 */
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
 *  第 9 页：线程运行用时（Threads）
 *
 *  布局：
 *    第 0 行 : 页面标题栏（由 ShowStaticContent 统一绘制）
 *    第 1 行 : 列标题 TASK / L(ast) / M(ax)
 *    第 2-7 行 : 各线程名称 + 最近一次运行耗时 + 历史最大耗时（单位 us）
 *
 *  显示格式：name     L:XXXX M:XXXX
 * ================================================================ */

void USER_UI_ShowDebugStatic(void)
{
    /* 列标题 */
    USER_OLED_putString(1, 0, "TASK       L/us  M/us", 21);
}

void USER_UI_ShowDebugDynamic(void)
{
    static uint8_t update_row = 0u;
    uint8_t task_count;
    uint8_t i;
    uint8_t row;
    USER_OS_TaskStats_t stats;
    char line_buf[22];

    task_count = USER_OS_GetTaskCount();

    /* 轮询刷新：每个周期只更新一行，降低 OLED 刷新开销 */
    if (task_count > 0u)
    {
        update_row++;
        if (update_row >= task_count)
        {
            update_row = 0u;
        }

        i = update_row;
        row = i + 2u; /* 数据行从第 2 行开始 */

        if (USER_OS_GetTaskStats(i, &stats))
        {
            /* 限制显示范围，避免越界 */
            if (stats.last_cost_us > 9999u)
            {
                stats.last_cost_us = 9999u;
            }
            if (stats.max_cost_us > 9999u)
            {
                stats.max_cost_us = 9999u;
            }

            /* 格式：name     L:XXXX M:XXXX */
            (void)snprintf(line_buf, sizeof(line_buf),
                           "%-8s L:%4u M:%4u",
                           (stats.name != NULL) ? stats.name : "?",
                           (uint16_t)stats.last_cost_us,
                           (uint16_t)stats.max_cost_us);
            USER_OLED_putString(row, 0u, line_buf, 21u);
        }
    }
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
 *    第 0 行 : 统一页面标题栏，由 USER_UI_ShowStaticContent() 绘制。
 *    左侧区域: 左半屏除标题栏外全部用于显示路线示意图。
 *    右侧第 2 行 : 路线号与路线名称。
 *    右侧第 3 行 : 长按启动提示 / GO 状态
 *    右侧第 4 行：进度条。
 *    右侧第 5 行 : 当前执行步骤与主要参数。
 *    右侧第 6 行 : 当前步骤剩余超时。
 *
 *  交互流程：
 *    IDLE → 按下 ENTER → CHARGING（长按 2s 进度条充满）
 *         → 提前松开 → IDLE（取消）
 *         → 2s 满 → STABILIZE（2s 稳定倒计时，TMO 2000→0，S-- START）→ 启动路线
 * ================================================================ */

/* ---- Route 页面布局参数 ---- */
#define UI_ROUTE_MAP_X 0u  // 左侧区域起始 X 坐标
#define UI_ROUTE_MAP_Y 0u  // 左侧区域起始 Y 坐标
#define UI_ROUTE_MAP_W 60u // 左侧区域宽度
#define UI_ROUTE_MAP_H 54u // 左侧区域高度

#define UI_ROUTE_INFO_COL 10u // 右侧信息列起始 X 坐标
#define UI_ROUTE_BAR_X 70u    // 进度条起始 X 坐标
#define UI_ROUTE_BAR_Y 26u    // 进度条起始 Y 坐标
#define UI_ROUTE_BAR_W 42u    // 进度条宽度
#define UI_ROUTE_BAR_H 6u     // 进度条高度

/**
 * @brief 计算 int16_t 的绝对值.
 *
 * @param value 输入值.
 *
 * @return 绝对值结果.
 */
static uint16_t USER_UI_AbsI16(int16_t value)
{
    if (value < 0)
    {
        return (uint16_t)(-value);
    }

    return (uint16_t)value;
}

/**
 * @brief 绘制模板路线的示意图.
 *
 * @note 本函数将左侧除标题栏外的空间尽量占满，
 *       仅用于表达路线结构，不要求严格按实际比例绘制.
 */
static void USER_UI_DrawTemplatePath(void)
{
    const uint8_t left_x = 10u;
    const uint8_t right_x = 42u;
    const uint8_t top_y = 14u;
    const uint8_t bottom_y = 56u;
    const uint8_t center_x = 26u;
    const uint8_t radius = 16u;

    /* 第一步：绘制左侧区域边框，占满左半屏 */
    USER_OLED_DrawRect(UI_ROUTE_MAP_X,
                       UI_ROUTE_MAP_Y,
                       UI_ROUTE_MAP_X + UI_ROUTE_MAP_W - 1u,
                       UI_ROUTE_MAP_Y + UI_ROUTE_MAP_H - 1u,
                       false);

    /* 第二步：绘制两条主竖线，增大纵向利用率 */
    USER_OLED_DrawVLine(left_x, top_y, bottom_y);
    USER_OLED_DrawVLine(right_x, top_y, bottom_y);

    /* 第三步：绘制顶部和底部 U 型连接，形成往返路线示意 */
    USER_OLED_DrawArc(center_x, top_y, radius, 0, 180);
    USER_OLED_DrawArc(center_x, bottom_y, radius, 180, 360);

    /* 第四步：绘制方向箭头 */
    USER_OLED_DrawLine(left_x, 28u, (uint8_t)(left_x - 3u), 32u);
    USER_OLED_DrawLine(left_x, 28u, (uint8_t)(left_x + 3u), 32u);

    USER_OLED_DrawLine(right_x, 42u, (uint8_t)(right_x - 3u), 38u);
    USER_OLED_DrawLine(right_x, 42u, (uint8_t)(right_x + 3u), 38u);
}

/**
 * @brief 绘制 Route 页面进度条.
 *
 * @param percent 百分比，范围 0~100.
 */
static void USER_UI_DrawRouteProgressBar(uint8_t percent)
{
    uint8_t fill_w;

    /* 第一步：限制百分比范围 */
    if (percent > 100u)
    {
        percent = 100u;
    }

    /* 第二步：计算内部填充宽度 */
    fill_w = (uint8_t)(((uint16_t)(UI_ROUTE_BAR_W - 2u) * percent) / 100u);

    /* 第三步：绘制进度条边框 */
    USER_OLED_DrawRect(UI_ROUTE_BAR_X,
                       UI_ROUTE_BAR_Y,
                       UI_ROUTE_BAR_X + UI_ROUTE_BAR_W - 1u,
                       UI_ROUTE_BAR_Y + UI_ROUTE_BAR_H - 1u,
                       false);

    /* 第四步：绘制进度条填充 */
    if (fill_w > 0u)
    {
        USER_OLED_DrawRect(UI_ROUTE_BAR_X + 1u,
                           UI_ROUTE_BAR_Y + 1u,
                           UI_ROUTE_BAR_X + fill_w,
                           UI_ROUTE_BAR_Y + UI_ROUTE_BAR_H - 2u,
                           true);
    }
}

/**
 * @brief 将当前动作格式化为适合 OLED 显示的短文本.
 *
 * @param action_ptr 动作指针.
 * @param step_index 当前步骤索引，从 0 开始.
 * @param out_buf 输出缓冲区.
 * @param out_len 输出缓冲区长度.
 */
static void USER_UI_FormatRouteStepText(const USER_Race_Action_t *action_ptr,
                                        int16_t step_index,
                                        char *out_buf,
                                        uint8_t out_len)
{
    uint16_t main_value = 0u;
    uint8_t step_no;

    if ((out_buf == NULL) || (out_len == 0u))
    {
        return;
    }

    /* 第一步：没有有效动作时显示空闲状态 */
    if ((action_ptr == NULL) || (step_index < 0))
    {
        (void)snprintf(out_buf, out_len, "S-- IDLE");
        return;
    }

    step_no = (uint8_t)(step_index + 1u);

    /* 第二步：根据动作类型格式化文本 */
    switch (action_ptr->action_type)
    {
    case USER_Race_ACTION_MOVE_DISTANCE:
        /* mm 转 cm，正值表示前进，负值表示后退 */
        main_value = (uint16_t)(USER_UI_AbsI16((int16_t)action_ptr->param1) / 10u);
        if (action_ptr->param1 >= 0.0f)
        {
            (void)snprintf(out_buf, out_len, "S%02u FW %u", step_no, main_value);
        }
        else
        {
            (void)snprintf(out_buf, out_len, "S%02u BW %u", step_no, main_value);
        }
        break;

    case USER_Race_ACTION_ROTATE_ANGLE:
        /* deg 保持不变，正值右转，负值左转 */
        main_value = USER_UI_AbsI16((int16_t)action_ptr->param1);
        if (action_ptr->param1 >= 0.0f)
        {
            (void)snprintf(out_buf, out_len, "S%02u TR %u", step_no, main_value);
        }
        else
        {
            (void)snprintf(out_buf, out_len, "S%02u TL %u", step_no, main_value);
        }
        break;

    case USER_Race_ACTION_WAIT_MS:
        main_value = (uint16_t)action_ptr->param1;
        (void)snprintf(out_buf, out_len, "S%02u WT %u", step_no, main_value);
        break;

    case USER_Race_ACTION_STOP:
        (void)snprintf(out_buf, out_len, "S%02u STP", step_no);
        break;

    case USER_Race_ACTION_END:
        (void)snprintf(out_buf, out_len, "S%02u END", step_no);
        break;

    default:
        (void)snprintf(out_buf, out_len, "S%02u N/A", step_no);
        break;
    }
}

/**
 * @brief 获取 Route 页面当前应显示的动作信息.
 *
 * @param action_ptr 输出动作.
 * @param step_index_ptr 输出步骤索引.
 * @param timeout_remain_ptr 输出剩余超时.
 *
 * @return true 表示存在有效动作；false 表示当前没有有效动作.
 */
static bool USER_UI_GetRouteDisplayAction(USER_Race_Action_t *action_ptr,
                                          int16_t *step_index_ptr,
                                          uint32_t *timeout_remain_ptr)
{
    bool has_action;

    /* 第一步：优先显示当前实际运行中的动作 */
    has_action = USER_Race_GetCurrentAction(action_ptr, step_index_ptr, timeout_remain_ptr);
    if (has_action)
    {
        return true;
    }

    /* 第二步：若当前未运行，则显示模板路线首个动作作为预览 */
    has_action = USER_Race_GetTemplatePreviewAction(action_ptr, timeout_remain_ptr);
    if (has_action)
    {
        if (step_index_ptr != NULL)
        {
            *step_index_ptr = 0;
        }
        return true;
    }

    return false;
}

/**
 * @brief 绘制 Route 页面静态内容.
 */
void USER_UI_ShowTemplateStatic(void)
{
    /* 第一步：绘制左侧路线示意图 */
    USER_UI_DrawTemplatePath();

    /* 第二步：绘制右侧固定内容占位 */
    USER_OLED_putString(2, UI_ROUTE_INFO_COL, "R00     ", 11); // 不能超过11字符

    /* 第三行：HOLD*/
    USER_OLED_putString(3, UI_ROUTE_INFO_COL, "HOLD", 4);
    USER_UI_DrawRouteProgressBar(0u);

    /* 第五行：步骤摘要 */
    USER_OLED_putString(5, UI_ROUTE_INFO_COL, "S-- IDLE", 8); // 不能超过11字符

    /* 第六行：超时剩余 */
    USER_OLED_putString(6, UI_ROUTE_INFO_COL, "TMO ----", 8); // 不能超过11字符
}

/**
 * @brief 绘制 Route 页面动态内容.
 */
void USER_UI_ShowTemplateDynamic(void)
{
    static bool buzzer_triggered = false;
    uint16_t press_time = button_press_time[ENTER];
    uint8_t progress = 0u;
    USER_Race_Action_t action;
    int16_t step_index = -1;
    uint32_t timeout_remain_ms = 0u;
    char step_text[13];
    bool has_action = false;

    /* 第一步：若当前未处于蓄力或倒计时状态，检测是否开始长按 */
    if (!ui_charging_active && !ui_countdown_active)
    {
        if (press_time > 0u)
        {
            ui_charging_active = true;
            ui_pending_route = RACE_ROUTE_TEMPLATE;

            /* 重新清屏并绘制静态内容，避免旧内容残留 */
            USER_OLED_CleanScreen();
            USER_UI_ShowStaticContent(PAGE_TEMPLATE);
            return;
        }
    }

    /* 第二步：计算长按启动进度 */
    if (ui_charging_active)
    {
        if (press_time >= UI_CHARGE_TIME_MS)
        {
            progress = 100u;
        }
        else
        {
            progress = (uint8_t)(((uint32_t)press_time * 100u) / UI_CHARGE_TIME_MS);
        }
    }
    else if (ui_countdown_active)
    {
        progress = 100u;
    }
    else
    {
        progress = 0u;
    }

    /* 第三步：刷新路线号和名称 */
    USER_OLED_putString(2, UI_ROUTE_INFO_COL, "           ", 11);
    USER_OLED_putString(2, UI_ROUTE_INFO_COL, "R00 ", 4);
    USER_OLED_putString(2, UI_ROUTE_INFO_COL + 4u,
                        USER_Race_GetRouteName(RACE_ROUTE_TEMPLATE),
                        7); // 不能超过7字符

    /* 第四步：刷新第三行状态显示 */
    USER_OLED_putString(3, UI_ROUTE_INFO_COL, "    ", 4);
    if (ui_countdown_active)
    {
        /* 稳定倒计时阶段：小车离手稳定，显示 HOLD */
        USER_OLED_putString(3, UI_ROUTE_INFO_COL, "HOLD", 4);
    }
    else
    {
        /* 空闲和长按阶段显示 HOLD */
        USER_OLED_putString(3, UI_ROUTE_INFO_COL, "HOLD", 4);
    }

    /* 第五步：刷新进度条 */
    USER_UI_DrawRouteProgressBar(progress);

    /* 第六步：刷新当前执行动作摘要 */
    if (ui_countdown_active)
    {
        /* 稳定倒计时阶段：固定显示 S-- START */
        USER_OLED_putString(5, UI_ROUTE_INFO_COL, "             ", 13);
        USER_OLED_putString(5, UI_ROUTE_INFO_COL, "S-- START", 9);
    }
    else
    {
        has_action = USER_UI_GetRouteDisplayAction(&action, &step_index, &timeout_remain_ms);
        if (has_action)
        {
            USER_UI_FormatRouteStepText(&action, step_index, step_text, sizeof(step_text));
        }
        else
        {
            USER_UI_FormatRouteStepText(NULL, -1, step_text, sizeof(step_text));
        }

        USER_OLED_putString(5, UI_ROUTE_INFO_COL, "             ", 13);
        USER_OLED_putString(5, UI_ROUTE_INFO_COL, step_text, 12);
    }

    /* 第七步：刷新当前步骤超时剩余 */
    USER_OLED_putString(6, UI_ROUTE_INFO_COL, "TMO ", 4);
    if (ui_countdown_active)
    {
        /* 稳定倒计时阶段：显示 TMO 从 2000 递减到 0 */
        USER_OLED_putUI16(6, UI_ROUTE_INFO_COL + 4u, ui_countdown_remain_ms, 4);
    }
    else if (has_action)
    {
        if (timeout_remain_ms > 9999u)
        {
            timeout_remain_ms = 9999u;
        }
        USER_OLED_putUI16(6, UI_ROUTE_INFO_COL + 4u, (uint16_t)timeout_remain_ms, 4);
    }
    else
    {
        USER_OLED_putString(6, UI_ROUTE_INFO_COL + 4u, "----", 4);
    }

    /* 第八步：倒计时开始时蜂鸣一次 */
    if (ui_countdown_active && !buzzer_triggered)
    {
        USER_LBB_Buzzer_On(100);
        buzzer_triggered = true;
    }
    else if (!ui_countdown_active)
    {
        buzzer_triggered = false;
    }
}