"""Capture exactly one W1E3 UART run; open port, then press board RESET."""
import argparse
import time
from pathlib import Path
import serial

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--port", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--timeout", type=float, default=60)
args = ap.parse_args()
deadline = time.monotonic() + args.timeout
with serial.Serial(args.port, 115200, timeout=0.5) as uart, \
        Path(args.out).open("w", encoding="utf-8", newline="") as output:
    print("Port open. Press RESET on the board now.", flush=True)
    while time.monotonic() < deadline:
        raw = uart.readline()
        if not raw:
            continue
        line = raw.decode("ascii", errors="strict")
        output.write(line)
        output.flush()
        if line.strip() == "# W1E3 DONE":
            print("Complete W1E3 capture saved.")
            break
    else:
        raise SystemExit("Timeout: incomplete capture. Open port and RESET again.")
