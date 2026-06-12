#include "userlib_pid.h"

#include <stdlib.h>

/**
 * @brief 更新斜坡控制目标值。
 * @details 每次调用时将当前 target 按 ramp_rate 百分比逐步接近 ramp_target。
 * @param pid PID 控制器指针。
 */
void USER_PID_UpdateRampTarget(PID_Control_Struct_TypeDef *pid)
{
    /* 如果未启用斜坡控制，直接返回 */
    if (!pid->ramp_enable)
    {
        return;
    }

    /* 计算最终目标值与当前目标值之间的差值 */
    int32_t target_diff = pid->ramp_target - pid->target;

    /* 如果已经接近最终目标值，则直接锁定目标并关闭斜坡 */
    if (abs(target_diff) <= 1)
    {
        pid->target = pid->ramp_target;
        pid->ramp_enable = false;
    }
    else
    {
        /* 按剩余差值的固定百分比计算本次步进量 */
        int32_t step = target_diff * pid->ramp_rate / 100;

        /* 防止差值较小时步进量被整数除法截断为 0 */
        if (step == 0)
        {
            step = (target_diff > 0) ? 1 : -1;
        }

        /* 更新当前目标值 */
        pid->target += step;
    }
}

/**
 * @brief 标准位置式 PID 控制算法。
 * @details 包含死区处理、积分分离、积分限幅、输出限幅和积分抗饱和。
 * @param pid PID 控制器指针。
 */
void USER_Positional_PID_Control(PID_Control_Struct_TypeDef *pid)
{
    /* 先处理目标斜坡，让 target 平滑靠近 ramp_target */
    USER_PID_UpdateRampTarget(pid);

    /* 计算当前误差 */
    pid->error = pid->target - pid->current;

    /* 死区处理：误差足够小时认为已经到达目标 */
    if (abs(pid->error) <= pid->deadzone)
    {
        pid->error = 0;
    }

    /* 积分分离：误差较小时才累加积分，减少大误差下的积分饱和 */
    if (abs(pid->error) <= pid->integral_depart)
    {
        /* 累加积分项 */
        pid->integral += pid->error;

        /* 限制积分上限 */
        if (pid->integral > pid->max_integral)
            pid->integral = pid->max_integral;
        /* 限制积分下限 */
        else if (pid->integral < pid->min_integral)
            pid->integral = pid->min_integral;
    }

    /* 计算微分项 */
    pid->derivative = pid->error - pid->last_error;

    /* 计算 PID 输出：比例项 + 积分项 + 微分项 */
    pid->output = (int32_t)(pid->kp * pid->error +
                            pid->ki * pid->integral +
                            pid->kd * pid->derivative);

    /* 限制输出上限 */
    if (pid->output > pid->max_output)
        pid->output = pid->max_output;
    /* 限制输出下限 */
    else if (pid->output < pid->min_output)
        pid->output = pid->min_output;

    /* 积分抗饱和：输出已经打满时撤销本次积分累加 */
    if ((pid->output >= pid->max_output || pid->output <= pid->min_output) &&
        abs(pid->error) <= pid->integral_depart)
    {
        pid->integral -= pid->error;
    }

    /* 保存本次误差，供下次计算微分项使用 */
    pid->last_error = pid->error;

    /* 保存本次输出，供调试或其他算法复用 */
    pid->last_output = pid->output;
}

/**
 * @brief 带扩展微分项的位置式 PID 控制算法。
 * @details 在标准位置式 PID 输出中叠加 ex_d * ex_kd。
 * @param pid PID 控制器指针。
 * @param ex_kd 附加微分系数。
 * @param ex_d 附加微分输入值。
 */
