"""Open UART first, flash/reset via the existing target, save a NEW V2 log."""
import argparse
import subprocess
import time
from pathlib import Path
import serial

ap = argparse.ArgumentParser()
ap.add_argument("optimization", choices=["O0", "O2"])
args = ap.parse_args()
repo = Path(__file__).resolve().parent.parent
output = repo / f"week1/ex3/measurements/trace_v2_{args.optimization}.csv"
flash_log = repo / f"week1/ex3/measurements/flash_v2_{args.optimization}.txt"
if output.exists() or flash_log.exists():
    raise SystemExit("V2 output exists; refusing to overwrite evidence")
with serial.Serial("COM19", 115200, timeout=0.25) as uart:
    uart.reset_input_buffer()
    print("UART COM19 OPEN; starting V2 flash/reset", flush=True)
    with flash_log.open("wb") as flash, output.open("wb") as capture:
        process = subprocess.Popen(["cmake", "--build", f"build/w1e3-v2-{args.optimization}", "--target", "w1e3_flash"],
                                   cwd=repo, stdout=flash, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        deadline = time.monotonic() + 90
        done = False
        while time.monotonic() < deadline:
            line = uart.readline()
            if line:
                capture.write(line)
                capture.flush()
                if line.strip() == b"# W1E3 DONE":
                    done = True
                    break
            if process.poll() is not None and process.returncode != 0:
                break
        if process.poll() is None:
            process.wait(timeout=15)
        code = process.returncode
    verified = b"Verified OK" in flash_log.read_bytes()
    print("FLASH_EXIT", code, "VERIFIED_OK", verified, "UART_DONE", done, flush=True)
    if code != 0 or not verified or not done:
        raise SystemExit("Hardware run incomplete; keep logs for diagnosis")
print("Hardware capture complete. Run V2 analyzer before accepting measurements.")
