#!/usr/bin/env python3
"""W3.3: mo cong COM, nhan RESET board, ghi lai toan bo log "W3E3 ..." cho den khi
thay dong DONE hoac het thoi gian cho, roi tu dong chay addr2line cho tung dong PC.

Usage:
    py tools/w3e3_capture.py --port COM19 --elf build/w3e3/w3e3.elf --log w3e3_run.log
"""
import argparse
import re
import subprocess
import sys
import time

PC_RE = re.compile(r"addr2line.*-a ([0-9A-Fa-fx]+) ([0-9A-Fa-fx]+)")


def main(argv=None):
    ap = argparse.ArgumentParser(description="W3.3: capture MCU post-mortem log and run addr2line")
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--elf", default="build/w3e3/w3e3.elf")
    ap.add_argument("--addr2line", default="arm-none-eabi-addr2line")
    ap.add_argument("--log", default="w3e3_run.log")
    ap.add_argument("--idle-timeout-sec", type=float, default=20.0,
                    help="stop after this many seconds with no new line")
    args = ap.parse_args(argv)

    import serial  # lazy import so --help works without pyserial

    ser = serial.Serial(args.port, args.baud, timeout=1)
    lines = []
    print(f"Dang cho log tren {args.port} (nhan RESET board ngay bay gio)...", file=sys.stderr)
    last_data = time.monotonic()
    done = False
    try:
        while time.monotonic() - last_data < args.idle_timeout_sec:
            raw = ser.readline()
            if not raw:
                continue
            text = raw.decode("ascii", errors="replace").rstrip("\r\n")
            if not text:
                continue
            last_data = time.monotonic()
            print(text)
            lines.append(text)
            if "W3E3 DONE" in text or "W3E3 HALT" in text:
                done = True
                last_data = time.monotonic() - args.idle_timeout_sec + 2.0  # give 2s to flush
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

    with open(args.log, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"\n{'DONE' if done else 'TIMEOUT'}: {len(lines)} dong -> {args.log}", file=sys.stderr)

    print("\n== addr2line ==")
    for ln in lines:
        m = PC_RE.search(ln)
        if not m:
            continue
        pc, lr = m.group(1), m.group(2)
        try:
            out = subprocess.run([args.addr2line, "-e", args.elf, "-f", "-p", "-a", pc, lr],
                                 capture_output=True, text=True, timeout=10)
            print(f"PC={pc} LR={lr}\n{out.stdout.strip()}")
        except FileNotFoundError:
            print(f"(khong tim thay {args.addr2line}; chay tay: "
                  f"{args.addr2line} -e {args.elf} -f -p -a {pc} {lr})")

    return 0 if done else 1


if __name__ == "__main__":
    sys.exit(main())