void USER_Positional_PID_Control_Advanced_ExD(PID_Control_Struct_TypeDef *pid, float ex_kd, float ex_d)
{
    /* 先处理目标斜坡，让 target 平滑靠近 ramp_target */
    USER_PID_UpdateRampTarget(pid);

    /* 计算当前误差 */
    pid->error = pid->target - pid->current;

    /* 死区处理：误差足够小时认为已经到达目标 */
    if (abs(pid->error) <= pid->deadzone)
    {
        pid->error = 0;
    }

    /* 积分分离：误差较小时才累加积分 */
    if (abs(pid->error) <= pid->integral_depart)
    {
        /* 累加积分项 */
        pid->integral += pid->error;

        /* 限制积分上限 */
        if (pid->integral > pid->max_integral)
            pid->integral = pid->max_integral;
        /* 限制积分下限 */
        else if (pid->integral < pid->min_integral)
            pid->integral = pid->min_integral;
    }

    /* 计算标准微分项 */
    pid->derivative = pid->error - pid->last_error;

    /* 计算 PID 输出，并叠加外部微分补偿项 */
    pid->output = (int32_t)(pid->kp * pid->error +
                            pid->ki * pid->integral +
                            ex_d * ex_kd +
                            pid->kd * pid->derivative);

    /* 限制输出上限 */
    if (pid->output > pid->max_output)
        pid->output = pid->max_output;
    /* 限制输出下限 */
    else if (pid->output < pid->min_output)
        pid->output = pid->min_output;

    /* 积分抗饱和：输出已经打满时撤销本次积分累加 */
    if ((pid->output >= pid->max_output || pid->output <= pid->min_output) &&
        abs(pid->error) <= pid->integral_depart)
    {
        pid->integral -= pid->error;
    }

    /* 保存本次误差，供下次计算微分项使用 */
    pid->last_error = pid->error;

    /* 保存本次输出，供调试或其他算法复用 */
    pid->last_output = pid->output;
}

/**
 * @brief 增量式 PID 控制算法。
 * @details 输出为本次控制增量累加后的总输出。
 * @param pid PID 控制器指针。
 */
void USER_Incremental_PID_Control(PID_Control_Struct_TypeDef *pid)
{
    /* 先处理目标斜坡，让 target 平滑靠近 ramp_target */
    USER_PID_UpdateRampTarget(pid);

    /* 计算当前误差 */
    pid->error = pid->target - pid->current;

    /* 死区处理：误差足够小时认为已经到达目标 */
    if (abs(pid->error) <= pid->deadzone)
    {
        pid->error = 0;
    }

    /* 计算增量式 PID 输出增量 */
    pid->delta_output = (int32_t)(pid->kp * (pid->error - pid->last_error) +
                                  pid->ki * pid->error +
                                  pid->kd * (pid->error - 2 * pid->last_error + pid->last_last_error));

    /* 限制单次增量输出上限 */
    if (pid->delta_output > pid->max_delta_output)
        pid->delta_output = pid->max_delta_output;
    /* 限制单次增量输出下限 */
    else if (pid->delta_output < pid->min_delta_output)
        pid->delta_output = pid->min_delta_output;

    /* 将本次输出增量累加到总输出 */
    pid->output = pid->last_output + pid->delta_output;

    /* 限制总输出上限 */
    if (pid->output > pid->max_output)
        pid->output = pid->max_output;
    /* 限制总输出下限 */
    else if (pid->output < pid->min_output)
        pid->output = pid->min_output;

    /* 保存上上次误差 */
    pid->last_last_error = pid->last_error;

    /* 保存上次误差 */
    pid->last_error = pid->error;

    /* 保存本次输出 */
    pid->last_output = pid->output;
}

/**
 * @brief 高级位置式 PID 控制算法。
 * @details 集成变积分、死区处理、简单微分低通滤波和输出限幅。
 * @param pid PID 控制器指针。
 */
