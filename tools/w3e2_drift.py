#!/usr/bin/env python3
"""W3.2: so tick_ms của MCU với đồng hồ PC để đo độ trôi trong cửa sổ 10 phút.

Firmware in "W3E2 event=START|SAMPLE|DONE tick_ms=... " qua USART1 (115200 8N1).
Script đóng dấu giờ PC (time.perf_counter) lúc nhận xong mỗi dòng, lấy dòng đầu tiên
làm mốc 0, rồi tính: chênh lệch = (giờ MCU - giờ PC) tính bằng ms. Độ trễ truyền
dòng log là hằng số nên bị triệt tiêu khi lấy hiệu giữa hai dòng.

Usage:
    py tools/w3e2_drift.py --port COM19 --bound-ppm 390 --csv w3e2_drift.csv
Mở cổng rồi nhấn RESET board để bắt dòng START làm mốc.
"""
import argparse
import re
import sys
import time

LINE_RE = re.compile(r"W3E2 event=(\w+) tick_ms=(\d+)")
FIELD_RE = re.compile(r"(sleep_entries|rtc_faults|late_wakes|recals|cal_freq_millihz)=(\d+)")


def parse_line(text):
    """Trả (event, tick_ms, fields) hoặc None nếu không phải dòng W3E2."""
    m = LINE_RE.search(text)
    if not m:
        return None
    fields = {k: int(v) for k, v in FIELD_RE.findall(text)}
    return m.group(1), int(m.group(2)), fields


def drift_row(ref, mcu_ms, pc_s):
    """ref = (mcu_ms0, pc_s0). Trả (giờ MCU s, giờ PC s, chênh ms, ppm)."""
    mcu_s = (mcu_ms - ref[0]) / 1000.0
    pc_rel = pc_s - ref[1]
    diff_ms = (mcu_s - pc_rel) * 1000.0
    ppm = diff_ms / (pc_rel * 1000.0) * 1e6 if pc_rel > 0 else 0.0
    return mcu_s, pc_rel, diff_ms, ppm


def main(argv=None):
    ap = argparse.ArgumentParser(description="W3.2: compare MCU tick_ms against the PC clock")
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--bound-ppm", type=float, default=390.0)
    ap.add_argument("--csv", help="write the drift table to a CSV file")
    args = ap.parse_args(argv)

    import serial  # nhập muộn để --help chạy được khi chưa cài pyserial

    ser = serial.Serial(args.port, args.baud, timeout=1)
    out = open(args.csv, "w", encoding="utf-8") if args.csv else None
    header = ("event,mcu_s,pc_s,diff_ms,ppm,sleep_entries,rtc_faults,late_wakes,"
              "recals,cal_freq_millihz")
    print(header)
    if out:
        out.write(header + "\n")

    ref = None
    last = None
    print("Dang cho dong W3E2 ... (nhan RESET board de bat START)", file=sys.stderr)
    try:
        while True:
            raw = ser.readline()
            now = time.perf_counter()
            parsed = parse_line(raw.decode("ascii", errors="replace"))
            if parsed is None:
                continue
            event, tick_ms, fields = parsed
            if ref is None:
                ref = (tick_ms, now)
            mcu_s, pc_s, diff_ms, ppm = drift_row(ref, tick_ms, now)
            row = (f"{event},{mcu_s:.3f},{pc_s:.3f},{diff_ms:+.1f},{ppm:+.1f},"
                   f"{fields.get('sleep_entries', 0)},{fields.get('rtc_faults', 0)},"
                   f"{fields.get('late_wakes', 0)},{fields.get('recals', 0)},"
                   f"{fields.get('cal_freq_millihz', 0)}")
            print(row)
            if out:
                out.write(row + "\n")
                out.flush()
            last = (event, diff_ms, ppm, fields)
            if event == "DONE":
                break
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        if out:
            out.close()

    if last is None:
        print("Khong nhan duoc dong W3E2 nao.", file=sys.stderr)
        return 2
    event, diff_ms, ppm, fields = last
    ok = event == "DONE" and abs(ppm) < args.bound_ppm
    print(f"RESULT: {'PASS' if ok else 'FAIL/INCOMPLETE'} event={event} "
          f"drift={diff_ms:+.1f} ms ({ppm:+.1f} ppm, bound {args.bound_ppm:.0f} ppm) "
          f"sleep_entries={fields.get('sleep_entries', 0)} "
          f"rtc_faults={fields.get('rtc_faults', 0)} late_wakes={fields.get('late_wakes', 0)}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
