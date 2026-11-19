/*
 * image.h - the BeaconGalaxy update file (BeaconGalaxy.bin on the SD card).
 *
 *   [ application binary, linked at 0x08040000 ][ 16-byte trailer ]
 *
 * Trailer, little-endian (written by tools/append_crc.py after each build):
 *   0  magic   "GXIM" (0x4D495847)
 *   4  length  bytes of the application binary (file size - 16)
 *   8  crc32   CRC-32 (zlib) of those bytes
 *   12 reserved, 0
 *
 * Pure functions, tested on the PC.
 */
#ifndef BOOT_IMAGE_H
#define BOOT_IMAGE_H

#include <stdint.h>
#include <stddef.h>

#define IMAGE_TRAILER_SIZE  16u
#define IMAGE_MAGIC         0x4D495847u      /* "GXIM" */

typedef struct {
  uint32_t length;
  uint32_t crc32;
} ImageInfo;

typedef enum {
  IMAGE_OK = 0,
  IMAGE_TOO_SMALL,       /* not even a vector table and a trailer */
  IMAGE_TOO_BIG,         /* does not fit the application flash */
  IMAGE_NO_TRAILER,      /* magic missing: not built for GalaxyBoot */
  IMAGE_BAD_LENGTH,      /* trailer length does not match the file */
  IMAGE_BAD_VECTORS,     /* stack pointer / reset vector not for this board */
  IMAGE_BAD_CRC,
} ImageResult;

/* Checks the trailer (last 16 bytes of a file of file_size bytes) */
ImageResult Image_CheckTrailer(const uint8_t trailer[IMAGE_TRAILER_SIZE], uint32_t file_size,
                               uint32_t max_length, ImageInfo *out);
/* Checks the first 8 bytes of the binary (initial SP, reset vector) */
ImageResult Image_CheckVectors(const uint8_t first8[8]);
const char *Image_ResultText(ImageResult r);

#endif
