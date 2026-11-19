/*
 * oled.c - SSD1306 64x48 OLED driver on SPI1 (transmit only). See oled.h.
 * Shared by BeaconGalaxy (GALAXY_RTOS=1: a mutex guards the bus) and GalaxyBoot
 * (bare metal).
 * SPI1 is set up here (not by CubeMX): master, 8 bit, mode 3, 84 MHz / 16
 * = 5.25 MHz (SSD1306 limit 10 MHz). A full frame is 384 bytes, ~0.6 ms.
 */
#include "oled.h"
#include "main.h"
#include <string.h>

#ifndef OLED_HOST_TEST
#if GALAXY_RTOS
#include "cmsis_os2.h"

static void P_Delay(uint32_t ms)
{
  if (osKernelGetState() == osKernelRunning) osDelay(ms);
  else HAL_Delay(ms);
}

/* The GUI task and the console both draw: one user of the bus at a time. */
static osMutexId_t lock;

static void P_Lock(void)
{
  if (lock) osMutexAcquire(lock, osWaitForever);
}

static void P_Unlock(void)
{
  if (lock) osMutexRelease(lock);
}
#else
static void P_Delay(uint32_t ms) { HAL_Delay(ms); }
static void P_Lock(void) { }
static void P_Unlock(void) { }
#endif

static void P_SpiInit(void)
{
  __HAL_RCC_SPI1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef g = { 0 };
  g.Pin = GPIO_PIN_5 | GPIO_PIN_7;               /* SCK, MOSI */
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  g.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &g);

  SPI1->CR1 = 0;
  SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_CPOL | SPI_CR1_CPHA |
              (3u << SPI_CR1_BR_Pos);            /* fPCLK / 16 */
  SPI1->CR1 |= SPI_CR1_SPE;
}

static void P_Send(const uint8_t *data, uint32_t len)
{
  for (uint32_t i = 0; i < len; i++)
  {
    while (!(SPI1->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&SPI1->DR = data[i];
  }
  while (!(SPI1->SR & SPI_SR_TXE)) { }
  while (SPI1->SR & SPI_SR_BSY) { }
}

static void P_Cmd(const uint8_t *cmds, uint32_t len)
{
  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_RESET);
  P_Send(cmds, len);
  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET);
}

static void P_Data(const uint8_t *d, uint32_t len)
{
  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_RESET);
  P_Send(d, len);
  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET);
}

void Oled_Init(void)
{
#if GALAXY_RTOS
  if (lock == NULL && osKernelGetState() == osKernelRunning) lock = osMutexNew(NULL);
#endif
  P_SpiInit();
  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
  P_Delay(5);
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
  P_Delay(10);
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
  P_Delay(10);

  static const uint8_t init[] = {
    0xAE,         /* display off */
    0xD5, 0x80,   /* clock divide */
    0xA8, 0x2F,   /* multiplex: 48 rows */
    0xD3, 0x00,   /* display offset */
    0x40,         /* start line 0 */
    0x8D, 0x14,   /* charge pump on */
    0x20, 0x02,   /* page addressing */
    0xA1,         /* segment remap */
    0xC8,         /* COM scan decrement */
    0xDA, 0x12,   /* COM pins */
    0x81, 0x8F,   /* contrast */
    0xD9, 0xF1,   /* pre-charge */
    0xDB, 0x40,   /* VCOMH */
    0xA4,         /* display follows RAM */
    0xA6,         /* normal (not inverted) */
  };
  P_Cmd(init, sizeof init);
  static const uint8_t blank[OLED_FB_SIZE];
  Oled_Flush(blank);
  static const uint8_t on = 0xAF;
  P_Cmd(&on, 1);
}

void Oled_Flush(const uint8_t *fb)
{
  static uint8_t pages[OLED_FB_SIZE];
  P_Lock();
  Oled_ToPages(fb, pages);
  for (uint8_t p = 0; p < OLED_HEIGHT / 8; p++)
  {
    const uint8_t addr[3] = { (uint8_t)(0xB0 | p), 0x12, 0x00 };   /* page p, column 32 */
    P_Cmd(addr, sizeof addr);
    P_Data(&pages[p * OLED_WIDTH], OLED_WIDTH);
  }
  P_Unlock();
}

void Oled_SetContrast(uint8_t c)
{
  const uint8_t cmd[2] = { 0x81, c };
  P_Lock();
  P_Cmd(cmd, 2);
  P_Unlock();
}

void Oled_Power(int on)
{
  const uint8_t cmd = on ? 0xAF : 0xAE;
  P_Lock();
  P_Cmd(&cmd, 1);
  P_Unlock();
}

void Oled_Invert(int on)
{
  const uint8_t cmd = on ? 0xA7 : 0xA6;
  P_Lock();
  P_Cmd(&cmd, 1);
  P_Unlock();
}
#endif /* OLED_HOST_TEST */

void Oled_ToPages(const uint8_t *fb, uint8_t pages[OLED_FB_SIZE])
{
  memset(pages, 0, OLED_FB_SIZE);
  for (int y = 0; y < OLED_HEIGHT; y++)
  {
    const uint8_t *row = &fb[y * (OLED_WIDTH / 8)];
    uint8_t bit = (uint8_t)(1u << (y & 7));
    uint8_t *page = &pages[(y >> 3) * OLED_WIDTH];
    for (int x = 0; x < OLED_WIDTH; x++)
    {
      if (row[x >> 3] & (0x80u >> (x & 7))) page[x] |= bit;
    }
  }
}
