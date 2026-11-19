/*
 * swd_console.h - a console port in RAM, driven by a debugger over SWD.
 *
 * The PC (tools/swd_console.py) finds the "swd_console" symbol in the ELF,
 * writes command bytes into in[] and then in_head, and reads out[] up to
 * out_head. The firmware polls it like a UART. Works while the firmware runs
 * (ST-LINK hot-plug), no wiring needed.
 *
 * Layout is fixed (the PC tool depends on it):
 *   +0  magic 'SWDC'   +4 in_size   +8 out_size
 *   +12 in_head (PC)   +16 in_tail (target)   +20 out_head (target, counts up)
 *   +24 in[in_size]    then out[out_size]
 */
#ifndef COMMON_SWD_CONSOLE_H
#define COMMON_SWD_CONSOLE_H

#include <stdint.h>
#include <stddef.h>

#define SWDCON_MAGIC    0x43445753u   /* "SWDC" */
#define SWDCON_IN_SIZE  256u
#define SWDCON_OUT_SIZE 4096u

typedef struct {
  uint32_t magic;
  uint32_t in_size;
  uint32_t out_size;
  volatile uint32_t in_head;
  volatile uint32_t in_tail;
  volatile uint32_t out_head;
  volatile char in[SWDCON_IN_SIZE];
  volatile char out[SWDCON_OUT_SIZE];
} SwdConsole;

void SwdConsole_Init(void);
int  SwdConsole_Read(uint8_t *b);                 /* 1 = got a byte */
void SwdConsole_Write(const char *s, size_t len); /* drops the oldest output when the PC is slow */

#endif
