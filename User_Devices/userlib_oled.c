/**
 * @file    userlib_oled.c
 * @brief   OLED 128×64 (SSD1306 / SH1106 兼容) 显示驱动核心。
 *
 * ============================================================================
 * 概述
 * ============================================================================
 * 本文件实现了基于 SPI + DMA 的 OLED 单色显示屏驱动，包含：
 *   - 硬件初始化 / 反初始化（SPI、DMA、GPIO）
 *   - 全屏 GRAM 缓冲区管理与 DMA 限帧刷新
 *   - 基础图形绘制：点、水平线、垂直线、任意斜线、虚线、矩形、圆、圆弧
 *   - 字符与数值格式化输出（字符串、十六进制、无符号/有符号整数、浮点数）
 *   - 进度条（Bar）与波形（Wave）辅助显示
 *   - 对比度、开关显示、反色等显示控制接口
 *
 * ============================================================================
 * 缓冲区策略
 * ============================================================================
 * GRAM[8][128] 是片内全帧缓冲区，MCU 所有绘制均在 GRAM 中完成。
 * USER_OLED_Service() 在 DMA 空闲且画面变脏时按固定周期将 GRAM 发送至 OLED。
 * 绘制函数仅修改 GRAM，不直接操作 OLED 硬件，因此可以批量更新后
 * 由 DMA 自动同步到屏幕，减少 SPI 阻塞等待。
 *
 * ============================================================================
 * 坐标系约定
 * ============================================================================
 * - X: 0 ~ 127（左→右），Y: 0 ~ 63（上→下）
 * - Page: 0 ~ 7，每页 8 像素高（与 SSD1306 页地址模式对应）
 * - GRAM[row][col] 中 row 使用翻转映射（7 - page），使 Y=0 对应屏幕顶部
 * - 绘制 API 全部使用 X/Y 坐标，内部自行完成 page/bit 转换
 *
 * ============================================================================
 * 字库加载策略
 * ============================================================================
 * FontLib 字库定义在独立的 userlib_fonts.c 中，通过 userlib_fonts.h 外部引用。
 * IAR 工程需要显式加入 User_Devices/userlib_fonts.c，避免 OLED 驱动文件继续内联字库。
 *
 * ============================================================================
 * 使用约束
 * ============================================================================
 * - 调用任何绘制 API 前须先调用 USER_OLED_Init() 完成初始化。
 * - 所有绘制函数假定在非 ISR 上下文中调用；SPI ISR 仅负责 DMA 重传。
 * - 浮点输出仅做定点化格式转换，不依赖 math 库三角或分类函数。
 */

/*===========================================================================
 * 头文件包含
 *===========================================================================*/

#include "userlib_oled.h"
#include "userlib_fonts.h"

#include <float.h>
#include <limits.h>
#include <string.h>

/*===========================================================================
 * 硬件相关宏定义
 *===========================================================================*/

#define CPU_FRQ ((uint32_t)80000000)
#define BUS_FRQ ((uint32_t)80000000)
#define CLK_PER_MS (BUS_FRQ / 1000u)

#define isReversed OLED_Dir_Normal

#define SPI1_NRST_GPIO_Port OLED_PORT
#define SPI1_NRST_Pin OLED_NRST_PIN
#define SPI1_DC_GPIO_Port OLED_PORT
#define SPI1_DC_Pin OLED_DC_PIN
#define SPI1_CH SPI_1_INST
#define SPI1_DMA_CH DMA_CH_SPI0_TX_CHAN_ID

/*===========================================================================
 * OLED 面板属性宏
 *===========================================================================*/

#define OLED_WIDTH 128u
#define OLED_HEIGHT 64u
#define OLED_PAGE_COUNT 8u
#define OLED_CHAR_WIDTH 6u
#define OLED_CHARS_PER_ROW 21u
#define OLED_DEFAULT_BRIGHTNESS 0xFFu

/*===========================================================================
 * SSD1306 命令码宏
 *===========================================================================*/

#define OLED_cmd_DisplayOFF 0xAEu
#define OLED_cmd_DisplayON 0xAFu
#define OLED_cmd_ScrSyncGram 0xA4u
#define OLED_cmd_NormalDisplay 0xA6u
#define OLED_cmd_ReverseDisplay 0xA7u
#define OLED_cmd_SetContrast 0x81u
#define OLED_cmd_SetMainClock 0xD5u
#define OLED_cmd_SetPrechgPeriod 0xD9u
#define OLED_cmd_PrechgPeriod_Normal 0xF1u
#define OLED_cmd_SetComPinMode 0xDAu
#define OLED_cmd_ComPinMode_Normal 0x12u
#define OLED_cmd_SetComplexRatio 0xA8u
#define OLED_cmd_ComplexRatio_Normal 0x3Fu
#define OLED_cmd_SetVCOMH 0xDBu
#define OLED_cmd_VCOMH_Normal 0x40u
#define OLED_cmd_SetChgPump 0x8Du
#define OLED_cmd_ChgPump_OFF 0x10u
#define OLED_cmd_ChgPump_7V5 0x14u
#define OLED_cmd_SetRamAddrMode 0x20u
#define OLED_cmd_RamAddrMode_Row 0x00u
#define OLED_cmd_SetVirticalShift 0xD3u
#define OLED_cmd_VirticalShift_None 0x00u
#define OLED_cmd_HorizonalDirL2R 0xA1u
#define OLED_cmd_HorizonalDirR2L 0xA0u
#define OLED_cmd_VirticalDirU2D 0xC8u
#define OLED_cmd_VirticalDirD2U 0xC0u

/*===========================================================================
 * 全局缓冲区与状态变量
 *===========================================================================*/

#define OLED_FRAME_BUFFER_SIZE (OLED_PAGE_COUNT * OLED_WIDTH)
#define OLED_REFRESH_PERIOD_MS 10u
#define OLED_PENDING_CONTRAST 0x01u
#define OLED_PENDING_DISPLAY 0x02u
#define OLED_PENDING_INVERT 0x04u

uint8_t GRAM[OLED_PAGE_COUNT][OLED_WIDTH] = {0};
uint8_t WaveRAM[OLED_WIDTH] = {0};
uint8_t BarRAM[OLED_PAGE_COUNT] = {0};

static char str_temp[22] = {0};
static uint8_t isOLED_Initialized = 0u;
static volatile uint8_t oled_dma_busy = 0u;
static uint8_t oled_frame_dirty = 1u;
static uint32_t oled_next_refresh_tick = 0u;
static uint8_t oled_pending_commands = 0u;
static uint8_t oled_pending_contrast = OLED_DEFAULT_BRIGHTNESS;
static bool oled_pending_display_on = true;
static bool oled_pending_invert = false;

#define USER_OLED_MARK_DIRTY() \
  do                            \
  {                             \
    oled_frame_dirty = 1u;      \
  } while (0)

#define USER_OLED_POINT_MASK(y_) ((uint8_t)(0x80u >> ((y_) & 0x07u)))
#define USER_OLED_ROW_FROM_Y(y_) ((uint8_t)(7u - ((y_) >> 3)))

#define USER_OLED_OR_BYTE(row_, x_, mask_)                         \
  do                                                               \
  {                                                                \
    uint8_t *cell__ = &GRAM[(uint8_t)(row_)][(uint8_t)(x_)];       \
    uint8_t value__ = (uint8_t)(*cell__ | (uint8_t)(mask_));       \
    if (value__ != *cell__)                                        \
    {                                                              \
      *cell__ = value__;                                           \
      USER_OLED_MARK_DIRTY();                                      \
    }                                                              \
  } while (0)

#define USER_OLED_AND_BYTE(row_, x_, mask_)                        \
  do                                                               \
  {                                                                \
    uint8_t *cell__ = &GRAM[(uint8_t)(row_)][(uint8_t)(x_)];       \
    uint8_t value__ = (uint8_t)(*cell__ & (uint8_t)(mask_));       \
    if (value__ != *cell__)                                        \
    {                                                              \
      *cell__ = value__;                                           \
      USER_OLED_MARK_DIRTY();                                      \
    }                                                              \
  } while (0)

#define USER_OLED_SET_POINT_FAST(x_, y_) \
  USER_OLED_OR_BYTE(USER_OLED_ROW_FROM_Y(y_), (x_), USER_OLED_POINT_MASK(y_))

#define USER_OLED_RESET_POINT_FAST(x_, y_) \
  USER_OLED_AND_BYTE(USER_OLED_ROW_FROM_Y(y_), (x_), (uint8_t)(~USER_OLED_POINT_MASK(y_)))

static inline void USER_OLED_SetByteIfChanged(uint8_t *dst, uint8_t value)
{
  if (*dst != value)
  {
    *dst = value;
    USER_OLED_MARK_DIRTY();
  }
}

