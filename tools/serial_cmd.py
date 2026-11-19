#!/usr/bin/env python3
"""
serial_cmd.py - send console commands to a board and print what comes back.

  python tools/serial_cmd.py COM10 "ver" "status"      run commands, print replies
  python tools/serial_cmd.py COM11 --listen 5          just print for 5 s
  python tools/serial_cmd.py COM10 --reset-listen 4    toggle nothing, read 4 s

Each command waits until a line "OK" or "ERR ..." arrives (or --timeout).
Needs pyserial (pip install pyserial).
"""
import argparse
import sys
import time

import serial


def read_until_done(port, timeout):
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        chunk = port.read(4096)
        if chunk:
            buf += chunk
            text = buf.decode("utf-8", "replace")
            lines = [l.strip() for l in text.replace("\r", "\n").split("\n")]
            if any(l == "OK" or l.startswith("ERR") for l in lines):
                time.sleep(0.05)
                buf += port.read(4096)
                break
    return buf.decode("utf-8", "replace")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("commands", nargs="*")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=3.0)
    ap.add_argument("--listen", type=float, default=0.0, help="seconds to listen first")
    args = ap.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.05) as port:
        if args.listen:
            end = time.time() + args.listen
            while time.time() < end:
                data = port.read(4096)
                if data:
                    sys.stdout.write(data.decode("utf-8", "replace"))
                    sys.stdout.flush()
        for cmd in args.commands:
            port.reset_input_buffer()
            port.write((cmd + "\r").encode())
            sys.stdout.write(read_until_done(port, args.timeout).replace("\r\n", "\n").replace("\r", ""))
            sys.stdout.write("\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
