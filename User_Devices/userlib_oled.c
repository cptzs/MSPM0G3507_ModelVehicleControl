/// @brief OLED_12864 driver core
/// @note Font data is stored in userlib_fonts.c to keep this driver maintainable.

#include "userlib_oled.h"
#include "userlib_fonts.h"

/* Keep OLED usable when the build system has not added userlib_fonts.c yet.
 * Define USER_OLED_FONT_EXTERNAL and compile userlib_fonts.c separately if a
 * project prefers a strict one-C-file-per-translation-unit layout.
 */
#ifndef USER_OLED_FONT_EXTERNAL
#include "userlib_fonts.c"
#endif

#include <limits.h>
#include <math.h>
#include <string.h>

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

#define OLED_WIDTH 128u
#define OLED_HEIGHT 64u
#define OLED_PAGE_COUNT 8u
#define OLED_CHAR_WIDTH 6u
#define OLED_CHARS_PER_ROW 21u
#define OLED_DEFAULT_BRIGHTNESS 0xFFu

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

uint8_t GRAM[OLED_PAGE_COUNT][OLED_WIDTH] = {0};
uint8_t WaveRAM[OLED_WIDTH] = {0};
uint8_t BarRAM[OLED_PAGE_COUNT] = {0};

static char str_temp[22] = {0};
static uint8_t isOLED_Initialized = 0u;
static uint8_t isOLED_AtWork = 0u;

static void __User_OLED_Delay(uint16_t ms)
{
  delay_cycles((uint32_t)CLK_PER_MS * ms);
}

static void __User_OLED_HW_Init(void)
{
  NVIC_EnableIRQ(SPI_1_INST_INT_IRQN);
}

static void __User_OLED_HW_DeInit(void)
{
}

static void __User_OLED_SetTxMode_Data(void)
{
  DL_GPIO_setPins(SPI1_DC_GPIO_Port, SPI1_DC_Pin);
}

static void __User_OLED_SetTxMode_Cmd(void)
{
  DL_GPIO_clearPins(SPI1_DC_GPIO_Port, SPI1_DC_Pin);
}

static void __User_OLED_GoNormal(void)
{
  DL_GPIO_setPins(SPI1_NRST_GPIO_Port, SPI1_NRST_Pin);
}

static void __User_OLED_GoReset(void)
{
  DL_GPIO_clearPins(SPI1_NRST_GPIO_Port, SPI1_NRST_Pin);
}

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

static void __User_OLED_SPI_Transmit(uint8_t *pdat, uint16_t size, uint16_t timeout)
{
  (void)size;
  DL_SPI_transmitData8(SPI1_CH, *pdat);
  __User_OLED_Delay(timeout);
}

static void __User_OLED_Send(uint8_t data)
{
  __User_OLED_SPI_Transmit(&data, 1u, 10u);
}

static inline uint8_t USER_OLED_MakePointMask(uint8_t y)
{
  return (uint8_t)(0x80u >> (y & 0x07u));
}

static inline uint8_t USER_OLED_MakeYMask(uint8_t bit_start, uint8_t bit_end)
{
  uint8_t mask = 0u;
  for (uint8_t bit = bit_start; bit <= bit_end; bit++)
  {
    mask |= (uint8_t)(0x80u >> bit);
  }
  return mask;
}

static inline void USER_OLED_SetPointFast(uint8_t x, uint8_t y)
{
  GRAM[7u - (y >> 3)][x] |= USER_OLED_MakePointMask(y);
}

static inline void USER_OLED_ResetPointFast(uint8_t x, uint8_t y)
{
  GRAM[7u - (y >> 3)][x] &= (uint8_t)(~USER_OLED_MakePointMask(y));
}

static inline void USER_OLED_SetPointClipped(int16_t x, int16_t y)
{
  if ((x >= 0) && (x < (int16_t)OLED_WIDTH) && (y >= 0) && (y < (int16_t)OLED_HEIGHT))
  {
    USER_OLED_SetPointFast((uint8_t)x, (uint8_t)y);
  }
}

static inline void USER_OLED_DrawHLineFast(uint8_t x1, uint8_t x2, uint8_t y)
{
  const uint8_t row = (uint8_t)(7u - (y >> 3));
  const uint8_t mask = USER_OLED_MakePointMask(y);
  for (uint8_t x = x1; x <= x2; x++)
  {
    GRAM[row][x] |= mask;
    if (x == x2)
    {
      break;
    }
  }
}

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

