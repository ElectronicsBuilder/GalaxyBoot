/*
 * crc16.c - CRC-16/CCITT-FALSE. Check value: "123456789" -> 0x29B1.
 */
#include "crc16.h"

uint16_t Crc16(const void *data, size_t len)
{
  const uint8_t *p = (const uint8_t *)data;
  uint16_t crc = 0xFFFFu;
  while (len--)
  {
    crc ^= (uint16_t)(*p++ << 8);
    for (int i = 0; i < 8; i++)
    {
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}
