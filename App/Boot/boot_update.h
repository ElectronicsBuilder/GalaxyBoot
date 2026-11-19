/*
 * boot_update.h - copy BeaconGalaxy.bin from the SD card into the application
 * flash (sectors 6-11 at 0x08040000).
 *
 * Order of work, so that nothing is erased for a file that cannot be used and a
 * cut-off update never looks like a valid application:
 *   1. open the file, check the trailer, the vector table and the CRC of the whole file
 *   2. erase only the sectors the image needs
 *   3. program everything except the first 8 bytes (initial SP + reset vector)
 *   4. check the CRC of the flash, then program those 8 bytes last
 */
#ifndef BOOT_UPDATE_H
#define BOOT_UPDATE_H

#include <stdint.h>

#define BOOT_UPDATE_FILE "BeaconGalaxy.bin"

typedef enum {
  UPD_OK,
  UPD_NO_CARD,          /* nothing erased */
  UPD_NO_FILE,          /* nothing erased */
  UPD_BAD_FILE,         /* nothing erased */
  UPD_FLASH_FAILED,     /* erased: the application is gone until an update succeeds */
} UpdateResult;

UpdateResult Update_Run(void);

/* Last update attempt, readable over SWD (symbol boot_diag) when the console is not connected */
typedef struct {
  uint32_t magic;        /* 0x47414944 "DIAG" once written */
  uint32_t result;       /* UpdateResult */
  uint32_t stage;        /* 1 mount, 2 open, 3 check, 4 erase, 5 write, 6 verify, 7 done */
  uint32_t fresult;      /* last FatFs error */
} BootDiag;
extern volatile BootDiag boot_diag;
const char *Update_ResultText(UpdateResult r);

#endif