static void USER_OLED_SetBytes(uint8_t *dst, uint8_t value, uint16_t len)
{
  while (len > 0u)
  {
    USER_OLED_SetByteIfChanged(dst, value);
    dst++;
    len--;
  }
}

static inline void USER_OLED_CopyFont6(uint8_t *dst, const uint8_t *src)
{
  USER_OLED_SetByteIfChanged(&dst[0], src[0]);
  USER_OLED_SetByteIfChanged(&dst[1], src[1]);
  USER_OLED_SetByteIfChanged(&dst[2], src[2]);
  USER_OLED_SetByteIfChanged(&dst[3], src[3]);
  USER_OLED_SetByteIfChanged(&dst[4], src[4]);
  USER_OLED_SetByteIfChanged(&dst[5], src[5]);
}

static uint16_t USER_OLED_NormalizeAngle(uint16_t angle)
{
  while (angle >= 360u)
  {
    angle = (uint16_t)(angle - 360u);
  }
  return angle;
}

/*===========================================================================
 * 内部辅助函数：硬件抽象层
 *===========================================================================*/

/**
 * @brief OLED 驱动内部毫秒级阻塞延时
 *
 * @param ms 延时时间，单位 ms。
 *
 * @note 本函数基于 delay_cycles() 实现，主要用于 OLED 复位和初始化命令间隔。
 */
static void __User_OLED_Delay(uint16_t ms)
{
  delay_cycles((uint32_t)CLK_PER_MS * ms);
}

/**
 * @brief 硬件初始化：使能 SPI 中断。
 *
 * @note 本函数在 USER_OLED_Init() 中调用，仅使能 SPI 对应 NVIC 中断线，
 *       不涉及 SPI 外设时钟或 GPIO 配置（这些由 SysConfig 生成代码处理）。
 */
static void __User_OLED_HW_Init(void)
{
  NVIC_EnableIRQ(SPI_1_INST_INT_IRQN);
}

/**
 * @brief 硬件反初始化（预留）。
 *
 * @note 当前实现为空，预留用于未来关闭 SPI 中断或释放 GPIO 资源。
 */
static void __User_OLED_HW_DeInit(void)
{
}

/**
 * @brief 设置 SPI 传输模式为数据（DC 引脚拉高）。
 *
 * @note OLED DC 引脚高电平表示接下来 SPI 发送的是显示数据（GRAM 内容），
 *       低电平表示发送的是命令。本函数在 DMA 限帧刷新前调用。
 */
static void __User_OLED_SetTxMode_Data(void)
{
  DL_GPIO_setPins(SPI1_DC_GPIO_Port, SPI1_DC_Pin);
}

/**
 * @brief 设置 SPI 传输模式为命令（DC 引脚拉低）。
 *
 * @note OLED DC 引脚低电平表示接下来 SPI 发送的是命令字节。
 *       所有 SSD1306 命令（对比度、显示开关、扫描方向等）发送前均需调用本函数。
 */
static void __User_OLED_SetTxMode_Cmd(void)
{
  DL_GPIO_clearPins(SPI1_DC_GPIO_Port, SPI1_DC_Pin);
}

/**
 * @brief 释放 OLED 复位引脚（NRST 拉高，正常工作状态）。
 *
 * @note NRST 高电平使 OLED 退出复位，进入正常工作模式。
 *       初始化序列中与 __User_OLED_GoReset() 配合完成硬件复位。
 */
static void __User_OLED_GoNormal(void)
{
  DL_GPIO_setPins(SPI1_NRST_GPIO_Port, SPI1_NRST_Pin);
}

/**
 * @brief 拉低 OLED 复位引脚（NRST 拉低，复位状态）。
 *
 * @note NRST 低电平使 OLED 进入硬件复位，内部寄存器恢复默认值。
 */
static void __User_OLED_GoReset(void)
{
  DL_GPIO_clearPins(SPI1_NRST_GPIO_Port, SPI1_NRST_Pin);
}

/**
 * @brief SPI DMA 发送：禁用旧通道后重新配置源/目标/大小并启动。
 *
 * @param pdat 指向发送数据缓冲区的指针。
 * @param size 本次 DMA 传输的字节数。
 *
 * @note 每次调用前先禁用 DMA 通道（若已使能），再重新配置源地址、
 *       目标地址（SPI TXDATA 寄存器）和传输大小，最后使能通道。
 *       用于 GRAM 全帧（1024 字节）连续 DMA 刷新。
 */
static void __User_OLED_SPI_Transmit_DMA(uint8_t *pdat, uint16_t size)
{
  if (DL_DMA_isChannelEnabled(DMA, SPI1_DMA_CH))
  {
    DL_DMA_disableChannel(DMA, SPI1_DMA_CH);
  }

  DL_DMA_setSrcAddr(DMA, SPI1_DMA_CH, (uint32_t)pdat);
  DL_DMA_setDestAddr(DMA, SPI1_DMA_CH, (uint32_t)(&SPI1_CH->TXDATA));
  DL_DMA_setTransferSize(DMA, SPI1_DMA_CH, size);
  DL_DMA_enableChannel(DMA, SPI1_DMA_CH);
}

static bool USER_OLED_TickReached(uint32_t now, uint32_t target)
{
  return ((int32_t)(now - target) >= 0);
}

static void USER_OLED_StartFrameTransfer(void)
{
  oled_frame_dirty = 0u;
  oled_dma_busy = 1u;
  __User_OLED_SetTxMode_Data();
  __User_OLED_SPI_Transmit_DMA(&GRAM[0][0],
                               OLED_FRAME_BUFFER_SIZE);
}

/**
 * @brief SPI 阻塞发送单个字节并延时等待。
 *
 * @param pdat   指向待发送数据的指针（实际只发送首字节）。
 * @param size   未使用（保留参数，兼容 DMA 发送接口）。
 * @param timeout 发送后阻塞延时，单位 ms。
 *
 * @note 本函数用于 OLED 初始化阶段逐字节发送命令，要求阻塞等待以保证
 *       命令时序。正常刷新使用 DMA 方式（__User_OLED_SPI_Transmit_DMA）。
 */
static void __User_OLED_SPI_Transmit(uint8_t *pdat, uint16_t size, uint16_t timeout)
{
  (void)size;
  (void)timeout;
  DL_SPI_transmitDataBlocking8(SPI1_CH, *pdat);
  while (DL_SPI_isBusy(SPI1_CH))
  {
  }
}

/**
 * @brief 通过 SPI 发送一个命令/数据字节。
 *
 * @param data 要发送的字节（SSD1306 命令码或显示数据）。
 *
 * @note 本函数是 SPI 阻塞发送的薄封装，固定 10ms 超时。
 *       调用前需先通过 __User_OLED_SetTxMode_Cmd/Data 设置 DC 引脚电平。
 */
static void __User_OLED_Send(uint8_t data)
{
  __User_OLED_SPI_Transmit(&data, 1u, 10u);
}

static void USER_OLED_ApplyPendingCommands(void)
{
  uint8_t pending = oled_pending_commands;

  if (pending == 0u)
  {
    return;
  }

  oled_pending_commands = 0u;
  __User_OLED_SetTxMode_Cmd();

  if ((pending & OLED_PENDING_CONTRAST) != 0u)
  {
    __User_OLED_Send(OLED_cmd_SetContrast);
    __User_OLED_Send(oled_pending_contrast);
  }
  if ((pending & OLED_PENDING_DISPLAY) != 0u)
  {
    __User_OLED_Send(oled_pending_display_on ? OLED_cmd_DisplayON
                                             : OLED_cmd_DisplayOFF);
  }
  if ((pending & OLED_PENDING_INVERT) != 0u)
  {
    __User_OLED_Send(oled_pending_invert ? OLED_cmd_ReverseDisplay
                                         : OLED_cmd_NormalDisplay);
  }

  __User_OLED_SetTxMode_Data();
}

/*===========================================================================
 * 内部辅助函数：像素位掩码与快速点操作
 *
 * GRAM 按页（page）组织，每页 8 个像素行。
 * Y 坐标 b2..b0 决定在页内的 bit 位置（0x80 >> (y & 0x07)）。
 * 行号 row = 7 - page，使得 Y=0 对应屏幕顶部。
 *===========================================================================*/

/**
 * @brief 根据 Y 坐标生成 OLED 页内像素掩码
 *
 * @param y 像素 Y 坐标，调用者需要保证坐标已经完成边界检查。
 *
 * @return 当前像素在页内对应的 bit mask。
 */
static inline uint8_t USER_OLED_MakePointMask(uint8_t y)
{
  return USER_OLED_POINT_MASK(y);
}

/**
 * @brief 生成页内连续位掩码（bit_start ~ bit_end 范围内的像素点亮）。
 *
 * @param bit_start 起始位（0~7，0 = MSB，对应页内顶部像素）。
 * @param bit_end   结束位（0~7，>= bit_start）。
 *
 * @return 页内连续位掩码，bit_start 到 bit_end 对应位为 1，其余为 0。
 *
 * @note 用于垂直线和矩形填充中，跨页边界时分别计算各页掩码。
 */