static inline void USER_OLED_DrawVLineFast(uint8_t x, uint8_t y1, uint8_t y2)
{
  const uint8_t page_start = (uint8_t)(y1 >> 3);
  const uint8_t page_end = (uint8_t)(y2 >> 3);

  for (uint8_t page = page_start; page <= page_end; page++)
  {
    const uint8_t bit_start = (page == page_start) ? (uint8_t)(y1 & 0x07u) : 0u;
    const uint8_t bit_end = (page == page_end) ? (uint8_t)(y2 & 0x07u) : 7u;
    const uint8_t row = (uint8_t)(7u - page);
    GRAM[row][x] |= USER_OLED_MakeYMask(bit_start, bit_end);
  }
}

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
      memset(&GRAM[row][x1], 0xFF, width);
    }
    else
    {
      for (uint16_t i = 0u; i < width; i++)
      {
        GRAM[row][x1 + i] |= mask;
      }
    }
  }
}

OLED_Status_t USER_OLED_Init(void)
{
  if (isOLED_Initialized)
  {
    return OLED_ERROR_INIT;
  }

  __User_OLED_HW_Init();
  __User_OLED_GoNormal();
  __User_OLED_Delay(10u);
  __User_OLED_GoReset();
  __User_OLED_Delay(10u);
  __User_OLED_GoNormal();
  __User_OLED_Delay(200u);

  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(OLED_cmd_SetChgPump);
  __User_OLED_Send(OLED_cmd_ChgPump_OFF);
  __User_OLED_Send(OLED_cmd_DisplayOFF);
  __User_OLED_Send(0x40u);
  __User_OLED_Send(0x00u);
  __User_OLED_Send(OLED_cmd_SetContrast);
  __User_OLED_Send(OLED_DEFAULT_BRIGHTNESS);

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

  __User_OLED_Send(OLED_cmd_NormalDisplay);
  __User_OLED_Send(OLED_cmd_SetComplexRatio);
  __User_OLED_Send(OLED_cmd_ComplexRatio_Normal);
  __User_OLED_Send(OLED_cmd_SetVirticalShift);
  __User_OLED_Send(OLED_cmd_VirticalShift_None);
  __User_OLED_Send(OLED_cmd_SetMainClock);
  __User_OLED_Send(0xF0u);
  __User_OLED_Send(OLED_cmd_SetPrechgPeriod);
  __User_OLED_Send(OLED_cmd_PrechgPeriod_Normal);
  __User_OLED_Send(OLED_cmd_SetComPinMode);
  __User_OLED_Send(OLED_cmd_ComPinMode_Normal);
  __User_OLED_Send(OLED_cmd_SetVCOMH);
  __User_OLED_Send(OLED_cmd_VCOMH_Normal);
  __User_OLED_Send(OLED_cmd_SetRamAddrMode);
  __User_OLED_Send(OLED_cmd_RamAddrMode_Row);
  __User_OLED_Send(OLED_cmd_ScrSyncGram);
  __User_OLED_Send(OLED_cmd_SetChgPump);
  __User_OLED_Send(OLED_cmd_ChgPump_7V5);
  __User_OLED_Delay(10u);

  isOLED_Initialized = 1u;
  USER_OLED_CleanScreen();
  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(OLED_cmd_DisplayON);
  __User_OLED_Delay(10u);
  __User_OLED_SetTxMode_Data();
  __User_OLED_SPI_Transmit_DMA(&GRAM[0][0], 1024u);
  isOLED_AtWork = 1u;
  return OLED_OK;
}

OLED_Status_t USER_OLED_DeInit(void)
{
  if (!isOLED_Initialized)
  {
    return OLED_ERROR_INIT;
  }
  __User_OLED_HW_DeInit();
  isOLED_AtWork = 0u;
  isOLED_Initialized = 0u;
  return OLED_OK;
}

void USER_OLED_CleanScreen(void)
{
  memset(&GRAM[0][0], 0x00, sizeof(GRAM));
  memset(WaveRAM, 0x00, sizeof(WaveRAM));
}

void USER_OLED_CleanRow(uint8_t row)
{
  if (row < OLED_PAGE_COUNT)
  {
    memset(&GRAM[row][0], 0x00, OLED_WIDTH);
  }
}

