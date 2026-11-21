#!/usr/bin/env python3
"""Live telemetry decoder, interactive shell, and capture tool for W4.2.

Decodes 80-byte binary 'W4E2' telemetry frames (with IEEE CRC-32 verification)
while cleanly separating ASCII replies from the W4.2 command shell on the same
USART1 TX wire.

Examples:
  # Interactive monitor + shell (temporary CP2102 baud 921600):
  py tools/w4e2_monitor.py --port COM19 --baud 921600

  # Interactive monitor at SSOT 1 Mbps (CH340 / FT232RL / CP2102N):
  py tools/w4e2_monitor.py --port COM19 --baud 1000000

  # Automated capture (30 s steady-state + shell & STOP verification + CSV):
  py tools/w4e2_monitor.py --port COM19 --baud 921600 --capture-sec 30 --csv week4/ex2/measurements/w4e2_capture.csv
"""
import argparse
import csv
import os
import queue
import struct
import sys
import threading
import time
import zlib

import serial

MAGIC = b"W4E2"
FRAME_BYTES = 80
CORE_MHZ = 72.0


def trailing_magic_prefix_len(buf: bytearray) -> int:
    for n in (3, 2, 1):
        if len(buf) >= n and buf[-n:] == MAGIC[:n]:
            return n
    return 0


class W4E2StreamDecoder:
    def __init__(self):
        self.buf = bytearray()
        self.ascii_buf = bytearray()
        self.frames_ok = 0
        self.crc_errors = 0

    def feed(self, data: bytes):
        frames = []
        lines = []
        self.buf.extend(data)

        while self.buf:
            idx = self.buf.find(MAGIC)
            if idx == -1:
                keep = trailing_magic_prefix_len(self.buf)
                flush_end = len(self.buf) - keep
                if flush_end > 0:
                    self._consume_ascii(self.buf[:flush_end], lines)
                    del self.buf[:flush_end]
                break

            if idx > 0:
                self._consume_ascii(self.buf[:idx], lines)
                del self.buf[:idx]

            if len(self.buf) < FRAME_BYTES:
                break

            pkt = bytes(self.buf[:FRAME_BYTES])
            w = struct.unpack("<20I", pkt)
            calc_crc = zlib.crc32(pkt[:76]) & 0xFFFFFFFF
            if calc_crc == w[19]:
                self.frames_ok += 1
                frames.append({
                    "tick_ms": w[1],
                    "period_ms": w[2],
                    "overrun": w[3],
                    "stale": w[4],
                    "jitter_us": w[5] / CORE_MHZ,
                    "wcrt_us": w[6] / CORE_MHZ,
                    "rx_faults": w[7],
                    "tx_errors": w[8],
                    "imu_rec": w[9],
                    "imu_pulses": w[10],
                    "stop": w[11],
                    "stop_faults": w[12],
                    "cpu_pct": w[13] & 0xFF,
                    "hb_miss": (w[13] >> 8) & 0xFF,
                    "stack_low": (w[13] >> 16) & 0xFF,
                    "hwm_aux": w[14],
                    "hwm_sh": w[15],
                    "hwm_tel": w[16],
                    "hwm_imu": w[17],
                    "hwm_ctl": w[18],
                })
                del self.buf[:FRAME_BYTES]
            else:
                self.crc_errors += 1
                del self.buf[:4]

        return frames, lines

    def _consume_ascii(self, chunk: bytes, lines: list):
        for b in chunk:
            if b == 10:  # '\n'
                text = self.ascii_buf.decode("ascii", errors="ignore").strip("\r")
                self.ascii_buf.clear()
                if text:
                    lines.append(text)
            elif b == 13 or 32 <= b < 127:
                self.ascii_buf.append(b)
                if len(self.ascii_buf) > 256:
                    self.ascii_buf.clear()
            else:
                # Non-ASCII byte indicates a partial binary tail; drop buffered noise.
                self.ascii_buf.clear()