static inline uint8_t USER_OLED_MakeYMask(uint8_t bit_start, uint8_t bit_end)
{
  return (uint8_t)(((uint8_t)(0xFFu >> bit_start)) &
                   ((uint8_t)(0xFFu << (7u - bit_end))));
}

/**
 * @brief 快速设置点（无边界检查，直接写入 GRAM）。
 *
 * @param x 像素 X 坐标，调用者保证已完成边界检查。
 * @param y 像素 Y 坐标，调用者保证已完成边界检查。
 *
 * @note 直接操作 GRAM[row][x]，不做任何边界裁剪。
 *       适合已被上层 API 验证坐标的内部路径使用。
 */
static inline void USER_OLED_SetPointFast(uint8_t x, uint8_t y)
{
  USER_OLED_SET_POINT_FAST(x, y);
}

/**
 * @brief 快速清除点（无边界检查，直接写入 GRAM）。
 *
 * @param x 像素 X 坐标，调用者保证已完成边界检查。
 * @param y 像素 Y 坐标，调用者保证已完成边界检查。
 *
 * @note 直接清除 GRAM[row][x] 中对应位，不做任何边界裁剪。
 */
static inline void USER_OLED_ResetPointFast(uint8_t x, uint8_t y)
{
  USER_OLED_RESET_POINT_FAST(x, y);
}

/**
 * @brief 带边界裁剪的点设置（越界则忽略）。
 *
 * @param x 像素 X 坐标（支持负值越界裁剪）。
 * @param y 像素 Y 坐标（支持负值越界裁剪）。
 *
 * @note 使用 int16_t 参数支持 Bresenham / 圆形算法中可能产生的负坐标，
 *       自动过滤越界像素，确保 GRAM 不会被越界写入。
 */
static inline void USER_OLED_SetPointClipped(int16_t x, int16_t y)
{
  if ((x >= 0) && (x < (int16_t)OLED_WIDTH) && (y >= 0) && (y < (int16_t)OLED_HEIGHT))
  {
    USER_OLED_SetPointFast((uint8_t)x, (uint8_t)y);
  }
}

/*===========================================================================
 * 内部辅助函数：快速水平线 / 垂直线 / 矩形填充
 *
 * 这些 Fast 函数不做边界检查，由上层公开 API 保证参数合法性。
 * 直接操作 GRAM，使用预计算的 page / mask 避免逐点计算开销。
 *===========================================================================*/

/**
 * @brief 快速绘制水平线
 *
 * @param x1 起点 X 坐标，要求已完成边界检查。
 * @param x2 终点 X 坐标，要求已完成边界检查且 x1 <= x2。
 * @param y  水平线所在 Y 坐标，要求已完成边界检查。
 *
 * @note 本函数直接操作 GRAM，不再逐点调用公开 API，适合被矩形和圆形填充路径复用。
 */
static inline void USER_OLED_DrawHLineFast(uint8_t x1, uint8_t x2, uint8_t y)
{
  const uint8_t row = (uint8_t)(7u - (y >> 3));
  const uint8_t mask = USER_OLED_MakePointMask(y);
  uint8_t *dst = &GRAM[row][x1];
  uint8_t count = (uint8_t)(x2 - x1 + 1u);

  while (count > 0u)
  {
    const uint8_t value = (uint8_t)(*dst | mask);
    if (value != *dst)
    {
      *dst = value;
      USER_OLED_MARK_DIRTY();
    }
    dst++;
    count--;
  }
}

/**
 * @brief 带边界裁剪的水平线绘制（越界自动截断，自动交换坐标）。
 *
 * @param x1 起点 X 坐标，支持任意顺序（自动交换）。
 * @param x2 终点 X 坐标，支持任意顺序（自动交换）。
 * @param y  水平线所在 Y 坐标。
 *
 * @note 越界部分自动截断至屏幕范围，完全在屏幕外的线段直接返回。
 *       被 USER_OLED_DrawCircle(fill=true) 等填充路径复用。
 */
static void USER_OLED_DrawHLineClipped(int16_t x1, int16_t x2, int16_t y)
{
  if ((y < 0) || (y >= (int16_t)OLED_HEIGHT))
  {
    return;
  }
  if (x1 > x2)
  {
    int16_t temp = x1;
    x1 = x2;
    x2 = temp;
  }
  if ((x2 < 0) || (x1 >= (int16_t)OLED_WIDTH))
  {
    return;
  }
  if (x1 < 0)
  {
    x1 = 0;
  }
  if (x2 >= (int16_t)OLED_WIDTH)
  {
    x2 = (int16_t)OLED_WIDTH - 1;
  }
  USER_OLED_DrawHLineFast((uint8_t)x1, (uint8_t)x2, (uint8_t)y);
}

/**
 * @brief 快速绘制垂直线（无边界检查）。
 *
 * @param x  垂直线所在 X 坐标，调用者保证已完成边界检查。
 * @param y1 起点 Y 坐标，调用者保证已完成边界检查且 y1 <= y2。
 * @param y2 终点 Y 坐标，调用者保证已完成边界检查且 y1 <= y2。
 *
 * @note 直接操作 GRAM，按页遍历，自动处理跨页边界的掩码分段。
 *       适合被矩形边框绘制路径复用。
 */
static inline void USER_OLED_DrawVLineFast(uint8_t x, uint8_t y1, uint8_t y2)
{
  const uint8_t page_start = (uint8_t)(y1 >> 3);
  const uint8_t page_end = (uint8_t)(y2 >> 3);

  for (uint8_t page = page_start; page <= page_end; page++)
  {
    const uint8_t bit_start = (page == page_start) ? (uint8_t)(y1 & 0x07u) : 0u;
    const uint8_t bit_end = (page == page_end) ? (uint8_t)(y2 & 0x07u) : 7u;
    const uint8_t row = (uint8_t)(7u - page);
    USER_OLED_OR_BYTE(row, x, USER_OLED_MakeYMask(bit_start, bit_end));
  }
}

/**
 * @brief 快速填充矩形（无边界检查）。
 *
 * @param x1 左上角 X 坐标，调用者保证已完成边界检查且 x1 <= x2。
 * @param y1 左上角 Y 坐标，调用者保证已完成边界检查且 y1 <= y2。
 * @param x2 右下角 X 坐标，调用者保证已完成边界检查且 x1 <= x2。
 * @param y2 右下角 Y 坐标，调用者保证已完成边界检查且 y1 <= y2。
 *
 * @note 直接操作 GRAM，按页遍历填充。当页内掩码为 0xFF 时使用 memset
 *       批量填充以提升性能，否则逐列 OR 写入掩码。
 */
static inline void USER_OLED_FillRectFast(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
  const uint16_t width = (uint16_t)x2 - (uint16_t)x1 + 1u;
  const uint8_t page_start = (uint8_t)(y1 >> 3);
  const uint8_t page_end = (uint8_t)(y2 >> 3);

  for (uint8_t page = page_start; page <= page_end; page++)
  {
    const uint8_t bit_start = (page == page_start) ? (uint8_t)(y1 & 0x07u) : 0u;
    const uint8_t bit_end = (page == page_end) ? (uint8_t)(y2 & 0x07u) : 7u;
    const uint8_t mask = USER_OLED_MakeYMask(bit_start, bit_end);
    const uint8_t row = (uint8_t)(7u - page);

    if (mask == 0xFFu)
    {
      USER_OLED_SetBytes(&GRAM[row][x1], 0xFF, width);
    }
    else
    {
      for (uint16_t i = 0u; i < width; i++)
      {
        USER_OLED_OR_BYTE(row, (uint8_t)(x1 + i), mask);
      }
    }
  }
}

/*===========================================================================
 * OLED 初始化与反初始化
 *
 * USER_OLED_Init() 执行步骤：
 *   1. 硬件初始化（SPI 时钟、DMA 通道、GPIO）
 *   2. 硬件复位序列（NRST 引脚）
 *   3. 发送 SSD1306 初始化命令序列（charge pump、对比度、扫描方向等）
 *   4. 清空 GRAM / WaveRAM 缓冲区
 *   5. 开启显示并启动 DMA 限帧刷新
 *===========================================================================*/
/**
 * @brief OLED 初始化：硬件配置 → 复位 → SSD1306 命令序列 → 清屏 → DMA 刷新。
 *
 * @return OLED_Status_t
 * @retval OLED_OK         初始化成功。
 * @retval OLED_ERROR_INIT 已初始化，重复调用返回错误。
 *
 * @note 初始化步骤：
 *       1. 使能 SPI NVIC 中断；
 *       2. NRST 引脚 H→L→H 硬件复位（10ms / 10ms / 200ms 延时）；
 *       3. 发送 SSD1306 初始化命令（电荷泵、对比度、扫描方向、时钟等）；
 *       4. 清零 GRAM / WaveRAM 缓冲区；
 *       5. 开启显示并启动 1024 字节 DMA 限帧刷新。
 *
 * @warning 本函数不可重入，重复调用返回 OLED_ERROR_INIT。
 */