void USER_OLED_putString(uint8_t row, uint8_t column, const char *str, uint8_t length)
{
  if ((row >= OLED_PAGE_COUNT) || (column >= OLED_CHARS_PER_ROW) || (str == NULL) || (length == 0u))
  {
    return;
  }

  uint8_t x = (uint8_t)(column * OLED_CHAR_WIDTH);
  const uint8_t x_end = (uint8_t)((x + (uint16_t)length * OLED_CHAR_WIDTH > OLED_WIDTH) ? OLED_WIDTH : x + length * OLED_CHAR_WIDTH);

  while ((x < x_end) && (*str != '\0'))
  {
    uint8_t index = (uint8_t)(*str++);
    if (index >= USER_FONT_ASCII_6X8_COUNT)
    {
      index = 32u;
    }
    memcpy(&GRAM[row][x], FontLib[index], OLED_CHAR_WIDTH);
    x = (uint8_t)(x + OLED_CHAR_WIDTH);
  }
}

void USER_OLED_putChar(uint8_t row, uint8_t column, char ch)
{
  if ((row >= OLED_PAGE_COUNT) || (column >= OLED_CHARS_PER_ROW))
  {
    return;
  }
  uint8_t index = (uint8_t)ch;
  if (index >= USER_FONT_ASCII_6X8_COUNT)
  {
    index = 32u;
  }
  memcpy(&GRAM[row][column * OLED_CHAR_WIDTH], FontLib[index], OLED_CHAR_WIDTH);
}

static bool USER_OLED_U16ToStr(uint16_t num, uint8_t radix, uint8_t *str, uint8_t len, char fill)
{
  if ((str == NULL) || (len == 0u) || (len > 17u) || ((radix != 2u) && (radix != 10u) && (radix != 16u)))
  {
    return false;
  }

  static const char hex_chars[] = "0123456789ABCDEF";
  uint8_t rev[17];
  uint8_t count = 0u;

  do
  {
    if (radix == 10u)
    {
      rev[count++] = (uint8_t)('0' + (num % 10u));
      num = (uint16_t)(num / 10u);
    }
    else
    {
      uint8_t shift = (radix == 16u) ? 4u : 1u;
      rev[count++] = (uint8_t)hex_chars[num & (radix - 1u)];
      num = (uint16_t)(num >> shift);
    }
  } while ((num != 0u) && (count < sizeof(rev)));

  if (count > len)
  {
    return false;
  }

  uint8_t pos = 0u;
  while (pos < (uint8_t)(len - count))
  {
    str[pos++] = (uint8_t)fill;
  }
  for (uint8_t i = 0u; i < count; i++)
  {
    str[pos++] = rev[count - 1u - i];
  }
  str[len] = '\0';
  return true;
}

void USER_OLED_putX16(uint8_t row, uint8_t column, uint16_t number, uint8_t length)
{
  if (USER_OLED_U16ToStr(number, 16u, (uint8_t *)str_temp, length, '0'))
  {
    USER_OLED_putString(row, column, str_temp, length);
  }
}

void USER_OLED_putUI16(uint8_t row, uint8_t column, uint16_t number, uint8_t length)
{
  if (USER_OLED_U16ToStr(number, 10u, (uint8_t *)str_temp, length, ' '))
  {
    USER_OLED_putString(row, column, str_temp, length);
  }
}

void USER_OLED_putI16(uint8_t row, uint8_t column, int16_t number, uint8_t length)
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
    USER_OLED_putString(row, column, str_temp, length);
  }
}

void USER_OLED_putFloat(uint8_t row, uint8_t column, float number, uint8_t int_length, uint8_t float_length)
{
  if ((int_length == 0u) || (int_length > 6u) || (float_length == 0u) || (float_length > 4u))
  {
    return;
  }
  if (isnan(number))
  {
    USER_OLED_putString(row, column, "NaN", 3u);
    return;
  }
  if (isinf(number))
  {
    USER_OLED_putString(row, column, number > 0.0f ? "Inf" : "-Inf", number > 0.0f ? 3u : 4u);
    return;
  }

  char temp[16] = {0};
  uint8_t pos = 0u;
  bool negative = false;
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

  static const uint16_t pow10[] = {1u, 10u, 100u, 1000u, 10000u};
  uint16_t int_part = (uint16_t)number;
  float frac = number - (float)int_part;
  frac += 0.5f / (float)pow10[float_length];
  if (frac >= 1.0f)
  {
    int_part++;
    frac -= 1.0f;
  }

  uint8_t int_digits = negative ? (uint8_t)(int_length - 1u) : int_length;
  char int_str[8] = {0};
  char frac_str[8] = {0};
  if (!USER_OLED_U16ToStr(int_part, 10u, (uint8_t *)int_str, int_digits, ' '))
  {
    return;
  }
  for (uint8_t i = 0u; i < int_digits; i++)
  {
    temp[pos++] = int_str[i];
  }
  temp[pos++] = '.';

  uint16_t frac_int = (uint16_t)(frac * (float)pow10[float_length]);
  if (frac_int >= pow10[float_length])
  {
    frac_int = (uint16_t)(pow10[float_length] - 1u);
  }
  if (!USER_OLED_U16ToStr(frac_int, 10u, (uint8_t *)frac_str, float_length, '0'))
  {
    return;
  }
  for (uint8_t i = 0u; i < float_length; i++)
  {
    temp[pos++] = frac_str[i];
  }
  temp[pos] = '\0';
  USER_OLED_putString(row, column, temp, (uint8_t)(int_length + float_length + 1u));
}

