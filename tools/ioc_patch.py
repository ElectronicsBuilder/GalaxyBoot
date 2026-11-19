#!/usr/bin/env python3
"""
ioc_patch.py - apply a patch file to a CubeMX .ioc and rebuild the pin/IP lists.

Usage:  python tools/ioc_patch.py <in.ioc> <patch.txt> <out.ioc>

Patch file lines:
  key=value          set (or add) a key
  -prefix            delete every key starting with "prefix" (e.g. -PA5. or -I2S3.)
  ip NAME            add NAME to the Mcu.IP list
  -ip NAME           remove NAME from the Mcu.IP list
  # comment

Mcu.PinN / Mcu.PinsNb are rebuilt from every "<pin>.Signal" key; Mcu.IPN /
Mcu.IPNb from the IP list. CubeMX re-validates everything when it loads the
file, so run "config load" + "project generate" and read its log afterwards.
"""
import re
import sys


def load(path):
    keys = {}
    header = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                header.append(line)
                continue
            k, _, v = line.partition("=")
            keys[k] = v
    return header, keys


def pin_sort_key(name):
    if name.startswith("VP_"):
        return (1, name)
    m = re.match(r"P([A-K])(\d+)", name)
    if m:
        return (0, m.group(1), int(m.group(2)), name)
    return (0, name)


def main():
    src, patch, dst = sys.argv[1:4]
    header, keys = load(src)
    ips = [keys[k] for k in sorted((k for k in keys if re.fullmatch(r"Mcu\.IP\d+", k)),
                                   key=lambda k: int(k[6:]))]

    with open(patch, encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("ip "):
                name = line[3:].strip()
                if name not in ips:
                    ips.append(name)
            elif line.startswith("-ip "):
                name = line[4:].strip()
                ips = [i for i in ips if i != name]
            elif line.startswith("-"):
                prefix = line[1:]
                for k in [k for k in keys if k.startswith(prefix)]:
                    del keys[k]
            else:
                k, _, v = line.partition("=")
                keys[k.strip()] = v.strip()

    for k in [k for k in keys if re.fullmatch(r"Mcu\.(Pin\d+|PinsNb|IP\d+|IPNb)", k)]:
        del keys[k]
    pins = sorted({k[:-len(".Signal")] for k in keys if k.endswith(".Signal")}, key=pin_sort_key)
    for i, p in enumerate(pins):
        keys["Mcu.Pin%d" % i] = p
    keys["Mcu.PinsNb"] = str(len(pins))
    for i, ip in enumerate(sorted(ips)):
        keys["Mcu.IP%d" % i] = ip
    keys["Mcu.IPNb"] = str(len(ips))

    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write("#MicroXplorer Configuration settings - do not modify\n")
        for k in sorted(keys):
            f.write("%s=%s\n" % (k, keys[k]))


if __name__ == "__main__":
    main()
