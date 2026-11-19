/*
 * swd_console.c - a console port in RAM, driven over SWD. See swd_console.h.
 */
#include "swd_console.h"

__attribute__((used)) SwdConsole swd_console;

void SwdConsole_Init(void)
{
  swd_console.in_size = SWDCON_IN_SIZE;
  swd_console.out_size = SWDCON_OUT_SIZE;
  swd_console.in_head = 0;
  swd_console.in_tail = 0;
  swd_console.out_head = 0;
  swd_console.magic = SWDCON_MAGIC;
}

int SwdConsole_Read(uint8_t *b)
{
  uint32_t t = swd_console.in_tail;
  if (t == swd_console.in_head) return 0;
  *b = (uint8_t)swd_console.in[t % SWDCON_IN_SIZE];
  swd_console.in_tail = t + 1u;
  return 1;
}

void SwdConsole_Write(const char *s, size_t len)
{
  uint32_t h = swd_console.out_head;
  for (size_t i = 0; i < len; i++)
  {
    swd_console.out[h % SWDCON_OUT_SIZE] = s[i];
    h++;
  }
  swd_console.out_head = h;
}
