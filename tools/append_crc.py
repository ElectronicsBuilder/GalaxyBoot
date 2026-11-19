#!/usr/bin/env python3
"""
append_crc.py - make the GalaxyBoot update file from a raw application binary.

  python tools/append_crc.py <in.bin> <out.bin>

Writes in.bin followed by a 16-byte trailer (little-endian):
  "GXIM" magic, length of in.bin, CRC-32 (zlib) of in.bin, 0.
GalaxyBoot checks all of it before erasing anything (GalaxyBoot/App/Boot/image.h).
"""
import struct
import sys
import zlib

MAGIC = 0x4D495847          # "GXIM"
APP_MAX = 768 * 1024        # sectors 6-11


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    if len(data) < 8 or len(data) > APP_MAX:
        sys.exit("append_crc: %s is %d bytes (must be 8..%d)" % (sys.argv[1], len(data), APP_MAX))
    crc = zlib.crc32(data) & 0xFFFFFFFF
    with open(sys.argv[2], "wb") as f:
        f.write(data)
        f.write(struct.pack("<IIII", MAGIC, len(data), crc, 0))
    print("append_crc: %s, %d bytes, crc32 0x%08X" % (sys.argv[2], len(data), crc))


if __name__ == "__main__":
    main()
