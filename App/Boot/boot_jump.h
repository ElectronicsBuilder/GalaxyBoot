/*
 * boot_jump.h - start the BeaconGalaxy application at 0x08040000.
 */
#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

#include <stdint.h>

#define BOOT_APP_ADDRESS   0x08040000u
#define BOOT_APP_END       0x08100000u      /* end of the 1 MB flash */
#define BOOT_APP_MAX_SIZE  (BOOT_APP_END - BOOT_APP_ADDRESS)   /* 768 KB, sectors 6-11 */

/* pure check of an image's first two words (tested on the PC) */
int  Boot_VectorsValid(uint32_t initial_sp, uint32_t reset_vector);

int  Boot_AppValid(void);          /* vector table at BOOT_APP_ADDRESS looks valid */
void Boot_JumpToApp(void);         /* does not return; resets clocks/interrupts first */

#endif