OLED_Status_t USER_OLED_Init(void)
{
  if (isOLED_Initialized)
  {
    return OLED_ERROR_INIT;
  }

  /*--- 步骤1: 硬件初始化（SPI / DMA / GPIO） ---*/
  __User_OLED_HW_Init();

  /*--- 步骤2: 硬件复位序列 NRST: H → L → H ---*/
  __User_OLED_GoNormal();
  __User_OLED_Delay(10u);
  __User_OLED_GoReset();
  __User_OLED_Delay(10u);
  __User_OLED_GoNormal();
  __User_OLED_Delay(200u);

  /*--- 步骤3: 发送 SSD1306 初始化命令序列 ---*/
  __User_OLED_SetTxMode_Cmd();

  /* 3a. 电荷泵配置：先关闭再设对比度 */
  __User_OLED_Send(OLED_cmd_SetChgPump);
  __User_OLED_Send(OLED_cmd_ChgPump_OFF);
  __User_OLED_Send(OLED_cmd_DisplayOFF);
  __User_OLED_Send(0x40u); /* 显示起始行 */
  __User_OLED_Send(0x00u);
  __User_OLED_Send(OLED_cmd_SetContrast);
  __User_OLED_Send(OLED_DEFAULT_BRIGHTNESS);

  /* 3b. 扫描方向：根据 isReversed 选择正向或反向 */
  if (isReversed == OLED_Dir_Reverse)
  {
    __User_OLED_Send(OLED_cmd_VirticalDirD2U);
    __User_OLED_Send(OLED_cmd_HorizonalDirR2L);
  }
  else
  {
    __User_OLED_Send(OLED_cmd_VirticalDirU2D);
    __User_OLED_Send(OLED_cmd_HorizonalDirL2R);
  }

  /* 3c. 显示参数配置 */
  __User_OLED_Send(OLED_cmd_NormalDisplay);
  __User_OLED_Send(OLED_cmd_SetComplexRatio); /* 复用比 */
  __User_OLED_Send(OLED_cmd_ComplexRatio_Normal);
  __User_OLED_Send(OLED_cmd_SetVirticalShift); /* 垂直偏移 */
  __User_OLED_Send(OLED_cmd_VirticalShift_None);
  __User_OLED_Send(OLED_cmd_SetMainClock); /* 主时钟 */
  __User_OLED_Send(0xF0u);
  __User_OLED_Send(OLED_cmd_SetPrechgPeriod); /* 预充电周期 */
  __User_OLED_Send(OLED_cmd_PrechgPeriod_Normal);
  __User_OLED_Send(OLED_cmd_SetComPinMode); /* COM 引脚配置 */
  __User_OLED_Send(OLED_cmd_ComPinMode_Normal);
  __User_OLED_Send(OLED_cmd_SetVCOMH); /* VCOMH 电压 */
  __User_OLED_Send(OLED_cmd_VCOMH_Normal);

  /* 3d. 地址模式与电荷泵使能 */
  __User_OLED_Send(OLED_cmd_SetRamAddrMode); /* 行地址模式 */
  __User_OLED_Send(OLED_cmd_RamAddrMode_Row);
  __User_OLED_Send(OLED_cmd_ScrSyncGram); /* 从 GRAM 同步显示 */
  __User_OLED_Send(OLED_cmd_SetChgPump);
  __User_OLED_Send(OLED_cmd_ChgPump_7V5); /* 使能电荷泵 7.5V */
  __User_OLED_Delay(10u);

  /*--- 步骤4: 清空缓冲区 ---*/
  isOLED_Initialized = 1u;
  memset(&GRAM[0][0], 0x00, OLED_FRAME_BUFFER_SIZE);
  memset(WaveRAM, 0x00, sizeof(WaveRAM));
  memset(BarRAM, 0xFF, sizeof(BarRAM));
  oled_pending_commands = 0u;
  oled_frame_dirty = 1u;

  /*--- 步骤5: 开启显示并启动 DMA 限帧刷新 ---*/
  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(OLED_cmd_DisplayON);
  __User_OLED_Delay(10u);
  oled_dma_busy = 0u;
  oled_next_refresh_tick = sysTick;
  USER_OLED_Service();
  return OLED_OK;
}

/*===========================================================================
 * OLED 反初始化
 *===========================================================================*/

/**
 * @brief OLED 反初始化：停止 DMA 刷新并清理状态。
 *
 * @return OLED_Status_t
 * @retval OLED_OK         反初始化成功。
 * @retval OLED_ERROR_INIT 尚未初始化，无需反初始化。
 *
 * @note 调用后停止新的帧传输请求，当前 DMA 状态被清除。
 *       当前不关闭 SPI 外设时钟（__User_OLED_HW_DeInit 预留）。
 */
OLED_Status_t USER_OLED_DeInit(void)
{
  if (!isOLED_Initialized)
  {
    return OLED_ERROR_INIT;
  }
  __User_OLED_HW_DeInit();
  if (DL_DMA_isChannelEnabled(DMA, SPI1_DMA_CH))
  {
    DL_DMA_disableChannel(DMA, SPI1_DMA_CH);
  }
  oled_dma_busy = 0u;
  oled_frame_dirty = 0u;
  isOLED_Initialized = 0u;
  return OLED_OK;
}

void USER_OLED_Service(void)
{
  uint32_t now;

  if (!isOLED_Initialized || oled_dma_busy)
  {
    return;
  }

  USER_OLED_ApplyPendingCommands();

  now = sysTick;
  if (!USER_OLED_TickReached(now, oled_next_refresh_tick))
  {
    return;
  }

  do
  {
    oled_next_refresh_tick += OLED_REFRESH_PERIOD_MS;
  } while (USER_OLED_TickReached(now, oled_next_refresh_tick));

  if (oled_frame_dirty == 0u)
  {
    return;
  }

  USER_OLED_StartFrameTransfer();
}

/*===========================================================================
 * 屏幕缓冲区操作：清屏 / 清行
 *===========================================================================*/

/**
 * @brief 清屏：将 GRAM 和 WaveRAM 全部清零。
 *
 * @note 调用后整个屏幕变为全黑，波形缓冲区同步清空。
 *       DMA 会在下一轮自动将清零后的 GRAM 发送至 OLED。
 */
void USER_OLED_CleanScreen(void)
{
  memset(&GRAM[0][0], 0x00, OLED_FRAME_BUFFER_SIZE);
  memset(WaveRAM, 0x00, sizeof(WaveRAM));
  memset(BarRAM, 0xFF, sizeof(BarRAM));
  USER_OLED_MARK_DIRTY();
}

/**
 * @brief 清除指定行（页）的 GRAM 内容。
 *
 * @param row 页号（0~7），越界则忽略。
 *
 * @note 仅清零 GRAM 对应页的 128 字节，不影响 WaveRAM 和其他页。
 */
void USER_OLED_CleanRow(uint8_t row)
{
  if (row < OLED_PAGE_COUNT)
  {
    USER_OLED_SetBytes(&GRAM[row][0], 0x00, OLED_WIDTH);
    BarRAM[row] = 0xFFu;
  }
}

/*===========================================================================
 * 文本输出：字符串 / 单字符
 *
 * 使用 6×8 ASCII 字库（FontLib）。
 * row: 页号 (0~7), column: 字符列 (0~20)，每字符宽 6 像素。
 * 越界字符回退为空格 (index=32)。
 *===========================================================================*/

/**
 * @brief 在指定位置显示字符串（6×8 ASCII 字库）。
 *
 * @param row    页号（0~7），越界则忽略。
 * @param column 字符列（0~20），越界则忽略。
 * @param str    指向源字符串的指针，为 NULL 则忽略。
 * @param length 要显示的字符数，为 0 则忽略。
 *
 * @note 每个字符宽 6 像素，超出屏幕右边界自动截断。
 *       越界字符（>= USER_FONT_ASCII_6X8_COUNT）回退显示空格。
 */
void USER_OLED_PutString(uint8_t row, uint8_t column, const char *str, uint8_t length)
{
  uint8_t remaining_chars;
  uint8_t x;

  if ((row >= OLED_PAGE_COUNT) || (column >= OLED_CHARS_PER_ROW) || (str == NULL) || (length == 0u))
  {
    return;
  }

  x = (uint8_t)((column << 2) + (column << 1));
  remaining_chars = (uint8_t)(OLED_CHARS_PER_ROW - column);
  if (length < remaining_chars)
  {
    remaining_chars = length;
  }

  while ((remaining_chars > 0u) && (*str != '\0'))
  {
    uint8_t index = (uint8_t)(*str++);
    if (index >= USER_FONT_ASCII_6X8_COUNT)
    {
      index = 32u;
    }
    USER_OLED_CopyFont6(&GRAM[row][x], FontLib[index]);
    x = (uint8_t)(x + OLED_CHAR_WIDTH);
    remaining_chars--;
  }
}

