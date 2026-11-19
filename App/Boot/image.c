/*
 * image.c - see image.h.
 */
#include "image.h"
#include "boot_jump.h"

static uint32_t P_U32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

ImageResult Image_CheckTrailer(const uint8_t t[IMAGE_TRAILER_SIZE], uint32_t file_size,
                               uint32_t max_length, ImageInfo *out)
{
  if (file_size < IMAGE_TRAILER_SIZE + 8u) return IMAGE_TOO_SMALL;
  if (file_size - IMAGE_TRAILER_SIZE > max_length) return IMAGE_TOO_BIG;
  if (P_U32(t) != IMAGE_MAGIC) return IMAGE_NO_TRAILER;
  uint32_t length = P_U32(t + 4);
  if (length != file_size - IMAGE_TRAILER_SIZE) return IMAGE_BAD_LENGTH;
  if (out)
  {
    out->length = length;
    out->crc32 = P_U32(t + 8);
  }
  return IMAGE_OK;
}

ImageResult Image_CheckVectors(const uint8_t first8[8])
{
  return Boot_VectorsValid(P_U32(first8), P_U32(first8 + 4)) ? IMAGE_OK : IMAGE_BAD_VECTORS;
}

const char *Image_ResultText(ImageResult r)
{
  switch (r)
  {
    case IMAGE_OK:          return "OK";
    case IMAGE_TOO_SMALL:   return "TOO SMALL";
    case IMAGE_TOO_BIG:     return "TOO BIG";
    case IMAGE_NO_TRAILER:  return "NO TRAILER";
    case IMAGE_BAD_LENGTH:  return "BAD LENGTH";
    case IMAGE_BAD_VECTORS: return "NOT AN APP";
    case IMAGE_BAD_CRC:     return "BAD CRC";
  }
  return "?";
}
