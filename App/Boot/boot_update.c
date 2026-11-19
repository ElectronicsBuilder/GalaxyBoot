/*
 * boot_update.c - see boot_update.h.
 */
#include "boot_update.h"
#include "boot_jump.h"
#include "boot_ui.h"
#include "image.h"
#include "crc32.h"
#include "fatfs.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define CHUNK        4096u
#define FIRST_SECTOR 6u                 /* 0x08040000 */
#define SECTOR_SIZE  (128u * 1024u)     /* sectors 6..11 */
#define VECTOR_BYTES 8u

static uint8_t chunk[CHUNK] __attribute__((aligned(4)));
static FIL file;
static unsigned last_percent = 101u;
volatile BootDiag boot_diag;

static void P_Progress(const char *what, uint32_t done, uint32_t total)
{
  unsigned p = total ? (unsigned)((uint64_t)done * 100u / total) : 100u;
  if (p == last_percent) return;
  last_percent = p;
  char line[BOOT_UI_COLS + 1];
  snprintf(line, sizeof line, "%s %3u%%", what, p);
  BootUi_Line(3, line);
  BootUi_Bar(4, p);
  BootUi_Show();
}

static void P_Status(const char *text)
{
  last_percent = 101u;
  BootUi_Line(2, text);
  BootUi_Line(3, "");
  BootUi_Line(4, "");
  BootUi_Show();
  Boot_Log("boot: %s", text);
}

/* Reads len bytes at offset; 0 = ok */
static int P_Read(uint32_t offset, void *buf, uint32_t len)
{
  UINT got;
  return (f_lseek(&file, offset) == FR_OK && f_read(&file, buf, len, &got) == FR_OK && got == len) ? 0 : -1;
}

static UpdateResult P_CheckFile(ImageInfo *info)
{
  uint8_t t[IMAGE_TRAILER_SIZE];
  uint32_t size = (uint32_t)f_size(&file);
  ImageResult r = IMAGE_TOO_SMALL;
  if (size >= IMAGE_TRAILER_SIZE && P_Read(size - IMAGE_TRAILER_SIZE, t, sizeof t) == 0)
    r = Image_CheckTrailer(t, size, BOOT_APP_MAX_SIZE, info);
  if (r == IMAGE_OK)
  {
    uint8_t v[VECTOR_BYTES];
    r = P_Read(0, v, sizeof v) == 0 ? Image_CheckVectors(v) : IMAGE_TOO_SMALL;
  }
  if (r == IMAGE_OK)
  {
    uint32_t crc = 0;
    P_Status("CHECKING");
    for (uint32_t off = 0; off < info->length && r == IMAGE_OK; off += CHUNK)
    {
      uint32_t n = info->length - off < CHUNK ? info->length - off : CHUNK;
      if (P_Read(off, chunk, n) != 0) r = IMAGE_TOO_SMALL;
      else crc = Crc32_Update(crc, chunk, n);
      P_Progress("CHECK", off + n, info->length);
    }
    if (r == IMAGE_OK && crc != info->crc32) r = IMAGE_BAD_CRC;
    Boot_Log("boot: %lu bytes, crc 0x%08lX (trailer 0x%08lX)", (unsigned long)info->length,
             (unsigned long)crc, (unsigned long)info->crc32);
  }
  if (r != IMAGE_OK)
  {
    Boot_Log("boot: file rejected: %s", Image_ResultText(r));
    BootUi_Line(5, Image_ResultText(r));
    return UPD_BAD_FILE;
  }
  return UPD_OK;
}

static int P_Erase(uint32_t length)
{
  uint32_t sectors = (length + SECTOR_SIZE - 1u) / SECTOR_SIZE;
  P_Status("ERASING");
  for (uint32_t i = 0; i < sectors; i++)
  {
    FLASH_EraseInitTypeDef e = { 0 };
    uint32_t bad = 0;
    e.TypeErase = FLASH_TYPEERASE_SECTORS;
    e.Sector = FIRST_SECTOR + i;
    e.NbSectors = 1;
    e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&e, &bad) != HAL_OK) return -1;
    P_Progress("ERASE", i + 1u, sectors);
  }
  return 0;
}

