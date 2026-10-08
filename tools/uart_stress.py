#!/usr/bin/env python3
"""W3.1 host-side UART stress generator - stdlib only except pyserial, and
pyserial is imported lazily (only inside the functions that need a real
port) so `--help`, `--list-ports` without hardware, and `--simulate` all
work on a machine where pyserial is not installed.

Frame wire format (host -> MCU), little-endian, matches what the student's
own USART1 RX-DMA + IDLE-line parser must decode:
    0xAA  SEQ(u32)  LEN(u16)  PAYLOAD(LEN bytes)  CRC32(u32, zlib.crc32 of PAYLOAD)

This script only sends (and optionally also listens for an echo/ACK stream
to verify 0 lost / 0 corrupted frames); the parser, CRC check, and any
ACK protocol on the MCU side are the student's own driver code.

Usage:
    python tools/uart_stress.py --port /dev/ttyUSB0 --baud 921600 \\
        --payload-size 32 --rate-hz 100 --duration-sec 600 --log-csv run.csv

    python tools/uart_stress.py --simulate --duration-sec 2   # self-test, no hardware
    python tools/uart_stress.py --list-ports
"""
import argparse
import struct
import sys
import time
import zlib

SYNC = 0xAA
HEADER_FMT = "<BIH"  # sync, seq, len
HEADER_LEN = struct.calcsize(HEADER_FMT)
CRC_FMT = "<I"
CRC_LEN = struct.calcsize(CRC_FMT)


def build_frame(seq, payload):
    header = struct.pack(HEADER_FMT, SYNC, seq, len(payload))
    crc = struct.pack(CRC_FMT, zlib.crc32(payload) & 0xFFFFFFFF)
    return header + payload + crc


def iter_parse_frames(buf: bytearray):
    """Pull as many complete, CRC-valid frames as possible out of buf.
    Yields (seq, payload, crc_ok). Mutates buf to drop consumed bytes.
    Never raises on garbage - it just resyncs on the next 0xAA."""
    results = []
    while True:
        idx = buf.find(bytes([SYNC]))
        if idx == -1:
            buf.clear()
            break
        if idx > 0:
            del buf[:idx]
        if len(buf) < HEADER_LEN:
            break
        sync, seq, plen = struct.unpack(HEADER_FMT, bytes(buf[:HEADER_LEN]))
        total = HEADER_LEN + plen + CRC_LEN
        if len(buf) < total:
            break
        payload = bytes(buf[HEADER_LEN:HEADER_LEN + plen])
        (crc_recv,) = struct.unpack(CRC_FMT, bytes(buf[HEADER_LEN + plen:total]))
        crc_ok = (zlib.crc32(payload) & 0xFFFFFFFF) == crc_recv
        results.append((seq, payload, crc_ok))
        del buf[:total]
    return results


def payload_for(seq, size, rng_byte=0x55):
    """Deterministic, non-trivial payload so a dropped/duplicated/corrupted
    byte is easy to spot: seq repeated + an incrementing counter byte."""
    body = bytearray(size)
    for i in range(size):
        body[i] = (rng_byte + seq + i) & 0xFF
    return bytes(body)


class FakeLoopbackSerial:
    """Stand-in for a pyserial Serial object: everything written is echoed
    straight back, so --simulate exercises the full framing/CRC/stats path
    with zero hardware and zero external dependency."""

    def __init__(self):
        self._buf = bytearray()

    def write(self, data):
        self._buf += data
        return len(data)

    def read(self, size=1):
        n = min(size, len(self._buf))
        out = bytes(self._buf[:n])
        del self._buf[:n]
        return out

    @property
    def in_waiting(self):
        return len(self._buf)

    def close(self):
        pass


def open_serial(port, baud, timeout=0.1):
    import serial  # lazy: only needed for real hardware runs

    return serial.Serial(port, baudrate=baud, timeout=timeout)


def list_ports():
    import serial.tools.list_ports  # lazy

    for p in serial.tools.list_ports.comports():
        print(f"{p.device}\t{p.description}")


def run_stress(ser, duration_sec, rate_hz, payload_size, log_csv=None):
    period = 1.0 / rate_hz
    deadline = time.monotonic() + duration_sec
    seq = 0
    sent = 0
    rx_buf = bytearray()
    seen = 0
    crc_errors = 0
    lost = 0
    expected_next = None
    log_f = open(log_csv, "w", encoding="utf-8") if log_csv else None
    if log_f:
        log_f.write("t_sent,seq,crc_ok,seq_gap\n")

    next_send = time.monotonic()
    try:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_send:
                frame = build_frame(seq, payload_for(seq, payload_size))
                ser.write(frame)
                seq += 1
                sent += 1
                next_send += period

            chunk = ser.read(4096)
            if chunk:
                rx_buf += chunk
                for rseq, payload, crc_ok in iter_parse_frames(rx_buf):
                    seen += 1
                    gap = 0
                    if expected_next is not None and rseq != expected_next:
                        gap = rseq - expected_next
                        lost += max(0, gap)
                    expected_next = rseq + 1
                    if not crc_ok:
                        crc_errors += 1
                    if log_f:
                        log_f.write(f"{now:.6f},{rseq},{int(crc_ok)},{gap}\n")
            else:
                time.sleep(0.001)
    finally:
        if log_f:
            log_f.close()

    return {
        "sent": sent,
        "received": seen,
        "crc_errors": crc_errors,
        "lost_estimate": lost,
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", help="serial device, e.g. /dev/ttyUSB0 or COM5")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--payload-size", type=int, default=32)
    ap.add_argument("--rate-hz", type=float, default=50.0)
    ap.add_argument("--duration-sec", type=float, default=600.0)
    ap.add_argument("--log-csv", help="per-frame CSV log path")
    ap.add_argument("--simulate", action="store_true", help="use an in-process loopback, no hardware/pyserial needed")
    ap.add_argument("--list-ports", action="store_true", help="list serial ports (needs pyserial) and exit")
    ap.add_argument("--params-h", help="optional params.h to read W3E1_BAUD_BPS as the --baud default")
    args = ap.parse_args(argv)

    if args.list_ports:
        list_ports()
        return 0

    if args.params_h and args.baud == 921600:  # only override the untouched default
        import re

        try:
            text = open(args.params_h, encoding="utf-8").read()
            m = re.search(r"#define\s+W3E1_BAUD_BPS\s+(\d+)", text)
            if m:
                args.baud = int(m.group(1))
        except OSError:
            pass

    if args.simulate:
        ser = FakeLoopbackSerial()
    else:
        if not args.port:
            ap.error("--port is required unless --simulate or --list-ports is given")
        ser = open_serial(args.port, args.baud)

    print(f"stress: baud={args.baud} payload={args.payload_size}B rate={args.rate_hz}Hz "
          f"duration={args.duration_sec}s simulate={args.simulate}")
    stats = run_stress(ser, args.duration_sec, args.rate_hz, args.payload_size, args.log_csv)
    ser.close()

    print(f"sent={stats['sent']} received={stats['received']} "
          f"crc_errors={stats['crc_errors']} lost_estimate={stats['lost_estimate']}")
    ok = stats["crc_errors"] == 0 and stats["lost_estimate"] == 0
    print("RESULT:", "PASS (0 CRC errors, 0 lost)" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
