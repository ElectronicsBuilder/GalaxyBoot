/*
 * uart_io.h - interrupt-driven UART byte streams for STM32F4 (RX + TX rings).
 *
 * CubeMX initialises the UART (baud rate, pins, NVIC). UartIo_Start() then
 * enables the RX interrupt; the USARTx_IRQHandler calls UartIo_Irq() in its
 * USER CODE block and returns, so the HAL handler is not used for these ports.
 *
 * The interrupt only moves bytes; tasks read with UartIo_Read() and write with
 * UartIo_Write() (waits, yielding to other tasks, while the TX ring is full).
 */
#ifndef COMMON_UART_IO_H
#define COMMON_UART_IO_H

#include <stddef.h>
#include <stdint.h>
#include "stm32f4xx.h"

#define UART_IO_RX_SIZE 512u    /* power of two */
#define UART_IO_TX_SIZE 2048u   /* power of two */

typedef struct UartIo UartIo;

/* Bind a port to a USART and enable its RX interrupt. Up to 3 ports. */
UartIo *UartIo_Start(USART_TypeDef *usart);

void   UartIo_Irq(UartIo *u);                      /* from USARTx_IRQHandler */
int    UartIo_Read(UartIo *u, uint8_t *b);          /* 1 = got a byte */
void   UartIo_Write(UartIo *u, const void *data, size_t len);
void   UartIo_Puts(UartIo *u, const char *s);
void   UartIo_Flush(UartIo *u, uint32_t timeout_ms); /* wait until sent */
uint32_t UartIo_RxOverruns(const UartIo *u);         /* bytes lost (ring full / ORE) */

#endif
