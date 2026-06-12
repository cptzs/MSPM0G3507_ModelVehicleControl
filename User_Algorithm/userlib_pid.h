#ifndef USERLIB_PID_H
#define USERLIB_PID_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief PID 控制器结构体。
 * @details 支持位置式 PID、增量式 PID、扩展微分 PID、高级位置式 PID 和目标斜坡控制。
 */
typedef struct
{
    /* PID 参数 */
    float kp; /* 比例系数 */
    float ki; /* 积分系数 */
    float kd; /* 微分系数 */

    /* 控制量 */
    int32_t target;     /* 目标值 */
    int32_t current;    /* 当前反馈值 */
    int32_t error;      /* 当前误差 */
    int32_t integral;   /* 积分项 */
    int32_t derivative; /* 微分项 */

    /* 功能控制参数 */
    int32_t deadzone; /* 死区范围 */
    bool vi_enable;   /* 变积分使能 */

    /* 积分限制 */
    int32_t max_integral;    /* 积分上限 */
    int32_t min_integral;    /* 积分下限 */
    int32_t integral_depart; /* 积分分离阈值 */

    /* 历史数据 */
    int32_t last_error;      /* 上一次误差 */
    int32_t last_last_error; /* 上上次误差，供增量式 PID 使用 */
    int32_t last_output;     /* 上一次输出 */

    /* 输出限制 */
    int32_t output;     /* PID 输出值 */
    int32_t max_output; /* 输出上限 */
    int32_t min_output; /* 输出下限 */

    /* 增量式 PID 专用 */
    int32_t delta_output;     /* 增量输出 */
    int32_t max_delta_output; /* 增量输出上限 */
    int32_t min_delta_output; /* 增量输出下限 */

    /* 目标斜坡控制 */
    int32_t ramp_target; /* 斜坡控制最终目标 */
    int32_t ramp_rate;   /* 每次调用时接近目标的百分比 */
    bool ramp_enable;    /* 斜坡控制使能 */
} PID_Control_Struct_TypeDef;

/**
 * @brief 更新斜坡控制目标。
 * @param pid PID 控制器指针。
 */
void USER_PID_UpdateRampTarget(PID_Control_Struct_TypeDef *pid);

/**
 * @brief 位置式 PID 控制算法。
 * @param pid PID 控制器指针。
 */
void USER_Positional_PID_Control(PID_Control_Struct_TypeDef *pid);

/**
 * @brief 带扩展微分项的位置式 PID 控制算法。
 * @param pid PID 控制器指针。
 * @param ex_kd 附加微分系数。
 * @param ex_d 附加微分输入值。
 */
void USER_Positional_PID_Control_Advanced_ExD(PID_Control_Struct_TypeDef *pid, float ex_kd, float ex_d);

/**
 * @brief 增量式 PID 控制算法。
 * @param pid PID 控制器指针。
 */
void USER_Incremental_PID_Control(PID_Control_Struct_TypeDef *pid);

/**
 * @brief 高级位置式 PID 控制算法。
 * @param pid PID 控制器指针。
 */
void USER_Positional_PID_Control_Advanced(PID_Control_Struct_TypeDef *pid);

/**
 * @brief 设置 PID 目标值，可选择启用斜坡控制。
 * @param pid PID 控制器指针。
 * @param target 最终目标值。
 * @param ramp_rate 斜坡速率百分比；传入 0 表示直接设置目标值。
 */
void USER_PID_SetTargetWithRamp(PID_Control_Struct_TypeDef *pid, int32_t target, int32_t ramp_rate);

/**
 * @brief 清空 PID 控制器状态并停止输出。
 * @param pid PID 控制器指针。
 */
void USER_PID_ClearAndStop(PID_Control_Struct_TypeDef *pid);

#endif /* USERLIB_PID_H */