/**
 * @brief 在指定位置显示单个字符（6×8 ASCII 字库）。
 *
 * @param row    页号（0~7），越界则忽略。
 * @param column 字符列（0~20），越界则忽略。
 * @param ch     要显示的 ASCII 字符。
 *
 * @note 越界字符回退显示空格，仅在字模字节变化时写入 GRAM。
 */
void USER_OLED_PutChar(uint8_t row, uint8_t column, char ch)
{
  uint8_t x;

  if ((row >= OLED_PAGE_COUNT) || (column >= OLED_CHARS_PER_ROW))
  {
    return;
  }

  x = (uint8_t)((column << 2) + (column << 1));
  if (((uint16_t)x + OLED_CHAR_WIDTH) > OLED_WIDTH)
  {
    return;
  }

  uint8_t index = (uint8_t)ch;
  if (index >= USER_FONT_ASCII_6X8_COUNT)
  {
    index = 32u;
  }
  USER_OLED_CopyFont6(&GRAM[row][x], FontLib[index]);
}

/*===========================================================================
 * 数值格式化输出（内部辅助）
 *
 * USER_OLED_U16ToStr: 无符号整数→右对齐字符串，支持 2/10/16 进制。
 * 用作 putX16 / putUI16 / putI16 / putFloat 的底层格式化引擎。
 *===========================================================================*/

/**
 * @brief 无符号整数→右对齐字符串（内部格式化引擎）。
 *
 * @param num   待转换的无符号 16 位整数。
 * @param radix 进制（支持 2 / 10 / 16）。
 * @param str   输出缓冲区指针。
 * @param len   输出字符串长度（不含结尾 '\0'）。
 * @param fill  填充字符（左端不足时填充）。
 *
 * @return true  转换成功。
 * @return false 参数无效或结果超出 len 宽度。
 *
 * @note 用作 putX16 / putUI16 / putI16 / putFloat 的底层格式化引擎。
 *       输出字符串以 '\0' 结尾，调用者需保证 str 至少有 len+1 字节。
 */
static bool USER_OLED_U16ToStr(uint16_t num, uint8_t radix, uint8_t *str, uint8_t len, char fill)
{
  if ((str == NULL) || (len == 0u) || (len > 17u) || ((radix != 2u) && (radix != 10u) && (radix != 16u)))
  {
    return false;
  }

  static const char hex_chars[] = "0123456789ABCDEF";
  uint8_t digits = 1u;
  uint8_t pos = 0u;

  if (radix == 10u)
  {
    static const uint16_t dec_place[] = {10000u, 1000u, 100u, 10u, 1u};
    uint8_t place_index = 4u;

    if (num >= 10000u)
    {
      digits = 5u;
      place_index = 0u;
    }
    else if (num >= 1000u)
    {
      digits = 4u;
      place_index = 1u;
    }
    else if (num >= 100u)
    {
      digits = 3u;
      place_index = 2u;
    }
    else if (num >= 10u)
    {
      digits = 2u;
      place_index = 3u;
    }

    if (digits > len)
    {
      return false;
    }

    while (pos < (uint8_t)(len - digits))
    {
      str[pos++] = (uint8_t)fill;
    }

    while (place_index < (uint8_t)(sizeof(dec_place) / sizeof(dec_place[0])))
    {
      uint8_t digit = 0u;
      const uint16_t place = dec_place[place_index++];
      while (num >= place)
      {
        num = (uint16_t)(num - place);
        digit++;
      }
      str[pos++] = (uint8_t)('0' + digit);
    }

    str[len] = '\0';
    return true;
  }

  if (radix == 16u)
  {
    if (num > 0x0FFFu) { digits = 4u; }
    else if (num > 0x00FFu) { digits = 3u; }
    else if (num > 0x000Fu) { digits = 2u; }

    if (digits > len)
    {
      return false;
    }

    while (pos < (uint8_t)(len - digits))
    {
      str[pos++] = (uint8_t)fill;
    }

    while (digits > 0u)
    {
      digits--;
      str[pos++] = (uint8_t)hex_chars[(num >> (uint8_t)(digits << 2)) & 0x0Fu];
    }

    str[len] = '\0';
    return true;
  }

  if (num >= 0x8000u) { digits = 16u; }
  else if (num >= 0x4000u) { digits = 15u; }
  else if (num >= 0x2000u) { digits = 14u; }
  else if (num >= 0x1000u) { digits = 13u; }
  else if (num >= 0x0800u) { digits = 12u; }
  else if (num >= 0x0400u) { digits = 11u; }
  else if (num >= 0x0200u) { digits = 10u; }
  else if (num >= 0x0100u) { digits = 9u; }
  else if (num >= 0x0080u) { digits = 8u; }
  else if (num >= 0x0040u) { digits = 7u; }
  else if (num >= 0x0020u) { digits = 6u; }
  else if (num >= 0x0010u) { digits = 5u; }
  else if (num >= 0x0008u) { digits = 4u; }
  else if (num >= 0x0004u) { digits = 3u; }
  else if (num >= 0x0002u) { digits = 2u; }

  if (digits > len)
  {
    return false;
  }

  while (pos < (uint8_t)(len - digits))
  {
    str[pos++] = (uint8_t)fill;
  }

  while (digits > 0u)
  {
    digits--;
    str[pos++] = ((num & (uint16_t)(1u << digits)) != 0u) ? (uint8_t)'1' : (uint8_t)'0';
  }

  str[len] = '\0';
  return true;
}

/**
 * @brief 在指定位置以十六进制格式显示 16 位无符号整数。
 *
 * @param row    页号（0~7）。
 * @param column 字符列（0~20）。
 * @param number 待显示的 16 位无符号整数（0~65535）。
 * @param length 显示宽度（1~4 字符），左端补 '0'。
 */
void USER_OLED_PutX16(uint8_t row, uint8_t column, uint16_t number, uint8_t length)
{
  if (USER_OLED_U16ToStr(number, 16u, (uint8_t *)str_temp, length, '0'))
  {
    USER_OLED_PutString(row, column, str_temp, length);
  }
}

/**
 * @brief 在指定位置以十进制格式显示 16 位无符号整数。
 *
 * @param row    页号（0~7）。
 * @param column 字符列（0~20）。
 * @param number 待显示的 16 位无符号整数（0~65535）。
 * @param length 显示宽度（1~5 字符），左端补空格。
 */
void USER_OLED_PutUI16(uint8_t row, uint8_t column, uint16_t number, uint8_t length)
{
  if (USER_OLED_U16ToStr(number, 10u, (uint8_t *)str_temp, length, ' '))
  {
    USER_OLED_PutString(row, column, str_temp, length);
  }
}

/**
 * @brief 在指定位置以十进制格式显示 16 位有符号整数。
 *
 * @param row    页号（0~7）。
 * @param column 字符列（0~20）。
 * @param number 待显示的 16 位有符号整数（-32768~32767）。
 * @param length 显示宽度（2~6 字符，含符号位），左端补空格。
 *
 * @note 负数时符号位占 1 字符，若 length < 2 则忽略显示。
 *       INT16_MIN 特殊处理绝对值以避免溢出。
 */
void USER_OLED_PutI16(uint8_t row, uint8_t column, int16_t number, uint8_t length)
{
  if ((length == 0u) || (length > 6u))
  {
    return;
  }

  uint8_t start = 0u;
  uint16_t abs_value;
  if (number < 0)
  {
    if (length < 2u)
    {
      return;
    }
    str_temp[0] = '-';
    start = 1u;
    abs_value = (number == INT16_MIN) ? 32768u : (uint16_t)(-number);
  }
  else
  {
    abs_value = (uint16_t)number;
  }

  if (USER_OLED_U16ToStr(abs_value, 10u, (uint8_t *)&str_temp[start], (uint8_t)(length - start), ' '))
  {
    USER_OLED_PutString(row, column, str_temp, length);
  }
}

/**
 * @brief 在指定位置以浮点数格式显示数值。
 *
 * @param row          页号（0~7）。
 * @param column       字符列（0~20）。
 * @param number       待显示的浮点数。
 * @param int_length   整数部分显示宽度（1~6 字符），左端补空格。
 * @param float_length 小数部分显示宽度（1~4 字符），右端补 '0'。
 *
 * @note 自动处理 NaN（显示 "NaN"）、±Inf（显示 "Inf" / "-Inf"）。
 *       小数部分四舍五入到指定位数，总宽度 = int_length + float_length + 1（小数点）。
 */
