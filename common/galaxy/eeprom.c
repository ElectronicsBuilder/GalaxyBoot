/*
 * eeprom.c - I2C2 serial EEPROM driver. See eeprom.h.
 * The old firmware wrote byte after byte without waiting for the ~5 ms write
 * cycle; here every page write is followed by ACK polling.
 */
#include "eeprom.h"
#include "main.h"
#include "i2c.h"

static int present;

#if GALAXY_RTOS
#include "cmsis_os2.h"
static osMutexId_t lock;
static void P_Lock(void)   { if (lock && osKernelGetState() == osKernelRunning) osMutexAcquire(lock, osWaitForever); }
static void P_Unlock(void) { if (lock && osKernelGetState() == osKernelRunning) osMutexRelease(lock); }
static void P_Sleep(void)  { if (osKernelGetState() == osKernelRunning) osDelay(1); else HAL_Delay(1); }
#else
static void P_Lock(void)   { }
static void P_Unlock(void) { }
static void P_Sleep(void)  { HAL_Delay(1); }
#endif

int Ee_Init(void)
{
#if GALAXY_RTOS
  if (lock == NULL) lock = osMutexNew(NULL);
#endif
  present = HAL_I2C_IsDeviceReady(&hi2c2, EE_I2C_ADDR, 3, 10) == HAL_OK;
  return present ? 0 : -1;
}

int Ee_Present(void) { return present; }

static int P_WaitReady(void)
{
  for (int i = 0; i < 20; i++)            /* write cycle <= 5 ms (10 for some parts) */
  {
    if (HAL_I2C_IsDeviceReady(&hi2c2, EE_I2C_ADDR, 1, 2) == HAL_OK) return 0;
    P_Sleep();
  }
  return -1;
}

int Ee_Read(uint32_t addr, void *buf, size_t len)
{
  if (addr + len > EE_SIZE) return -1;
  P_Lock();
  HAL_StatusTypeDef r = HAL_I2C_Mem_Read(&hi2c2, EE_I2C_ADDR, (uint16_t)addr, I2C_MEMADD_SIZE_16BIT,
                                         (uint8_t *)buf, (uint16_t)len, 100);
  P_Unlock();
  return r == HAL_OK ? 0 : -1;
}

int Ee_Write(uint32_t addr, const void *buf, size_t len)
{
  const uint8_t *p = (const uint8_t *)buf;
  if (addr + len > EE_SIZE) return -1;
  P_Lock();
  int rc = 0;
  while (len > 0 && rc == 0)
  {
    size_t room = EE_PAGE - (addr % EE_PAGE);
    size_t n = len < room ? len : room;
    if (HAL_I2C_Mem_Write(&hi2c2, EE_I2C_ADDR, (uint16_t)addr, I2C_MEMADD_SIZE_16BIT,
                          (uint8_t *)p, (uint16_t)n, 100) != HAL_OK) rc = -1;
    else rc = P_WaitReady();
    addr += n;
    p += n;
    len -= n;
  }
  P_Unlock();
  return rc;
}
