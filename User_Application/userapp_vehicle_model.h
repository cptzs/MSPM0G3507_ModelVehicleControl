#ifndef USERAPP_VEHICLE_MODEL_H
#define USERAPP_VEHICLE_MODEL_H

/**
 * @file userapp_vehicle_model.h
 * @brief 应用层共享的车辆几何参数和编码器标定常量。
 *
 * 本头文件只保存物理标定常量和派生换算系数。它从 MCM 中拆出，
 * 使状态估测层可以复用同一套车辆模型，而不依赖运动控制代码。
 *
 * 标定说明：
 * 1. `VEHICLE_*` 描述驱动轮和底盘几何。
 * 2. `ODOMETER_*` 描述独立路程计轮。
 * 3. 所有距离相关常量均使用 mm。
 * 4. 编码器速度字段的采样周期为 `MCM_CONTROL_PERIOD_MS`。
 */

/* 圆周率，无单位。 */
#define MCM_PI 3.14159265f

/* 控制和状态估测共用的更新周期，单位：ms。 */
#define MCM_CONTROL_PERIOD_MS 10

/* 驱动轮直径，单位：mm。实际调车时应测量轮胎有效滚动直径，而不只是标称外径。 */
#define VEHICLE_WHEEL_DIAMETER_MM 65.0f

/* 驱动轮周长，单位：mm。由驱动轮直径自动推导。 */
#define VEHICLE_WHEEL_CIRCUMFERENCE_MM (VEHICLE_WHEEL_DIAMETER_MM * MCM_PI)

/* 左右驱动轮接地点之间的等效轮距，单位：mm。影响差速转角和原地旋转角度估计。 */
#define VEHICLE_TRACK_WIDTH_MM 114.0f

/* 电机编码器到驱动轮输出轴的传动比例，无单位。这里表示输出轮转一圈对应的电机侧比例关系。 */
#define VEHICLE_GEAR_RATIO 0.0357142857f

/* 单个驱动电机编码器每转脉冲数，单位：pulse/rev。按编码器实际计数方式填写。 */
#define VEHICLE_ENCODER_RESOLUTION_PPR 500.0f

/* 驱动轮编码器速度字段的刷新周期，单位：ms。当前与控制周期保持一致。 */
#define VEHICLE_ENCODER_UPDATE_INTERVAL_MS MCM_CONTROL_PERIOD_MS

/* 驱动轮前进 1 mm 对应的编码器计数，单位：pulse/mm。由编码器分辨率、轮周长和传动比推导。 */
#define VEHICLE_ENCODER_PULSE_PER_MM (VEHICLE_ENCODER_RESOLUTION_PPR / (VEHICLE_WHEEL_CIRCUMFERENCE_MM * VEHICLE_GEAR_RATIO))

/* 每个控制周期内 1 mm/s 速度对应的编码器计数，单位：pulse/(mm/s/control-period)。供速度 PID 换算使用。 */
#define VEHICLE_ENCODER_PULSE_PER_CONTROL_PERIOD (VEHICLE_ENCODER_PULSE_PER_MM * ((float)MCM_CONTROL_PERIOD_MS / 1000.0f))

/* 独立路程计轮直径，单位：mm。应按实际贴地滚动轮的有效直径标定。 */
#define ODOMETER_WHEEL_DIAMETER_MM 35.0f

/* 独立路程计轮周长，单位：mm。由路程计轮直径自动推导。 */
#define ODOMETER_WHEEL_CIRCUMFERENCE_MM (ODOMETER_WHEEL_DIAMETER_MM * MCM_PI)

/* 独立路程计编码器每转脉冲数，单位：pulse/rev。按实际解码后的计数分辨率填写。 */
#define ODOMETER_ENCODER_RESOLUTION_PPR 4096.0f

/* 独立路程计速度/增量估计的刷新周期，单位：ms。当前与控制周期保持一致。 */
#define ODOMETER_UPDATE_INTERVAL_MS MCM_CONTROL_PERIOD_MS

/* 独立路程计前进 1 mm 对应的编码器计数，单位：pulse/mm。 */
#define ODOMETER_ENCODER_PULSE_PER_MM (ODOMETER_ENCODER_RESOLUTION_PPR / ODOMETER_WHEEL_CIRCUMFERENCE_MM)

/* 独立路程计单个编码器计数对应的距离，单位：mm/pulse。 */
#define ODOMETER_MM_PER_PULSE (1.0f / ODOMETER_ENCODER_PULSE_PER_MM)

#endif /* USERAPP_VEHICLE_MODEL_H */