void USER_OLED_PutFloat(uint8_t row, uint8_t column, float number, uint8_t int_length, uint8_t float_length)
{
  static const uint16_t pow10[] = {1u, 10u, 100u, 1000u, 10000u};
  uint16_t scale;
  uint16_t int_part;
  uint16_t frac_int;
  uint8_t int_digits;
  uint8_t pos = 0u;
  bool negative = false;
  char temp[16] = {0};
  char int_str[8] = {0};
  char frac_str[8] = {0};

  if ((int_length == 0u) || (int_length > 6u) || (float_length == 0u) || (float_length > 4u))
  {
    return;
  }
  if (number != number)
  {
    USER_OLED_PutString(row, column, "NaN", 3u);
    return;
  }
  if ((number > FLT_MAX) || (number < -FLT_MAX))
  {
    USER_OLED_PutString(row, column, number > 0.0f ? "Inf" : "-Inf", number > 0.0f ? 3u : 4u);
    return;
  }

  if (number < 0.0f)
  {
    if (int_length < 2u)
    {
      return;
    }
    negative = true;
    number = -number;
    temp[pos++] = '-';
  }

  scale = pow10[float_length];
  int_part = (uint16_t)number;
  frac_int = (uint16_t)(((number - (float)int_part) * (float)scale) + 0.5f);
  if (frac_int >= scale)
  {
    int_part++;
    frac_int = (uint16_t)(frac_int - scale);
  }

  int_digits = negative ? (uint8_t)(int_length - 1u) : int_length;
  if (!USER_OLED_U16ToStr(int_part, 10u, (uint8_t *)int_str, int_digits, ' '))
  {
    return;
  }
  for (uint8_t i = 0u; i < int_digits; i++)
  {
    temp[pos++] = int_str[i];
  }
  temp[pos++] = '.';

  if (!USER_OLED_U16ToStr(frac_int, 10u, (uint8_t *)frac_str, float_length, '0'))
  {
    return;
  }
  for (uint8_t i = 0u; i < float_length; i++)
  {
    temp[pos++] = frac_str[i];
  }
  temp[pos] = '\0';
  USER_OLED_PutString(row, column, temp, (uint8_t)(int_length + float_length + 1u));
}

/*===========================================================================
 * 像素点操作（公开 API）
 *
 * 所有公开点操作均带边界检查，内部调用 Fast 版本完成实际写入。
 *===========================================================================*/

/**
 * @brief 在指定位置绘制点（公开 API，带边界检查）。
 *
 * @param x X 坐标（0~127），越界则忽略。
 * @param y Y 坐标（0~63），越界则忽略。
 *
 * @note 边界内调用 USER_OLED_SetPointFast 直接写入 GRAM。
 */
void USER_OLED_SetPoint(uint8_t x, uint8_t y)
{
  if ((x < OLED_WIDTH) && (y < OLED_HEIGHT))
  {
    USER_OLED_SetPointFast(x, y);
  }
}

/**
 * @brief 在指定位置清除点（公开 API，带边界检查）。
 *
 * @param x X 坐标（0~127），越界则忽略。
 * @param y Y 坐标（0~63），越界则忽略。
 *
 * @note 边界内调用 USER_OLED_ResetPointFast 直接清除 GRAM 对应位。
 */
void USER_OLED_ResetPoint(uint8_t x, uint8_t y)
{
  if ((x < OLED_WIDTH) && (y < OLED_HEIGHT))
  {
    USER_OLED_ResetPointFast(x, y);
  }
}

/*===========================================================================
 * 线段绘制：水平线 / 垂直线 / 斜线 / 虚线
 *
 * 均带边界检查，自动交换坐标顺序。
 * 斜线使用 Bresenham 算法。
 *===========================================================================*/

/**
 * @brief 绘制水平线（公开 API，带边界检查，自动交换坐标）。
 *
 * @param x1 起点 X 坐标（0~127），越界则忽略整条线。
 * @param x2 终点 X 坐标（0~127），越界则忽略整条线。
 * @param y  Y 坐标（0~63），越界则忽略整条线。
 *
 * @note 若 x1 > x2 自动交换，内部调用 USER_OLED_DrawHLineFast。
 */
void USER_OLED_DrawHLine(uint8_t x1, uint8_t x2, uint8_t y)
{
  if ((x1 >= OLED_WIDTH) || (x2 >= OLED_WIDTH) || (y >= OLED_HEIGHT))
  {
    return;
  }
  if (x1 > x2)
  {
    uint8_t temp = x1;
    x1 = x2;
    x2 = temp;
  }
  USER_OLED_DrawHLineFast(x1, x2, y);
}

/**
 * @brief 绘制垂直线（公开 API，带边界检查，自动交换坐标）。
 *
 * @param x  X 坐标（0~127），越界则忽略整条线。
 * @param y1 起点 Y 坐标（0~63），越界则忽略整条线。
 * @param y2 终点 Y 坐标（0~63），越界则忽略整条线。
 *
 * @note 若 y1 > y2 自动交换，内部调用 USER_OLED_DrawVLineFast。
 */
void USER_OLED_DrawVLine(uint8_t x, uint8_t y1, uint8_t y2)
{
  if ((x >= OLED_WIDTH) || (y1 >= OLED_HEIGHT) || (y2 >= OLED_HEIGHT))
  {
    return;
  }
  if (y1 > y2)
  {
    uint8_t temp = y1;
    y1 = y2;
    y2 = temp;
  }
  USER_OLED_DrawVLineFast(x, y1, y2);
}

/**
 * @brief 绘制任意斜线（公开 API，Bresenham 算法）。
 *
 * @param x1 起点 X 坐标（0~127），越界则忽略。
 * @param y1 起点 Y 坐标（0~63），越界则忽略。
 * @param x2 终点 X 坐标（0~127），越界则忽略。
 * @param y2 终点 Y 坐标（0~63），越界则忽略。
 *
 * @note 水平和垂直线自动退化到对应的专用绘制函数。
 *       单点（x1==x2 && y1==y2）直接调用 SetPointFast。
 */
void USER_OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
  if ((x1 >= OLED_WIDTH) || (x2 >= OLED_WIDTH) || (y1 >= OLED_HEIGHT) || (y2 >= OLED_HEIGHT))
  {
    return;
  }
  if ((x1 == x2) && (y1 == y2))
  {
    USER_OLED_SetPointFast(x1, y1);
    return;
  }
  if (y1 == y2)
  {
    if (x1 > x2)
    {
      uint8_t temp = x1;
      x1 = x2;
      x2 = temp;
    }
    USER_OLED_DrawHLineFast(x1, x2, y1);
    return;
  }
  if (x1 == x2)
  {
    if (y1 > y2)
    {
      uint8_t temp = y1;
      y1 = y2;
      y2 = temp;
    }
    USER_OLED_DrawVLineFast(x1, y1, y2);
    return;
  }

  int16_t dx = (x2 >= x1) ? (int16_t)(x2 - x1) : (int16_t)(x1 - x2);
  int16_t dy = (y2 >= y1) ? (int16_t)(y2 - y1) : (int16_t)(y1 - y2);
  int16_t sx = (x1 < x2) ? 1 : -1;
  int16_t sy = (y1 < y2) ? 1 : -1;
  int16_t err = dx - dy;
  int16_t x = x1;
  int16_t y = y1;

  while (1)
  {
    USER_OLED_SetPointFast((uint8_t)x, (uint8_t)y);
    if ((x == x2) && (y == y2))
    {
      break;
    }
    int16_t e2 = (int16_t)(err + err);
    if (e2 > -dy)
    {
      err -= dy;
      x += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y += sy;
    }
  }
}

/**
 * @brief 绘制虚线（公开 API，Bresenham 算法 + 点线模式）。
 *
 * @param x1          起点 X 坐标（0~127），越界则忽略。
 * @param y1          起点 Y 坐标（0~63），越界则忽略。
 * @param x2          终点 X 坐标（0~127），越界则忽略。
 * @param y2          终点 Y 坐标（0~63），越界则忽略。
 * @param dash_length 每段实线像素数，为 0 则忽略。
 * @param gap_length  每段间隙像素数，为 0 则忽略。
 *
 * @note 使用段剩余计数在实线和间隙之间切换，避免逐像素取模。
 */
void USER_OLED_DrawDashedLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t dash_length, uint8_t gap_length)
{
  if ((x1 >= OLED_WIDTH) || (x2 >= OLED_WIDTH) || (y1 >= OLED_HEIGHT) || (y2 >= OLED_HEIGHT) || (dash_length == 0u) || (gap_length == 0u))
  {
    return;
  }

  int16_t dx = (x2 >= x1) ? (int16_t)(x2 - x1) : (int16_t)(x1 - x2);
  int16_t dy = (y2 >= y1) ? (int16_t)(y2 - y1) : (int16_t)(y1 - y2);
  int16_t sx = (x1 < x2) ? 1 : -1;
  int16_t sy = (y1 < y2) ? 1 : -1;
  int16_t err = dx - dy;
  int16_t x = x1;
  int16_t y = y1;
  uint8_t segment_remaining = dash_length;
  bool drawing = true;

  while (1)
  {
    if (drawing)
    {
      USER_OLED_SetPointFast((uint8_t)x, (uint8_t)y);
    }
    if ((x == x2) && (y == y2))
    {
      break;
    }
    int16_t e2 = (int16_t)(err + err);
    if (e2 > -dy)
    {
      err -= dy;
      x += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y += sy;
    }
    segment_remaining--;
    if (segment_remaining == 0u)
    {
      drawing = !drawing;
      segment_remaining = drawing ? dash_length : gap_length;
    }
  }
}

