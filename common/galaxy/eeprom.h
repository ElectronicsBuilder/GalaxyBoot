/*
 * eeprom.h - I2C2 serial EEPROM (M24C512: 64 KB, 16-bit addresses, 128-byte
 * pages, device address 0xA0).
 *
 * Map (see docs/memory-map.md):
 *   100..102   boot fuse (read by GalaxyBoot): F0 00 0A = "update from SD"
 *   0x0100..   settings records (CRC protected, App/Settings)
 */
#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>
#include <stddef.h>

#define EE_I2C_ADDR     0xA0u
#define EE_SIZE         65536u
#define EE_PAGE         128u

int Ee_Init(void);                                      /* 0 = device answers */
int Ee_Read(uint32_t addr, void *buf, size_t len);       /* 0 = ok */
int Ee_Write(uint32_t addr, const void *buf, size_t len);/* 0 = ok (page-split, waits for each write cycle) */
int Ee_Present(void);

#endif
