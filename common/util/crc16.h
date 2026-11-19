/*
 * crc16.h - CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), for stored records.
 */
#ifndef COMMON_CRC16_H
#define COMMON_CRC16_H

#include <stdint.h>
#include <stddef.h>

uint16_t Crc16(const void *data, size_t len);

#endif
