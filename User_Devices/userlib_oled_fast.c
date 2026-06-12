/// @brief OLED drawing acceleration wrapper
/// @note This file keeps the legacy OLED driver source unchanged, renames its
///       point/line/rectangle/circle symbols at compile time, and provides
///       faster public drawing APIs backed by direct GRAM operations.

#define USER_OLED_SetPoint   USER_OLED_SetPoint_Legacy
#define USER_OLED_ResetPoint USER_OLED_ResetPoint_Legacy
#define USER_OLED_DrawLine   USER_OLED_DrawLine_Legacy
#define USER_OLED_DrawRect   USER_OLED_DrawRect_Legacy
#define USER_OLED_DrawCircle USER_OLED_DrawCircle_Legacy

#include "userlib_oled.c"

#undef USER_OLED_SetPoint
#undef USER_OLED_ResetPoint
#undef USER_OLED_DrawLine
#undef USER_OLED_DrawRect
#undef USER_OLED_DrawCircle

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
  if ((x >= 0) && (x < OLED_WIDTH) && (y >= 0) && (y < OLED_HEIGHT))
  {
    USER_OLED_SetPointFast((uint8_t)x, (uint8_t)y);
  }
}

static inline void USER_OLED_DrawHLineFast(uint8_t x1, uint8_t x2, uint8_t y)
{
  const uint8_t row = (uint8_t)(7u - (y >> 3));
  const uint8_t mask = USER_OLED_MakePointMask(y);
  const uint16_t width = (uint16_t)x2 - (uint16_t)x1 + 1u;

  for (uint16_t i = 0u; i < width; i++)
  {
    GRAM[row][x1 + i] |= mask;
  }
}

static inline void USER_OLED_DrawHLineClipped(int16_t x1, int16_t x2, int16_t y)
{
  if ((y < 0) || (y >= OLED_HEIGHT))
  {
    return;
  }

  if (x1 > x2)
  {
    int16_t temp = x1;
    x1 = x2;
    x2 = temp;
  }

  if ((x2 < 0) || (x1 >= OLED_WIDTH))
  {
    return;
  }

  if (x1 < 0)
  {
    x1 = 0;
  }
  if (x2 >= OLED_WIDTH)
  {
    x2 = OLED_WIDTH - 1;
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

void USER_OLED_SetPoint(uint8_t x, uint8_t y)
{
  if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT))
  {
    return;
  }
  USER_OLED_SetPointFast(x, y);
}

void USER_OLED_ResetPoint(uint8_t x, uint8_t y)
{
  if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT))
  {
    return;
  }
  USER_OLED_ResetPointFast(x, y);
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
  if ((x1 >= OLED_WIDTH) || (x2 >= OLED_WIDTH) ||
      (y1 >= OLED_HEIGHT) || (y2 >= OLED_HEIGHT))
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

void USER_OLED_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool fill)
{
  if ((x1 >= OLED_WIDTH) || (x2 >= OLED_WIDTH) ||
      (y1 >= OLED_HEIGHT) || (y2 >= OLED_HEIGHT))
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
    return;
  }

  USER_OLED_DrawHLineFast(x1, x2, y1);
  if (y2 != y1)
  {
    USER_OLED_DrawHLineFast(x1, x2, y2);
  }

  if (y2 > (uint8_t)(y1 + 1u))
  {
    USER_OLED_DrawVLineFast(x1, (uint8_t)(y1 + 1u), (uint8_t)(y2 - 1u));
    if (x2 != x1)
    {
      USER_OLED_DrawVLineFast(x2, (uint8_t)(y1 + 1u), (uint8_t)(y2 - 1u));
    }
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
      if (y != 0)
      {
        USER_OLED_DrawHLineClipped((int16_t)x0 - x, (int16_t)x0 + x, (int16_t)y0 - y);
      }

      if (x != y)
      {
        USER_OLED_DrawHLineClipped((int16_t)x0 - y, (int16_t)x0 + y, (int16_t)y0 + x);
        USER_OLED_DrawHLineClipped((int16_t)x0 - y, (int16_t)x0 + y, (int16_t)y0 - x);
      }
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
      y += 1;
      err += (int16_t)(2 * y + 1);
    }

    if (err > 0)
    {
      x -= 1;
      err -= (int16_t)(2 * x + 1);
    }
  }
}