def format_frame(f: dict, ok_cnt: int, crc_err: int) -> str:
    return (
        f"[TEL #{ok_cnt:5d} err={crc_err}] "
        f"t={f['tick_ms']:6d}ms per={f['period_ms']:4d}ms "
        f"cpu={f['cpu_pct']:2d}% jit={f['jitter_us']:5.2f}us wcrt={f['wcrt_us']:5.2f}us "
        f"over={f['overrun']} stale={f['stale']} rec={f['imu_rec']} stop={f['stop']} "
        f"hb={f['hb_miss']} low={f['stack_low']} "
        f"HWMw(aux/sh/tel/imu/ctl)={f['hwm_aux']}/{f['hwm_sh']}/{f['hwm_tel']}/{f['hwm_imu']}/{f['hwm_ctl']}"
    )


def send_shell(ser: serial.Serial, cmd: str, wake_first: bool = False):
    if wake_first:
        ser.write(b"\n")
        ser.flush()
        time.sleep(0.02)
    ser.write((cmd.strip() + "\n").encode("ascii"))
    ser.flush()


def run_interactive(ser: serial.Serial, print_interval_s: float):
    decoder = W4E2StreamDecoder()
    cmd_queue = queue.Queue()
    stop_event = threading.Event()

    def stdin_worker():
        while not stop_event.is_set():
            try:
                line = sys.stdin.readline()
            except EOFError:
                break
            if not line:
                break
            cmd_queue.put(line.strip())

    t = threading.Thread(target=stdin_worker, daemon=True)
    t.start()

    print(
        f"Connected {ser.port} @ {ser.baudrate} bps. "
        "Commands: status | stats | set telemetry <10..1000> | quiesce on | quiesce off | quit"
    )
    last_print = 0.0
    last_frame_time = time.monotonic()
    last_frame = None
    last_cmd_time = 0.0

    try:
        while not stop_event.is_set():
            raw = ser.read(ser.in_waiting or 1)
            now = time.monotonic()
            if raw:
                frames, lines = decoder.feed(raw)
                for line in lines:
                    print(f"[SHELL] {line}", flush=True)
                if frames:
                    last_frame = frames[-1]
                    last_frame_time = now
            if last_frame and (now - last_print) >= print_interval_s and (now - last_frame_time) < 1.5:
                print(format_frame(last_frame, decoder.frames_ok, decoder.crc_errors), flush=True)
                last_print = now

            try:
                cmd = cmd_queue.get_nowait()
            except queue.Empty:
                continue

            if not cmd:
                continue
            if cmd.lower() in ("q", "quit", "exit"):
                break

            # Enforce >= 50 ms gap between shell commands (W4.1 T_min = 50 ms).
            gap = time.monotonic() - last_cmd_time
            if gap < 0.08:
                time.sleep(0.08 - gap)
            quiet = (time.monotonic() - last_frame_time) > 0.25
            send_shell(ser, cmd, wake_first=quiet)
            last_cmd_time = time.monotonic()
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()


