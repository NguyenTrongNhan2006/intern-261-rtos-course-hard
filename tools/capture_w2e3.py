"""Capture exactly one W2E3 UART run; open port, then flash or press board RESET.

Writes the full log to --out and the trace-only lines to <out>.trace.csv
(input for tools/trace_gantt.py).
"""
import argparse
import time
from pathlib import Path
import serial

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--port", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--timeout", type=float, default=60)
args = ap.parse_args()
out = Path(args.out)
trace_path = out.with_suffix(".trace.csv")
deadline = time.monotonic() + args.timeout
in_trace = False
with serial.Serial(args.port, 115200, timeout=0.5) as uart, \
        out.open("w", encoding="utf-8", newline="") as output, \
        trace_path.open("w", encoding="utf-8", newline="") as trace:
    print("Port open. Flash or press RESET on the board now.", flush=True)
    while time.monotonic() < deadline:
        raw = uart.readline()
        if not raw:
            continue
        line = raw.decode("ascii", errors="strict")
        output.write(line)
        output.flush()
        text = line.strip()
        if text.startswith("# TRACE"):
            in_trace = True
        elif text.startswith("# RECORDS"):
            in_trace = False
        elif in_trace and not text.startswith("#"):
            trace.write(line)
        if text == "# END":
            print(f"Complete W2E3 capture saved: {out} + {trace_path}")
            break
    else:
        raise SystemExit("Timeout: incomplete capture. Open port and RESET again.")
