/*
 * boot_ui.c - see boot_ui.h.
 */
#include "boot_ui.h"
#include "oled.h"
#include "font5x7.h"
#include "usart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static uint8_t fb[OLED_FB_SIZE];      /* rows top to bottom, MSB = left pixel */

static void P_Pixel(int x, int y, int on)
{
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
  uint8_t *b = &fb[y * (OLED_WIDTH / 8) + x / 8];
  uint8_t m = (uint8_t)(0x80u >> (x & 7));
  if (on) *b |= m; else *b &= (uint8_t)~m;
}

static void P_ClearRow(int row)
{
  for (int y = row * 8; y < row * 8 + 8; y++)
    for (int x = 0; x < OLED_WIDTH; x++) P_Pixel(x, y, 0);
}

void BootUi_Init(void)
{
  Oled_Init();
  BootUi_Clear();
}

void BootUi_Clear(void)
{
  memset(fb, 0, sizeof fb);
}

void BootUi_Line(int row, const char *text)
{
  if (row < 0 || row >= BOOT_UI_ROWS) return;
  P_ClearRow(row);
  for (int i = 0; i < BOOT_UI_COLS && text[i]; i++)
  {
    const uint8_t *g = Font5x7_Glyph(text[i]);
    for (int col = 0; col < 5; col++)
      for (int bit = 0; bit < 7; bit++)
        if (g[col] & (1u << bit)) P_Pixel(2 + i * 6 + col, row * 8 + bit, 1);
  }
}

void BootUi_Bar(int row, unsigned percent)
{
  if (row < 0 || row >= BOOT_UI_ROWS) return;
  if (percent > 100u) percent = 100u;
  P_ClearRow(row);
  int y0 = row * 8 + 1, y1 = row * 8 + 6;
  int fill = 2 + (int)(percent * 59u / 100u);
  for (int x = 1; x <= 62; x++) { P_Pixel(x, y0, 1); P_Pixel(x, y1, 1); }
  for (int y = y0; y <= y1; y++) { P_Pixel(1, y, 1); P_Pixel(62, y, 1); }
  for (int y = y0 + 2; y <= y1 - 2; y++)
    for (int x = 3; x < fill && x <= 60; x++) P_Pixel(x, y, 1);
}

void BootUi_Show(void)
{
  Oled_Flush(fb);
}

void Boot_Log(const char *fmt, ...)
{
  char line[96];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(line, sizeof line - 2, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if (n > (int)sizeof line - 3) n = (int)sizeof line - 3;
  line[n++] = '\r';
  line[n++] = '\n';
  HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)n, 100);
}