def run_capture(ser: serial.Serial, duration_s: float, csv_path: str, test_stop: bool):
    decoder = W4E2StreamDecoder()
    all_frames = []

    def pump(seconds: float):
        end = time.monotonic() + seconds
        got_lines = []
        while time.monotonic() < end:
            raw = ser.read(ser.in_waiting or 1)
            if raw:
                frames, lines = decoder.feed(raw)
                all_frames.extend(frames)
                got_lines.extend(lines)
        return got_lines

    ser.reset_input_buffer()
    print(f"Capturing steady-state telemetry on {ser.port} @ {ser.baudrate} bps for {duration_s:.1f} s...")
    pump(0.3)
    all_frames.clear()
    decoder.frames_ok = 0
    decoder.crc_errors = 0

    t0 = time.monotonic()
    next_log = t0
    while time.monotonic() - t0 < duration_s:
        raw = ser.read(ser.in_waiting or 1)
        if raw:
            frames, lines = decoder.feed(raw)
            all_frames.extend(frames)
            for l in lines:
                print(f"[SHELL] {l}")
        now = time.monotonic()
        if all_frames and now >= next_log:
            print(format_frame(all_frames[-1], decoder.frames_ok, decoder.crc_errors), flush=True)
            next_log = now + 1.0

    # Query shell status & stats + runtime parameter change test
    print("\n--- Shell & Runtime Tests ---")
    for cmd in ("status", "stats", "set telemetry 200", "status", "set telemetry 10"):
        send_shell(ser, cmd)
        lines = pump(0.18)
        print(f"CMD `{cmd}` -> {' | '.join(lines) if lines else '(no reply)'}")

    if test_stop:
        print("\n--- Tickless STOP Mode Test (quiesce on 1.5s -> wake -> quiesce off) ---")
        send_shell(ser, "quiesce on")
        lines = pump(0.15)
        print(f"CMD `quiesce on` -> {' | '.join(lines)}")
        before_q = len(all_frames)
        pump(1.5)
        during_q = len(all_frames) - before_q
        send_shell(ser, "quiesce off", wake_first=True)
        lines = pump(0.18)
        print(f"Frames during 1.5s quiesce: {during_q} | CMD `quiesce off` -> {' | '.join(lines)}")
        send_shell(ser, "status")
        lines = pump(0.18)
        print(f"CMD `status` (post-STOP) -> {' | '.join(lines)}")

    if csv_path and all_frames:
        os.makedirs(os.path.dirname(os.path.abspath(csv_path)), exist_ok=True)
        with open(csv_path, "w", newline="", encoding="utf-8") as fh:
            writer = csv.DictWriter(fh, fieldnames=list(all_frames[0].keys()))
            writer.writeheader()
            writer.writerows(all_frames)
        print(f"\nSaved {len(all_frames)} valid frames to {csv_path}")

    if all_frames:
        last = all_frames[-1]
        first = all_frames[0]
        cpus = [f["cpu_pct"] for f in all_frames]
        print("\n=== SUMMARY ===")
        print(f"Valid frames: {len(all_frames)} | CRC errors: {decoder.crc_errors}")
        print(f"Control overrun: {last['overrun']} | Stale delta during window: {last['stale'] - first['stale']} (total={last['stale']})")
        print(f"Max jitter: {last['jitter_us']:.2f} us (limit 113 us) | Max WCRT: {last['wcrt_us']:.2f} us (R_HI 97 us)")
        print(f"CPU load: min={min(cpus)}% avg={sum(cpus)/len(cpus):.2f}% max={max(cpus)}% (budget <= 76%)")
        print(f"IMU recoveries: {last['imu_rec']} | RX faults: {last['rx_faults']} | TX errors: {last['tx_errors']}")
        print(f"STOP entries: {last['stop']} | STOP faults: {last['stop_faults']} | hb_miss={last['hb_miss']} | stack_low={last['stack_low']}")
        print(
            "Stack HWM (free words / budget): "
            f"control={last['hwm_ctl']}/192, imu={last['hwm_imu']}/192, "
            f"telemetry={last['hwm_tel']}/192, shell={last['hwm_sh']}/256, idle_aux={last['hwm_aux']}/192"
        )


def main():
    ap = argparse.ArgumentParser(description="W4.2 binary telemetry monitor & shell console")
    ap.add_argument("--port", default="COM19", help="Serial port (default: COM19)")
    ap.add_argument("--baud", type=int, default=921600, help="Baud rate (default: 921600; use 1000000 on CH340/FT232)")
    ap.add_argument("--print-ms", type=int, default=500, help="Interactive telemetry print interval in ms")
    ap.add_argument("--capture-sec", type=float, default=0.0, help="Run automated capture for N seconds instead of interactive mode")
    ap.add_argument("--csv", default="", help="Optional CSV output path in --capture-sec mode")
    ap.add_argument("--no-stop-test", action="store_true", help="Skip quiesce/STOP test in --capture-sec mode")
    args = ap.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.05)
    except serial.SerialException as exc:
        print(f"ERROR opening {args.port}: {exc}", file=sys.stderr)
        print("Hint: close Serial Debug Assistant / other terminal holding the COM port first.", file=sys.stderr)
        sys.exit(1)

    try:
        if args.capture_sec > 0.0:
            run_capture(ser, args.capture_sec, args.csv, not args.no_stop_test)
        else:
            run_interactive(ser, max(0.05, args.print_ms / 1000.0))
    finally:
        ser.close()


if __name__ == "__main__":
    main()
