#!/usr/bin/env python3
"""
swd_console.py - talk to the firmware console through the ST-LINK (no UART).

  python tools/swd_console.py <elf> <stlink-sn> "status" "ls"     run commands
  python tools/swd_console.py <elf> <stlink-sn> --tail 5          print new output for 5 s

The firmware has a "swd_console" RAM block (common/util/swd_console.h). This
tool finds it with arm-none-eabi-nm, writes the command into in[] and in_head,
then polls out[] until "OK" / "ERR" appears. Uses STM32_Programmer_CLI in
hot-plug mode, so the firmware keeps running.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

# STM32_Programmer_CLI: $STM32_PROGRAMMER_CLI, else the PATH, else the CubeCLT 1.16 default
CLI = (os.environ.get("STM32_PROGRAMMER_CLI")
       or shutil.which("STM32_Programmer_CLI")
       or r"C:/ST/STM32CubeCLT_1.16.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe")
MAGIC = 0x43445753


class Probe:
    def __init__(self, sn):
        self.sn = sn

    def run(self, *args):
        cmd = [CLI, "-c", "port=SWD", "sn=" + self.sn, "mode=HOTPLUG"] + list(args)
        out = subprocess.run(cmd, capture_output=True, text=True).stdout
        return out

    def read(self, addr, size):
        """read size bytes (multiple of 4)"""
        size = (size + 3) & ~3
        out = self.run("-r32", "0x%08X" % addr, str(size))
        words = []
        for line in out.splitlines():
            m = re.match(r"\s*0x([0-9A-Fa-f]{8})\s*:\s*(.*)", line)
            if m:
                words += [int(w, 16) for w in m.group(2).split()]
        data = b"".join(w.to_bytes(4, "little") for w in words)
        if len(data) < size:
            raise RuntimeError("read failed at 0x%08X:\n%s" % (addr, out))
        return data[:size]

    def write8(self, addr, data):
        if data:
            self.run("-w8", "0x%08X" % addr, *["0x%02X" % b for b in data])

    def write32(self, addr, value):
        self.run("-w32", "0x%08X" % addr, "0x%08X" % value)


def find_symbol(elf, name):
    out = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    raise SystemExit("symbol %s not found in %s" % (name, elf))


class SwdConsole:
    def __init__(self, probe, base):
        self.p = probe
        self.base = base
        hdr = self.p.read(base, 24)
        vals = [int.from_bytes(hdr[i:i + 4], "little") for i in range(0, 24, 4)]
        if vals[0] != MAGIC:
            raise SystemExit("swd_console not initialised (magic %08X) - firmware running?" % vals[0])
        self.in_size, self.out_size = vals[1], vals[2]
        self.in_head, self.out_seen = vals[3], vals[5]
        self.in_addr = base + 24
        self.out_addr = self.in_addr + self.in_size

    def out_head(self):
        return int.from_bytes(self.p.read(self.base + 20, 4), "little")

    def read_new(self):
        head = self.out_head()
        n = head - self.out_seen
        if n <= 0:
            return ""
        if n > self.out_size:
            self.out_seen = head - self.out_size
            n = self.out_size
        start = self.out_seen % self.out_size
        if start + n <= self.out_size:
            data = self.p.read(self.out_addr + (start & ~3), (start & 3) + n)[start & 3:][:n]
        else:
            first = self.out_size - start
            data = self.p.read(self.out_addr + (start & ~3), (start & 3) + first)[start & 3:][:first]
            data += self.p.read(self.out_addr, n - first)[:n - first]
        self.out_seen = head
        return data.decode("utf-8", "replace")

    def send(self, text):
        data = text.encode()
        pos = self.in_head
        # in[] is a ring; write in up to two parts
        idx = pos % self.in_size
        first = data[:self.in_size - idx]
        self.p.write8(self.in_addr + idx, first)
        if len(data) > len(first):
            self.p.write8(self.in_addr, data[len(first):])
        self.in_head = pos + len(data)
        self.p.write32(self.base + 12, self.in_head)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("sn")
    ap.add_argument("commands", nargs="*")
    ap.add_argument("--timeout", type=float, default=6.0)
    ap.add_argument("--tail", type=float, default=0.0)
    args = ap.parse_args()

    con = SwdConsole(Probe(args.sn), find_symbol(args.elf, "swd_console"))
    if args.tail:
        end = time.time() + args.tail
        while time.time() < end:
            sys.stdout.write(con.read_new().replace("\r", ""))
            sys.stdout.flush()
    for cmd in args.commands:
        con.read_new()                       # skip old output
        con.send(cmd + "\r")
        end = time.time() + args.timeout
        text = ""
        while time.time() < end:
            text += con.read_new()
            lines = [l.strip() for l in text.replace("\r", "\n").split("\n")]
            if any(l == "OK" or l.startswith("ERR") for l in lines):
                break
        sys.stdout.write(text.replace("\r\n", "\n").replace("\r", "") + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