void USER_Positional_PID_Control_Advanced(PID_Control_Struct_TypeDef *pid)
{
    int32_t abs_error;
    int32_t derivative_raw;
    float ki_adjusted;

    /* 先处理目标斜坡，让 target 平滑靠近 ramp_target */
    USER_PID_UpdateRampTarget(pid);

    /* 计算当前误差 */
    pid->error = pid->target - pid->current;

    /* 死区处理：误差足够小时认为已经到达目标 */
    if (abs(pid->error) <= pid->deadzone)
    {
        pid->error = 0;
    }

    /* 误差为零时无需继续计算 */
    if (pid->error == 0)
    {
        return;
    }

    /* 默认使用原始积分系数 */
    ki_adjusted = pid->ki;

    /* 如果启用变积分，根据误差大小调整积分系数 */
    if (pid->vi_enable)
    {
        /* 计算误差绝对值 */
        abs_error = abs(pid->error);

        /* 误差很大时大幅减小积分，避免超调 */
        if (abs_error > pid->integral_depart)
        {
            ki_adjusted = pid->ki * 0.3f;
        }
        /* 误差中等时适度减小积分 */
        else if (abs_error > pid->integral_depart / 2)
        {
            ki_adjusted = pid->ki * 0.7f;
        }
        /* 误差很小时增大积分，提高稳态精度 */
        else if (abs_error < pid->integral_depart / 4)
        {
            ki_adjusted = pid->ki * 1.5f;
        }
        /* 误差较小时略微增大积分 */
        else
        {
            ki_adjusted = pid->ki * 1.1f;
        }
    }

    /* 积分分离：误差较小时才累加积分 */
    if (abs(pid->error) <= pid->integral_depart)
    {
        /* 累加积分项 */
        pid->integral += pid->error;

        /* 限制积分上限 */
        if (pid->integral > pid->max_integral)
            pid->integral = pid->max_integral;
        /* 限制积分下限 */
        else if (pid->integral < pid->min_integral)
            pid->integral = pid->min_integral;
    }

    /* 计算原始微分项 */
    derivative_raw = pid->error - pid->last_error;

    /* 对微分项做简单一阶低通滤波，减少高频噪声 */
    pid->derivative = (derivative_raw + pid->derivative) / 2;

    /* 计算高级 PID 输出 */
    pid->output = (int32_t)(pid->kp * pid->error +
                            ki_adjusted * pid->integral +
                            pid->kd * pid->derivative);

    /* 限制输出上限 */
    if (pid->output > pid->max_output)
        pid->output = pid->max_output;
    /* 限制输出下限 */
    else if (pid->output < pid->min_output)
        pid->output = pid->min_output;

    /* 积分抗饱和：输出已经打满时撤销本次积分累加 */
    if ((pid->output >= pid->max_output || pid->output <= pid->min_output) &&
        abs(pid->error) <= pid->integral_depart)
    {
        pid->integral -= pid->error;
    }

    /* 保存本次误差，供下次计算微分项使用 */
    pid->last_error = pid->error;

    /* 保存本次输出，供调试或其他算法复用 */
    pid->last_output = pid->output;
}

/**
 * @brief 设置 PID 目标值，可选择启用斜坡控制。
 * @param pid PID 控制器指针。
 * @param target 最终目标值。
 * @param ramp_rate 斜坡速率百分比；传入 0 表示直接设置目标值。
 */
void USER_PID_SetTargetWithRamp(PID_Control_Struct_TypeDef *pid, int32_t target, int32_t ramp_rate)
{
    /* ramp_rate 为 0 时禁用斜坡，直接设置目标 */
    if (ramp_rate == 0)
    {
        pid->ramp_enable = false;
        pid->target = target;
        pid->ramp_target = target;
    }
    else
    {
        /* 启用斜坡控制 */
        pid->ramp_enable = true;

        /* 记录斜坡最终目标 */
        pid->ramp_target = target;

        /* 保存斜坡速率，使用绝对值防止负数输入 */
        pid->ramp_rate = abs(ramp_rate);
    }
}

/**
 * @brief 清空 PID 控制器状态并停止输出。
 * @param pid PID 控制器指针。
 */
void USER_PID_ClearAndStop(PID_Control_Struct_TypeDef *pid)
{
    /* 清空目标值 */
    pid->target = 0;

    /* 清空当前反馈值 */
    pid->current = 0;

    /* 清空误差 */
    pid->error = 0;

    /* 清空积分项 */
    pid->integral = 0;

    /* 清空微分项 */
    pid->derivative = 0;

    /* 清空输出 */
    pid->output = 0;

    /* 清空历史误差 */
    pid->last_error = 0;

    /* 清空上上次误差 */
    pid->last_last_error = 0;

    /* 清空历史输出 */
    pid->last_output = 0;

    /* 关闭斜坡控制 */
    pid->ramp_enable = false;

    /* 清空斜坡目标 */
    pid->ramp_target = 0;

    /* 清空斜坡速率 */
    pid->ramp_rate = 0;
}
