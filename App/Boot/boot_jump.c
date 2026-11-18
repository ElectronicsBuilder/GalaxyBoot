/*
 * boot_jump.c - start the application. See boot_jump.h.
 */
#include "boot_jump.h"

int Boot_VectorsValid(uint32_t sp, uint32_t reset)
{
  int sp_ok = (sp > 0x20000000u) && (sp <= 0x20020000u) && ((sp & 3u) == 0u);   /* 128 KB SRAM */
  int pc_ok = (reset & 1u) && (reset > BOOT_APP_ADDRESS) && (reset < BOOT_APP_END); /* Thumb, in the app */
  return sp_ok && pc_ok;
}

#ifndef BOOT_HOST_TEST
#include "stm32f4xx.h"

int Boot_AppValid(void)
{
  const uint32_t *v = (const uint32_t *)BOOT_APP_ADDRESS;
  return Boot_VectorsValid(v[0], v[1]);
}

void Boot_JumpToApp(void)
{
  const uint32_t *v = (const uint32_t *)BOOT_APP_ADDRESS;
  uint32_t sp = v[0];
  uint32_t reset = v[1];

  __disable_irq();
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  for (unsigned i = 0; i < 8u; i++)                    /* all NVIC lines off, nothing pending */
  {
    NVIC->ICER[i] = 0xFFFFFFFFu;
    NVIC->ICPR[i] = 0xFFFFFFFFu;
  }
  /* back to the reset clock (HSI) so the app starts like after power-up */
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) { }
  RCC->CFGR = 0;
  while ((RCC->CFGR & RCC_CFGR_SWS) != 0u) { }
  RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);
  /* peripherals used by the bootloader back to reset */
  RCC->AHB1RSTR = 0xFFFFFFFFu; RCC->AHB1RSTR = 0;
  RCC->APB1RSTR = 0xFFFFFFFFu; RCC->APB1RSTR = 0;
  RCC->APB2RSTR = 0xFFFFFFFFu; RCC->APB2RSTR = 0;

  SCB->VTOR = BOOT_APP_ADDRESS;
  __DSB();
  __ISB();
  __set_MSP(sp);
  __set_CONTROL(0);
  __enable_irq();
  ((void (*)(void))reset)();
  for (;;) { }
}
#endif
