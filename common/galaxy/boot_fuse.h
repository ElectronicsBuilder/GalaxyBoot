/*
 * boot_fuse.h - how BeaconGalaxy asks GalaxyBoot for an SD-card update:
 * three bytes in the EEPROM. Same address and value as the original firmware.
 */
#ifndef GALAXY_BOOT_FUSE_H
#define GALAXY_BOOT_FUSE_H

#define BOOT_FUSE_ADDR   100u
#define BOOT_FUSE_LEN    3u
#define BOOT_FUSE_SET    { 0xF0, 0x00, 0x0A }      /* update from BeaconGalaxy.bin at the next reset */
#define BOOT_FUSE_CLEAR  { 0x00, 0x00, 0x00 }

#endif
