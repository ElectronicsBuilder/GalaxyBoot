/*
 * uart_io.c - interrupt-driven UART byte streams. See uart_io.h.
 */
#include "uart_io.h"
#include "ring.h"
#include <string.h>

#ifdef UART_IO_NO_RTOS
#define P_YIELD() ((void)0)
#else
#include "cmsis_os2.h"
#define P_YIELD() do { if (osKernelGetState() == osKernelRunning) osDelay(1); } while (0)
#endif

struct UartIo {
  USART_TypeDef *usart;
  Ring rx;
  Ring tx;
  uint8_t rx_buf[UART_IO_RX_SIZE];
  uint8_t tx_buf[UART_IO_TX_SIZE];
  volatile uint32_t rx_lost;
};

#define UART_IO_PORTS 3
static UartIo ports[UART_IO_PORTS];

UartIo *UartIo_Start(USART_TypeDef *usart)
{
  for (int i = 0; i < UART_IO_PORTS; i++)
  {
    UartIo *u = &ports[i];
    if (u->usart == usart) return u;
    if (u->usart == NULL)
    {
      Ring_Init(&u->rx, u->rx_buf, UART_IO_RX_SIZE);
      Ring_Init(&u->tx, u->tx_buf, UART_IO_TX_SIZE);
      u->usart = usart;
      (void)usart->SR;
      (void)usart->DR;                    /* clear stale RXNE / ORE */
      usart->CR1 |= USART_CR1_RXNEIE;
      return u;
    }
  }
  return NULL;
}

void UartIo_Irq(UartIo *u)
{
  if (u == NULL) return;
  USART_TypeDef *usart = u->usart;
  uint32_t sr = usart->SR;

  if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE))
  {
    uint8_t b = (uint8_t)usart->DR;      /* reading DR also clears ORE/FE/NE */
    if (sr & USART_SR_ORE) u->rx_lost++;
    if ((sr & USART_SR_RXNE) && !(sr & USART_SR_FE))
    {
      if (!Ring_Put(&u->rx, b)) u->rx_lost++;
    }
  }

  if ((usart->CR1 & USART_CR1_TXEIE) && (sr & USART_SR_TXE))
  {
    uint8_t b;
    if (Ring_Get(&u->tx, &b)) usart->DR = b;
    else usart->CR1 &= ~USART_CR1_TXEIE;
  }
}

int UartIo_Read(UartIo *u, uint8_t *b)
{
  return (u != NULL) && Ring_Get(&u->rx, b);
}

void UartIo_Write(UartIo *u, const void *data, size_t len)
{
  if (u == NULL) return;
  const uint8_t *p = (const uint8_t *)data;
  while (len > 0)
  {
    if (Ring_Put(&u->tx, *p))
    {
      p++;
      len--;
      continue;
    }
    u->usart->CR1 |= USART_CR1_TXEIE;    /* ring full: let it drain */
    P_YIELD();
  }
  u->usart->CR1 |= USART_CR1_TXEIE;
}

void UartIo_Puts(UartIo *u, const char *s)
{
  UartIo_Write(u, s, strlen(s));
}

void UartIo_Flush(UartIo *u, uint32_t timeout_ms)
{
  if (u == NULL) return;
  for (uint32_t t = 0; t < timeout_ms; t++)
  {
    if (Ring_Empty(&u->tx) && (u->usart->SR & USART_SR_TC)) return;
    P_YIELD();
  }
}

uint32_t UartIo_RxOverruns(const UartIo *u)
{
  return u ? u->rx_lost : 0u;
}