void USER_OLED_SetPoint(uint8_t x, uint8_t y)
{
  if ((x < OLED_WIDTH) && (y < OLED_HEIGHT))
  {
    USER_OLED_SetPointFast(x, y);
  }
}

void USER_OLED_ResetPoint(uint8_t x, uint8_t y)
{
  if ((x < OLED_WIDTH) && (y < OLED_HEIGHT))
  {
    USER_OLED_ResetPointFast(x, y);
  }
}

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
    USER_OLED_DrawHLine(x1, x2, y1);
    return;
  }
  if (x1 == x2)
  {
    USER_OLED_DrawVLine(x1, y1, y2);
    return;
  }

  int16_t dx = abs((int16_t)x2 - (int16_t)x1);
  int16_t dy = abs((int16_t)y2 - (int16_t)y1);
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
    int16_t e2 = (int16_t)(2 * err);
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

void USER_OLED_DrawDashedLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t dash_length, uint8_t gap_length)
{
  if ((x1 >= OLED_WIDTH) || (x2 >= OLED_WIDTH) || (y1 >= OLED_HEIGHT) || (y2 >= OLED_HEIGHT) || (dash_length == 0u) || (gap_length == 0u))
  {
    return;
  }

  int16_t dx = abs((int16_t)x2 - (int16_t)x1);
  int16_t dy = abs((int16_t)y2 - (int16_t)y1);
  int16_t sx = (x1 < x2) ? 1 : -1;
  int16_t sy = (y1 < y2) ? 1 : -1;
  int16_t err = dx - dy;
  int16_t x = x1;
  int16_t y = y1;
  uint8_t count = 0u;
  uint8_t pattern = (uint8_t)(dash_length + gap_length);

  while (1)
  {
    if ((count % pattern) < dash_length)
    {
      USER_OLED_SetPointFast((uint8_t)x, (uint8_t)y);
    }
    if ((x == x2) && (y == y2))
    {
      break;
    }
    int16_t e2 = (int16_t)(2 * err);
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
    count++;
  }
}

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
  else
  {
    USER_OLED_DrawHLineFast(x1, x2, y1);
    USER_OLED_DrawHLineFast(x1, x2, y2);
    USER_OLED_DrawVLineFast(x1, y1, y2);
    USER_OLED_DrawVLineFast(x2, y1, y2);
  }
}

void USER_OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t radius, bool fill)
{
  if ((x0 >= OLED_WIDTH) || (y0 >= OLED_HEIGHT) || (radius == 0u))
  {
    return;
  }

  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;
  while (x >= y)
  {
    if (fill)
    {
      USER_OLED_DrawHLineClipped((int16_t)x0 - x, (int16_t)x0 + x, (int16_t)y0 + y);
      USER_OLED_DrawHLineClipped((int16_t)x0 - x, (int16_t)x0 + x, (int16_t)y0 - y);
      USER_OLED_DrawHLineClipped((int16_t)x0 - y, (int16_t)x0 + y, (int16_t)y0 + x);
      USER_OLED_DrawHLineClipped((int16_t)x0 - y, (int16_t)x0 + y, (int16_t)y0 - x);
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
      err += (int16_t)(2 * y + 1);
    }
    if (err > 0)
    {
      x--;
      err -= (int16_t)(2 * x + 1);
    }
  }
}

static bool USER_OLED_IsAngleInRange(uint16_t angle, uint16_t start_angle, uint16_t end_angle)
{
  if (start_angle <= end_angle)
  {
    return (angle >= start_angle) && (angle <= end_angle);
  }
  return (angle >= start_angle) || (angle <= end_angle);
}

