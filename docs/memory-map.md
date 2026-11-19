# Memory map

## Galaxy board (STM32F405RG: 1 MB flash, 128 KB RAM, 64 KB CCM RAM)

### Flash

| Address | Sectors | Size | Contents |
|---|---|---|---|
| 0x0800 0000 – 0x0803 FFFF | 0–5 | 256 KB | **GalaxyBoot** (linker script `GalaxyBoot_boot.ld`) |
| 0x0804 0000 – 0x080F FFFF | 6–11 | 768 KB | **BeaconGalaxy** (`BeaconGalaxy_app.ld`, `VECT_TAB_OFFSET=0x40000`) |

- Flashing BeaconGalaxy over SWD writes only sectors 6–11, so the bootloader
  stays.
- Flashing GalaxyBoot writes only sectors 0–5, so the application stays.
- GalaxyBoot starts the application only if its first two words look valid:
  the initial stack pointer is in RAM and the reset vector is a Thumb address
  inside 0x0804 0000 – 0x080F FFFF.

### RAM

- **BeaconGalaxy:**
  - FreeRTOS heap (60 KB), the TouchGFX frame buffer, matrix buffers and
    FatFs in RAM.
  - The GIF frame-location index (4 KB) in CCM RAM, which only the CPU
    accesses.
- **GalaxyBoot:** about 10 KB, no RTOS.

## EEPROM (M24C512 on I2C2, 64 KB, address 0xA0)

| Address | Size | Contents |
|---|---|---|
| 100 – 102 | 3 | **Boot fuse.** `F0 00 0A` means "update from the SD card at the next reset". BeaconGalaxy's About › UPGRADE sets it. GalaxyBoot clears it to `00 00 00` after an update, and after an attempt that left the application untouched. Same place and value as the original firmware. Defined in `common/galaxy/boot_fuse.h`. |
| 0x0100 – | 8 + n + 2 | **Settings record:** a header (magic `"GXST"`, version 1, payload length), the `Settings` struct (`App/Settings/settings.h`) and a CRC-16 of the payload. A missing or damaged record means defaults are used. |

## Update file (`BeaconGalaxy.bin`)

Every BeaconGalaxy build writes `build/<preset>/BeaconGalaxy.bin`. It is the
raw application, followed by a 16-byte trailer written by
`tools/append_crc.py`:

| Offset | Value |
|---|---|
| +0 | magic `"GXIM"` (0x4D495847) |
| +4 | length of the application binary |
| +8 | CRC-32 (zlib) of the application binary |
| +12 | 0 |

Before erasing anything, GalaxyBoot checks the size, the trailer, the vector
table and the CRC of the whole file. It then erases only the sectors it
needs, programs everything except the first 8 bytes, and checks the CRC of
the flash. It writes the vector table last. An update cut off part-way
therefore never looks like a valid application.

## AudioGalaxy (STM32F407VG Discovery)

The whole 1 MB flash is the application, starting at 0x0800 0000. There is no
bootloader; it is flashed over the on-board ST-LINK.
