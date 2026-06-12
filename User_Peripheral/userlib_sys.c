#include "userlib_sys.h"

#define USER_SYSTEM_ADD_TIME(counter)        \
  do                                         \
  {                                          \
    if ((counter) < SYSTEM_TIME_PENDING_MAX) \
    {                                        \
      (counter)++;                           \
    }                                        \
  } while (0)

/// @brief 全局系统Tick计数器，用于记录SysTick中断的次数
uint32_t sysTick = 0;

/// @brief 系统周期任务挂起计数表
SystemTime_t system_time = {0};

/// @brief SysTick回调函数数组，用于存储不同的回调函数
void (*systick_callback[SYSTICK_CALLBACK_MAX])(void) = {0};

/// @brief 系统时间更新回调函数
/// @note 该函数会在每次SysTick中断时被调用，用于更新系统时间表
void USER_SYSTEM_UpdateTime(void);

/// @brief 用户系统初始化函数
void USER_SYSTEM_Init(void)
{
  // 清空回调函数数组
  memset(systick_callback, 0, sizeof(systick_callback));
  // 初始化系统Tick计数器
  sysTick = 0;
  //   // 注册系统时间更新回调函数：更新时间表
  USER_SYSTICK_RegisterCallback(USER_SYSTEM_UpdateTime);
}

/// @brief SysTick定时器中断服务程序
void SysTick_Handler(void)
{
  uint16_t i;
  sysTick++; // 增加系统Tick计数器

  /* 检查是否有注册的回调函数 */
  for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
  {
    if (systick_callback[i] != 0)
    {
      /* 调用注册的回调函数 */
      systick_callback[i]();
    }
  }
}

/// @brief 注册SysTick回调函数
/// @param callback 要注册的回调函数指针
/// @return true: 注册成功或已注册, false: 注册失败（槽位已满）
bool USER_SYSTICK_RegisterCallback(void (*callback)(void))
{
  uint16_t i;

  /* 输入参数检查 */
  if (callback == 0)
  {
    return false;
  }

  /* 如果回调函数已经注册，直接返回成功，避免重复占用槽位 */
  for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
  {
    if (systick_callback[i] == callback)
    {
      return true;
    }
  }

  /* 查找第一个空闲的回调函数槽位 */
  for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
  {
    if (systick_callback[i] == 0)
    {
      /* 注册回调函数 */
      systick_callback[i] = callback;
      return true; /* 成功注册回调函数 */
    }
  }
  /* 如果没有空闲槽位，返回失败 */
  return false;
}

/// @brief 注销SysTick回调函数
/// @param callback 要注销的回调函数指针
/// @return true: 注销成功, false: 注销失败（未找到该函数）
bool USER_SYSTICK_UnregisterCallback(void (*callback)(void))
{
  uint16_t i;

  /* 输入参数检查 */
  if (callback == 0)
  {
    return false;
  }

  /* 查找并取消注册回调函数 */
  for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
  {
    if (systick_callback[i] == callback)
    {
      systick_callback[i] = 0; /* 清除回调函数 */
      return true;             /* 成功取消注册 */
    }
  }
  /* 如果没有找到回调函数，返回失败 */
  return false;
}

/// @brief 获取已注册的回调函数数量
/// @return 已注册的回调函数数量
uint8_t USER_SYSTICK_GetCallbackCount(void)
{
  uint8_t count;
  uint16_t i;

  count = 0;
  for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
  {
    if (systick_callback[i] != 0)
    {
      count++;
    }
  }
  return count;
}

/// @brief 检查指定回调函数是否已注册
/// @param callback 要检查的回调函数指针
/// @return true: 已注册, false: 未注册
bool USER_SYSTICK_IsCallbackRegistered(void (*callback)(void))
{
  uint16_t i;

  if (callback == 0)
  {
    return false;
  }

  for (i = 0; i < SYSTICK_CALLBACK_MAX; i++)
  {
    if (systick_callback[i] == callback)
    {
      return true;
    }
  }
  return false;
}

/// @brief 清除所有已注册的回调函数
void USER_SYSTICK_ClearAllCallbacks(void)
{
  memset(systick_callback, 0, sizeof(systick_callback));
}

/// @brief 消耗一个周期任务挂起计数
bool USER_SYSTEM_ConsumeTime(volatile uint8_t *time_counter)
{
  uint32_t primask;
  bool has_pending;

  if (time_counter == 0)
  {
    return false;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  has_pending = (*time_counter > 0U);
  if (has_pending)
  {
    (*time_counter)--;
  }

  __set_PRIMASK(primask);
  return has_pending;
}

// 注意：此处使用随机数起始，尽量减少同一时刻被触发的标志数量，分散CPU负载
uint16_t cnt_2ms = 0;
uint16_t cnt_5ms = 3;
uint16_t cnt_10ms = 7;
uint16_t cnt_20ms = 11;
uint16_t cnt_50ms = 37;
uint16_t cnt_100ms = 83;
uint16_t cnt_200ms = 121;
uint16_t cnt_500ms = 357;
uint16_t cnt_1000ms = 777;

/// @brief 系统时间表更新程序（SysTick 回调）
/// @details 由 SysTick 中断每 1ms 调用一次，通过计数器分频累加 1ms/2ms/5ms/10ms/
///          20ms/50ms/100ms/200ms/500ms/1000ms 周期任务挂起次数。
///          主循环通过 USER_SYSTEM_ConsumeTime() 消耗挂起次数，避免二值标志丢周期。
void USER_SYSTEM_UpdateTime(void)
{

  /* 1ms 周期任务挂起次数 */
  USER_SYSTEM_ADD_TIME(system_time.t_1ms);

  /* 2ms定时器 */
  cnt_2ms++;
  if (cnt_2ms >= 2)
  {
    cnt_2ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_2ms);
  }

  /* 5ms定时器 */
  cnt_5ms++;
  if (cnt_5ms >= 5)
  {
    cnt_5ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_5ms);
  }

  /* 10ms定时器 */
  cnt_10ms++;
  if (cnt_10ms >= 10)
  {
    cnt_10ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_10ms);
  }

  /* 20ms定时器 */
  cnt_20ms++;
  if (cnt_20ms >= 20)
  {
    cnt_20ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_20ms);
  }

  /* 50ms定时器 */
  cnt_50ms++;
  if (cnt_50ms >= 50)
  {
    cnt_50ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_50ms);
  }

  /* 100ms定时器 */
  cnt_100ms++;
  if (cnt_100ms >= 100)
  {
    cnt_100ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_100ms);
  }

  /* 200ms定时器 */
  cnt_200ms++;
  if (cnt_200ms >= 200)
  {
    cnt_200ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_200ms);
  }

  /* 500ms定时器 */
  cnt_500ms++;
  if (cnt_500ms >= 500)
  {
    cnt_500ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_500ms);
  }

  /* 1000ms定时器 */
  cnt_1000ms++;
  if (cnt_1000ms >= 1000)
  {
    cnt_1000ms = 0;
    USER_SYSTEM_ADD_TIME(system_time.t_1000ms);
  }
}