/*===========================================================================
 * 几何图形：矩形 / 圆 / 圆弧
 *
 * 矩形支持填充与非填充模式。圆使用中点画圆算法。
 * 圆弧通过 Q7 角度向量和叉积判断指定角度区间内的像素。
 *===========================================================================*/

/**
 * @brief 绘制矩形（公开 API，支持填充与边框模式）。
 *
 * @param x1   左上角 X 坐标（0~127），越界则忽略。
 * @param y1   左上角 Y 坐标（0~63），越界则忽略。
 * @param x2   右下角 X 坐标（0~127），越界则忽略。
 * @param y2   右下角 Y 坐标（0~63），越界则忽略。
 * @param fill true = 填充矩形，false = 仅绘制边框。
 *
 * @note 坐标自动排序（x1<=x2, y1<=y2）。
 *       填充模式调用 USER_OLED_FillRectFast，边框模式用 4 条线段组合。
 */
void USER_OLED_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool fill)
{
  if ((x1 >= OLED_WIDTH) || (x2 >= OLED_WIDTH) || (y1 >= OLED_HEIGHT) || (y2 >= OLED_HEIGHT))
  {
    return;
  }
  if (x1 > x2)
  {
    uint8_t temp = x1;
    x1 = x2;
    x2 = temp;
  }
  if (y1 > y2)
  {
    uint8_t temp = y1;
    y1 = y2;
    y2 = temp;
  }

  if (fill)
  {
    USER_OLED_FillRectFast(x1, y1, x2, y2);
  }
  else if (y1 == y2)
  {
    USER_OLED_DrawHLineFast(x1, x2, y1);
  }
  else if (x1 == x2)
  {
    USER_OLED_DrawVLineFast(x1, y1, y2);
  }
  else
  {
    USER_OLED_DrawHLineFast(x1, x2, y1);
    USER_OLED_DrawHLineFast(x1, x2, y2);
    USER_OLED_DrawVLineFast(x1, y1, y2);
    USER_OLED_DrawVLineFast(x2, y1, y2);
  }
}

/**
 * @brief 绘制圆形（公开 API，中点画圆算法，支持填充与边框模式）。
 *
 * @param x0     圆心 X 坐标（0~127），越界则忽略。
 * @param y0     圆心 Y 坐标（0~63），越界则忽略。
 * @param radius 半径（1~63），为 0 则忽略。
 * @param fill   true = 填充圆，false = 仅绘制圆周。
 *
 * @note 填充模式下使用水平扫描线填充（USER_OLED_DrawHLineClipped），
 *       自动处理越界裁剪，避免圆超出屏幕时产生错误。
 */
void USER_OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t radius, bool fill)
{
  bool fully_inside;

  if ((x0 >= OLED_WIDTH) || (y0 >= OLED_HEIGHT) || (radius == 0u))
  {
    return;
  }

  fully_inside = ((uint16_t)x0 >= radius) &&
                 ((uint16_t)y0 >= radius) &&
                 (((uint16_t)x0 + radius) < OLED_WIDTH) &&
                 (((uint16_t)y0 + radius) < OLED_HEIGHT);

  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;
  while (x >= y)
  {
    if (fill)
    {
      if (fully_inside)
      {
        USER_OLED_DrawHLineFast((uint8_t)((int16_t)x0 - x), (uint8_t)((int16_t)x0 + x), (uint8_t)((int16_t)y0 + y));
        USER_OLED_DrawHLineFast((uint8_t)((int16_t)x0 - x), (uint8_t)((int16_t)x0 + x), (uint8_t)((int16_t)y0 - y));
        USER_OLED_DrawHLineFast((uint8_t)((int16_t)x0 - y), (uint8_t)((int16_t)x0 + y), (uint8_t)((int16_t)y0 + x));
        USER_OLED_DrawHLineFast((uint8_t)((int16_t)x0 - y), (uint8_t)((int16_t)x0 + y), (uint8_t)((int16_t)y0 - x));
      }
      else
      {
        USER_OLED_DrawHLineClipped((int16_t)x0 - x, (int16_t)x0 + x, (int16_t)y0 + y);
        USER_OLED_DrawHLineClipped((int16_t)x0 - x, (int16_t)x0 + x, (int16_t)y0 - y);
        USER_OLED_DrawHLineClipped((int16_t)x0 - y, (int16_t)x0 + y, (int16_t)y0 + x);
        USER_OLED_DrawHLineClipped((int16_t)x0 - y, (int16_t)x0 + y, (int16_t)y0 - x);
      }
    }
    else if (fully_inside)
    {
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 + x), (uint8_t)((int16_t)y0 + y));
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 - x), (uint8_t)((int16_t)y0 + y));
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 + x), (uint8_t)((int16_t)y0 - y));
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 - x), (uint8_t)((int16_t)y0 - y));
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 + y), (uint8_t)((int16_t)y0 + x));
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 - y), (uint8_t)((int16_t)y0 + x));
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 + y), (uint8_t)((int16_t)y0 - x));
      USER_OLED_SetPointFast((uint8_t)((int16_t)x0 - y), (uint8_t)((int16_t)y0 - x));
    }
    else
    {
      USER_OLED_SetPointClipped((int16_t)x0 + x, (int16_t)y0 + y);
      USER_OLED_SetPointClipped((int16_t)x0 - x, (int16_t)y0 + y);
      USER_OLED_SetPointClipped((int16_t)x0 + x, (int16_t)y0 - y);
      USER_OLED_SetPointClipped((int16_t)x0 - x, (int16_t)y0 - y);
      USER_OLED_SetPointClipped((int16_t)x0 + y, (int16_t)y0 + x);
      USER_OLED_SetPointClipped((int16_t)x0 - y, (int16_t)y0 + x);
      USER_OLED_SetPointClipped((int16_t)x0 + y, (int16_t)y0 - x);
      USER_OLED_SetPointClipped((int16_t)x0 - y, (int16_t)y0 - x);
    }

    if (err <= 0)
    {
      y++;
      err += (int16_t)(y + y + 1);
    }
    if (err > 0)
    {
      x--;
      err -= (int16_t)(x + x + 1);
    }
  }
}

/**
 * @brief 判断角度是否在 [start, end] 区间内（支持跨 0° 边界）。
 *
 * @param angle       待判断的角度（0~359 度）。
 * @param start_angle 起始角度（0~359 度）。
 * @param end_angle   结束角度（0~359 度）。
 *
 * @return true  角度在区间内。
 * @return false 角度不在区间内。
 *
 * @note 当 start_angle > end_angle 时表示区间跨越 0° 边界
 *       （例如 start=350°, end=10°），此时采用 OR 逻辑判断。
 */
static const int8_t USER_OLED_SinQ7_0_90[91] = {
    0, 2, 4, 7, 9, 11, 13, 15, 18, 20, 22, 24, 26,
    29, 31, 33, 35, 37, 39, 41, 43, 46, 48, 50, 52, 54,
    56, 58, 60, 62, 63, 65, 67, 69, 71, 73, 75, 76, 78,
    80, 82, 83, 85, 87, 88, 90, 91, 93, 94, 96, 97, 99,
    100, 101, 103, 104, 105, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 119, 120, 121, 121, 122, 123, 123, 124,
    124, 125, 125, 125, 126, 126, 126, 127, 127, 127, 127, 127, 127};

static void USER_OLED_AngleVectorQ7(uint16_t angle, int16_t *cos_q7, int16_t *sin_q7)
{
  angle = USER_OLED_NormalizeAngle(angle);

  if (angle <= 90u)
  {
    *cos_q7 = USER_OLED_SinQ7_0_90[90u - angle];
    *sin_q7 = USER_OLED_SinQ7_0_90[angle];
  }
  else if (angle <= 180u)
  {
    *cos_q7 = (int16_t)(-USER_OLED_SinQ7_0_90[angle - 90u]);
    *sin_q7 = USER_OLED_SinQ7_0_90[180u - angle];
  }
  else if (angle <= 270u)
  {
    *cos_q7 = (int16_t)(-USER_OLED_SinQ7_0_90[270u - angle]);
    *sin_q7 = (int16_t)(-USER_OLED_SinQ7_0_90[angle - 180u]);
  }
  else
  {
    *cos_q7 = USER_OLED_SinQ7_0_90[angle - 270u];
    *sin_q7 = (int16_t)(-USER_OLED_SinQ7_0_90[360u - angle]);
  }
}

