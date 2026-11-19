/*
 * crc32.h - CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320, init/xorout
 * 0xFFFFFFFF), same as zlib / Python binascii.crc32. Used for firmware images.
 */
#ifndef COMMON_CRC32_H
#define COMMON_CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t Crc32(const void *data, size_t len);
/* incremental: start with crc = 0, feed blocks, the result is the final CRC */
uint32_t Crc32_Update(uint32_t crc, const void *data, size_t len);

#endif