/* Programs [from, length) of the image; words beyond the file are 0xFF */
static int P_Program(uint32_t from, uint32_t length)
{
  P_Status("WRITING");
  for (uint32_t off = from; off < length; off += CHUNK)
  {
    uint32_t n = length - off < CHUNK ? length - off : CHUNK;
    memset(chunk, 0xFF, CHUNK);
    if (P_Read(off, chunk, n) != 0) return -1;
    for (uint32_t i = 0; i < n; i += 4u)
    {
      uint32_t w;
      memcpy(&w, &chunk[i], 4);
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BOOT_APP_ADDRESS + off + i, w) != HAL_OK) return -1;
    }
    P_Progress("WRITE", off + n, length);
  }
  return 0;
}

static int P_Flash(const ImageInfo *info)
{
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                         FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  boot_diag.stage = 4;
  int rc = P_Erase(info->length);
  boot_diag.stage = 5;
  if (rc == 0) rc = P_Program(VECTOR_BYTES, info->length);
  boot_diag.stage = 6;
  if (rc == 0)
  {
    /* everything but the vectors is in: check it, with the vectors taken from the file */
    uint8_t v[VECTOR_BYTES];
    uint32_t crc = 0;
    rc = P_Read(0, v, sizeof v);
    crc = Crc32_Update(crc, v, sizeof v);
    crc = Crc32_Update(crc, (const void *)(BOOT_APP_ADDRESS + VECTOR_BYTES), info->length - VECTOR_BYTES);
    if (rc == 0 && crc != info->crc32)
    {
      Boot_Log("boot: flash crc 0x%08lX, expected 0x%08lX", (unsigned long)crc, (unsigned long)info->crc32);
      rc = -1;
    }
    for (uint32_t i = 0; rc == 0 && i < VECTOR_BYTES; i += 4u)   /* the app becomes valid only now */
    {
      uint32_t w;
      memcpy(&w, &v[i], 4);
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BOOT_APP_ADDRESS + i, w) != HAL_OK) rc = -1;
    }
    if (rc == 0 && memcmp((const void *)BOOT_APP_ADDRESS, v, sizeof v) != 0) rc = -1;
  }
  HAL_FLASH_Lock();
  return rc;
}

UpdateResult Update_Run(void)
{
  ImageInfo info;
  char path[32];
  FRESULT fr;
  boot_diag.magic = 0x47414944u;
  boot_diag.stage = 1;
  P_Status("SD CARD");
  if ((fr = f_mount(&SDFatFS, SDPath, 1)) != FR_OK)
  {
    boot_diag.fresult = fr;
    boot_diag.result = UPD_NO_CARD;
    Boot_Log("boot: no SD card (FatFs %d)", (int)fr);
    return UPD_NO_CARD;
  }
  snprintf(path, sizeof path, "%s%s", SDPath, BOOT_UPDATE_FILE);
  boot_diag.stage = 2;
  if ((fr = f_open(&file, path, FA_READ)) != FR_OK)
  {
    boot_diag.fresult = fr;
    boot_diag.result = UPD_NO_FILE;
    Boot_Log("boot: %s not found", path);
    f_mount(NULL, SDPath, 0);
    return UPD_NO_FILE;
  }
  boot_diag.stage = 3;
  UpdateResult r = P_CheckFile(&info);
  if (r == UPD_OK && P_Flash(&info) != 0) r = UPD_FLASH_FAILED;
  if (r == UPD_OK) boot_diag.stage = 7;
  boot_diag.result = r;
  f_close(&file);
  f_mount(NULL, SDPath, 0);
  return r;
}

const char *Update_ResultText(UpdateResult r)
{
  switch (r)
  {
    case UPD_OK:           return "UPDATED";
    case UPD_NO_CARD:      return "NO SD CARD";
    case UPD_NO_FILE:      return "NO FILE";
    case UPD_BAD_FILE:     return "BAD FILE";
    case UPD_FLASH_FAILED: return "FLASH FAIL";
  }
  return "?";
}
