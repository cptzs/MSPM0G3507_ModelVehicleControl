#include "ti_msp_dl_config.h"
#include "user_config.h"

#include <stdbool.h>
#include <stdint.h>

/* 用户外设接口头文件 */
#include "userlib_adc.h"
#include "userlib_can.h"
#include "userlib_pwm.h"
#include "userlib_sys.h"
#include "userlib_uart.h"

/* 用户设备头文件 */
#include "userlib_encoder.h"
#include "userlib_imu.h"
#include "userlib_lbb.h"
#include "userlib_lidar.h"
#include "userlib_modbus.h"
#include "userlib_motor.h"
#if OEMT_SENSOR_MODE == OEMT_SENSOR_MODE_DG
#include "userlib_oemt_dg.h"
#else
#include "userlib_oemt_an.h"
#endif
#include "userlib_oled.h"
#include "userlib_servo.h"

/* 用户应用头文件 */
#include "user_ui.h"
#include "userapp_mcm.h"
#include "userapp_race.h"
#include "userapp_state_estimator.h"

#include "globals.h"

/**
 * @brief 用户设备初始化。
 *
 * 当前工程默认启动所有外部设备，不再通过宏裁剪设备启动逻辑。
 * OEMT 由于模拟式和数字式共享同一组物理接口，仍通过 OEMT_SENSOR_MODE 做互斥选择。
 */
static void USER_Device_Init(void)
{
  USER_LBB_Init();
  USER_OLED_Init();
  USER_ENCODER_Init(encoder_data);
  USER_IMU_Init(true, &imu_data, UART_0);
  USER_Motor_Init();
  USER_ADC_Init(adc_data, 7);
  USER_SERVO_Init();
  USER_lidar_Init(lidar_data);

#if OEMT_SENSOR_MODE == OEMT_SENSOR_MODE_DG
  USER_OEMT_DG_Init(oemt_data);
#else
  USER_OEMT_AN_Init(oemt_data, &adc_data[ADC_CHANNEL_2_P25]);
#endif

  USER_Modbus_Slave_Init(true, UART_1, modbus_regs, &modbus_status);
}

/**
 * @brief 程序入口。
 */
int main(void)
{
  SYSCFG_DL_init();

  USER_SYSTEM_Init();     /* 初始化系统时钟和系统滴答 */
  USER_GlobalData_Init(); /* 初始化全局数据结构 */
  USER_Device_Init();     /* 初始化用户外部设备 */

  USER_STATE_Init(); /* 初始化统一车辆状态估计。 */

  while (true)
  {
    /******************************** 1ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_1ms))
    {
      USER_GlobalData_Task();
    }

    /******************************** 2ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_2ms))
    {
    }

    /******************************** 5ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_5ms))
    {
      USER_LIDAR_Task();
      USER_UI_Task();
    }

    /******************************** 10ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_10ms))
    {
      USER_State_Task();
      USER_MCM_Task();
      USER_Race_Task();
    }

    /******************************** 20ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_20ms))
    {
    }

    /******************************** 50ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_50ms))
    {
    }

    /******************************** 100ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_100ms))
    {
    }

    /******************************** 200ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_200ms))
    {
    }

    /******************************** 500ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_500ms))
    {
      USER_LBB_LED_On(LED0, 250);
    }

    /******************************** 1000ms 周期任务 ********************************/
    if (USER_SYSTEM_ConsumeTime(&system_time.t_1000ms))
    {
    }
  }
}