static inline int32_t USER_OLED_CrossI16(int16_t ax, int16_t ay, int16_t bx, int16_t by)
{
  return ((int32_t)ax * (int32_t)by) - ((int32_t)ay * (int32_t)bx);
}

static inline int32_t USER_OLED_DotI16(int16_t ax, int16_t ay, int16_t bx, int16_t by)
{
  return ((int32_t)ax * (int32_t)bx) + ((int32_t)ay * (int32_t)by);
}

static bool USER_OLED_VectorInAngleRange(int16_t vx,
                                         int16_t vy,
                                         int16_t start_x,
                                         int16_t start_y,
                                         int16_t end_x,
                                         int16_t end_y,
                                         uint16_t span)
{
  if (span == 0u)
  {
    return (USER_OLED_CrossI16(start_x, start_y, vx, vy) == 0) &&
           (USER_OLED_DotI16(start_x, start_y, vx, vy) >= 0);
  }

  if (span <= 180u)
  {
    return (USER_OLED_CrossI16(start_x, start_y, vx, vy) >= 0) &&
           (USER_OLED_CrossI16(vx, vy, end_x, end_y) >= 0);
  }

  return !((USER_OLED_CrossI16(end_x, end_y, vx, vy) > 0) &&
           (USER_OLED_CrossI16(vx, vy, start_x, start_y) > 0));
}

/**
 * @brief 绘制圆弧（公开 API，中点画圆 + 整数角度过滤）。
 *
 * @param x0          圆心 X 坐标（0~127），越界则忽略。
 * @param y0          圆心 Y 坐标（0~63），越界则忽略。
 * @param radius      半径（1~63），为 0 则忽略。
 * @param start_angle 起始角度（0~359 度，0° = 右，逆时针增加）。
 * @param end_angle   结束角度（0~359 度，0° = 右，逆时针增加）。
 *
 * @note 角度自动对 360 取模，支持跨 0° 边界的区间（如 350°~10°）。
 *       每像素通过整数向量叉积判断是否在 [start, end] 内。
 */
void USER_OLED_DrawArc(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t start_angle, uint16_t end_angle)
{
  int16_t start_x;
  int16_t start_y;
  int16_t end_x;
  int16_t end_y;
  uint16_t span;

  if ((x0 >= OLED_WIDTH) || (y0 >= OLED_HEIGHT) || (radius == 0u))
  {
    return;
  }

  start_angle = USER_OLED_NormalizeAngle(start_angle);
  end_angle = USER_OLED_NormalizeAngle(end_angle);
  span = (end_angle >= start_angle)
             ? (uint16_t)(end_angle - start_angle)
             : (uint16_t)(360u - start_angle + end_angle);
  USER_OLED_AngleVectorQ7(start_angle, &start_x, &start_y);
  USER_OLED_AngleVectorQ7(end_angle, &end_x, &end_y);

  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;
  while (x >= y)
  {
#define USER_OLED_TRY_ARC_POINT(dx_, dy_)                                                     \
    do                                                                                         \
    {                                                                                          \
      const int16_t dx__ = (int16_t)(dx_);                                                     \
      const int16_t dy__ = (int16_t)(dy_);                                                     \
      const int16_t draw_x__ = (int16_t)((int16_t)x0 + dx__);                                  \
      const int16_t draw_y__ = (int16_t)((int16_t)y0 + dy__);                                  \
      if ((draw_x__ >= 0) && (draw_x__ < (int16_t)OLED_WIDTH) &&                               \
          (draw_y__ >= 0) && (draw_y__ < (int16_t)OLED_HEIGHT) &&                              \
          USER_OLED_VectorInAngleRange(dx__, (int16_t)(-dy__), start_x, start_y, end_x, end_y, span)) \
      {                                                                                        \
        USER_OLED_SetPointFast((uint8_t)draw_x__, (uint8_t)draw_y__);                          \
      }                                                                                        \
    } while (0)

    USER_OLED_TRY_ARC_POINT(x, y);
    USER_OLED_TRY_ARC_POINT(-x, y);
    USER_OLED_TRY_ARC_POINT(x, -y);
    USER_OLED_TRY_ARC_POINT(-x, -y);
    USER_OLED_TRY_ARC_POINT(y, x);
    USER_OLED_TRY_ARC_POINT(-y, x);
    USER_OLED_TRY_ARC_POINT(y, -x);
    USER_OLED_TRY_ARC_POINT(-y, -x);

#undef USER_OLED_TRY_ARC_POINT

    if (err <= 0)
    {
      y++;
      err += (int16_t)(y + y + 1);
    }
    if (err > 0)
    {
      x--;
      err -= (int16_t)(x + x + 1);
    }
  }
}

/*===========================================================================
 * 辅助显示：进度条（Bar）/ 波形（Wave）
 *
 * Bar: 在指定行显示 0~100% 进度条，增量更新避免重绘。
 * Wave: 滚动波形显示，每次 UpdateWave 将波形左移一格。
 *===========================================================================*/

void USER_OLED_DrawBar(uint8_t row, uint8_t percent)
{
  uint8_t old_percent;

  if (row >= OLED_PAGE_COUNT)
  {
    return;
  }
  if (percent > 100u)
  {
    USER_OLED_PutString(row, 0u, "Err :Overflow", 13u);
    return;
  }
  if (BarRAM[row] == percent)
  {
    return;
  }

  old_percent = BarRAM[row];
  if (old_percent == 0xFFu)
  {
    USER_OLED_SetBytes(&GRAM[row][25u], 0x00, 100u);
    if (percent > 0u)
    {
      USER_OLED_SetBytes(&GRAM[row][25u], 0x7F, percent);
    }
  }
  else if (old_percent > percent)
  {
    USER_OLED_SetBytes(&GRAM[row][25u + percent], 0x00, (uint8_t)(old_percent - percent));
  }
  else
  {
    USER_OLED_SetBytes(&GRAM[row][25u + old_percent], 0x7F, (uint8_t)(percent - old_percent));
  }
  BarRAM[row] = percent;
  USER_OLED_PutUI16(row, 0u, percent, 3u);
}

void USER_OLED_UpdateWave(uint8_t value)
{
  value &= 0x3Fu;
  for (uint8_t i = 0u; i < 127u; i++)
  {
    USER_OLED_SetByteIfChanged(&GRAM[WaveRAM[i]][i], 0u);
    WaveRAM[i] = WaveRAM[i + 1u];
    USER_OLED_SetByteIfChanged(&GRAM[WaveRAM[i]][i], GRAM[WaveRAM[i]][i + 1u]);
  }
  USER_OLED_SetByteIfChanged(&GRAM[WaveRAM[126]][127], 0u);
  WaveRAM[127] = (uint8_t)(7u - (value >> 3));
  USER_OLED_SetPointFast(127u, value);
}

void USER_OLED_ClearWave(void)
{
  memset(WaveRAM, 0, sizeof(WaveRAM));
  memset(&GRAM[0][0], 0, OLED_FRAME_BUFFER_SIZE);
  memset(BarRAM, 0xFF, sizeof(BarRAM));
  USER_OLED_MARK_DIRTY();
}

/*===========================================================================
 * 显示控制：对比度 / 开关显示 / 反色
 *
 * 控制函数只记录待发送命令，由 USER_OLED_Service() 在 DMA 空闲时统一发送。
 *===========================================================================*/

void USER_OLED_SetContrast(uint8_t contrast)
{
  if (!isOLED_Initialized)
  {
    return;
  }
  oled_pending_contrast = contrast;
  oled_pending_commands |= OLED_PENDING_CONTRAST;
}

void USER_OLED_SetDisplayOn(bool on)
{
  if (!isOLED_Initialized)
  {
    return;
  }
  oled_pending_display_on = on;
  oled_pending_commands |= OLED_PENDING_DISPLAY;
}

void USER_OLED_InvertDisplay(bool invert)
{
  if (!isOLED_Initialized)
  {
    return;
  }
  oled_pending_invert = invert;
  oled_pending_commands |= OLED_PENDING_INVERT;
}

/**
 * @brief SPI0 DMA 传输完成中断服务例程。
 * @details 每次 DMA 传输完一帧后只清除 busy 状态。
 *          下一帧由 USER_OLED_Service() 按固定周期启动。
 */
void SPI0_IRQHandler(void)
{
  DL_SPI_IIDX itSource = DL_SPI_getPendingInterrupt(SPI0);
  if (itSource == DL_SPI_IIDX_DMA_DONE_TX)
  {
    DL_SPI_clearInterruptStatus(SPI0, SPI_CPU_INT_IMASK_DMA_DONE_TX_MASK);
    oled_dma_busy = 0u;
  }
}
