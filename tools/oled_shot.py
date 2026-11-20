#!/usr/bin/env python3
"""
oled_shot.py - screenshot of the BeaconGalaxy OLED, read over the ST-LINK.

  python tools/oled_shot.py <elf> <stlink-sn> out.png [--scale 8] [--keys "down down enter"]

Reads the TouchGFX framebuffer (symbol frameBuf, 64x48, 1 bit per pixel, MSB
is the leftmost pixel) in hot-plug mode, so the firmware keeps running, and
writes a PNG drawn like the real panel (lit pixels light, a small gap between
pixels). With --keys, each word is sent first as a console "key" command.
Needs Pillow.
"""
import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from swd_console import Probe, SwdConsole, find_symbol   # noqa: E402

W, H = 64, 48


def framebuffer(elf):
    """address of frameBuf (it sits in an unnamed namespace, so match the demangled name)"""
    out = subprocess.run(["arm-none-eabi-nm", "-C", elf], capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split(None, 2)
        if len(parts) == 3 and (parts[2] == "frameBuf" or parts[2].endswith("::frameBuf")):
            return int(parts[0], 16)
    raise SystemExit("frameBuf not found in " + elf)


def grab(probe, elf):
    data = probe.read(framebuffer(elf), W * H // 8)
    return [[bool(data[y * (W // 8) + (x >> 3)] & (0x80 >> (x & 7))) for x in range(W)] for y in range(H)]


def save(pixels, path, scale):
    from PIL import Image, ImageDraw
    border = scale * 2
    img = Image.new("RGB", (W * scale + 2 * border, H * scale + 2 * border), (8, 10, 14))
    d = ImageDraw.Draw(img)
    gap = 1 if scale >= 4 else 0
    for y in range(H):
        for x in range(W):
            if pixels[y][x]:
                x0, y0 = border + x * scale, border + y * scale
                d.rectangle([x0, y0, x0 + scale - 1 - gap, y0 + scale - 1 - gap], fill=(150, 210, 255))
    img.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("sn")
    ap.add_argument("out")
    ap.add_argument("--scale", type=int, default=8)
    ap.add_argument("--keys", default="")
    ap.add_argument("--wait", type=float, default=0.6, help="seconds to let the screen settle after keys")
    a = ap.parse_args()
    probe = Probe(a.sn)
    if a.keys:
        con = SwdConsole(probe, find_symbol(a.elf, "swd_console"))
        for k in a.keys.split():
            con.send("key %s\r" % k)
            time.sleep(0.35)
        time.sleep(a.wait)
    save(grab(probe, a.elf), a.out, a.scale)
    print(a.out)


if __name__ == "__main__":
    main()