void USER_OLED_DrawArc(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t start_angle, uint16_t end_angle)
{
  if ((x0 >= OLED_WIDTH) || (y0 >= OLED_HEIGHT) || (radius == 0u))
  {
    return;
  }
  start_angle %= 360u;
  end_angle %= 360u;

  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;
  while (x >= y)
  {
    const int16_t px[8] = {x, -x, x, -x, y, -y, y, -y};
    const int16_t py[8] = {y, y, -y, -y, x, x, -x, -x};

    for (uint8_t i = 0u; i < 8u; i++)
    {
      int16_t draw_x = (int16_t)x0 + px[i];
      int16_t draw_y = (int16_t)y0 + py[i];
      if ((draw_x >= 0) && (draw_x < (int16_t)OLED_WIDTH) && (draw_y >= 0) && (draw_y < (int16_t)OLED_HEIGHT))
      {
        double angle_d = atan2(-(double)py[i], (double)px[i]) * 180.0 / 3.14159265359;
        if (angle_d < 0.0)
        {
          angle_d += 360.0;
        }
        uint16_t angle = (uint16_t)angle_d % 360u;
        if (USER_OLED_IsAngleInRange(angle, start_angle, end_angle))
        {
          USER_OLED_SetPointFast((uint8_t)draw_x, (uint8_t)draw_y);
        }
      }
    }

    if (err <= 0)
    {
      y++;
      err += (int16_t)(2 * y + 1);
    }
    if (err > 0)
    {
      x--;
      err -= (int16_t)(2 * x + 1);
    }
  }
}

void USER_OLED_DrawBar(uint8_t row, uint8_t percent)
{
  if (row >= OLED_PAGE_COUNT)
  {
    return;
  }
  if (percent > 100u)
  {
    USER_OLED_putString(row, 0u, "Err :Overflow", 13u);
    return;
  }
  if (BarRAM[row] == percent)
  {
    return;
  }
  if (BarRAM[row] > percent)
  {
    memset(&GRAM[row][25u + percent], 0x00, (uint8_t)(BarRAM[row] - percent));
  }
  else
  {
    memset(&GRAM[row][25u + BarRAM[row]], 0x7F, (uint8_t)(percent - BarRAM[row]));
  }
  BarRAM[row] = percent;
  USER_OLED_putUI16(row, 0u, percent, 3u);
}

void USER_OLED_UpdateWave(uint8_t value)
{
  value &= 0x3Fu;
  for (uint8_t i = 0u; i < 127u; i++)
  {
    GRAM[WaveRAM[i]][i] = 0u;
    WaveRAM[i] = WaveRAM[i + 1u];
    GRAM[WaveRAM[i]][i] = GRAM[WaveRAM[i]][i + 1u];
  }
  GRAM[WaveRAM[126]][127] = 0u;
  WaveRAM[127] = (uint8_t)(7u - (value >> 3));
  USER_OLED_SetPointFast(127u, value);
}

void USER_OLED_ClearWave(void)
{
  memset(WaveRAM, 0, sizeof(WaveRAM));
  memset(GRAM, 0, sizeof(GRAM));
}

void USER_OLED_SetContrast(uint8_t contrast)
{
  if (!isOLED_Initialized)
  {
    return;
  }
  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(OLED_cmd_SetContrast);
  __User_OLED_Send(contrast);
  __User_OLED_SetTxMode_Data();
}

void USER_OLED_SetDisplayOn(bool on)
{
  if (!isOLED_Initialized)
  {
    return;
  }
  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(on ? OLED_cmd_DisplayON : OLED_cmd_DisplayOFF);
  __User_OLED_SetTxMode_Data();
}

void USER_OLED_InvertDisplay(bool invert)
{
  if (!isOLED_Initialized)
  {
    return;
  }
  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(invert ? OLED_cmd_ReverseDisplay : OLED_cmd_NormalDisplay);
  __User_OLED_SetTxMode_Data();
}

void SPI0_IRQHandler(void)
{
  DL_SPI_IIDX itSource = DL_SPI_getPendingInterrupt(SPI0);
  if (itSource == DL_SPI_IIDX_DMA_DONE_TX)
  {
    DL_SPI_clearInterruptStatus(SPI0, SPI_CPU_INT_IMASK_DMA_DONE_TX_MASK);
    if (isOLED_AtWork)
    {
      __User_OLED_SPI_Transmit_DMA(&GRAM[0][0], 1024u);
    }
  }
}
