#!/usr/bin/env python3
"""W3.1: chạy tools/uart_stress.py đúng tốc độ --rate-hz và thu luôn log của MCU.

Hai việc wrapper này làm, script gốc giữ nguyên:
1. uart_stress.open_serial() mở cổng với timeout=0.1, nên ser.read(4096) chặn tới
   100 ms mỗi vòng và tốc độ thật chỉ khoảng 9,9 Hz dù đặt --rate-hz 67. Wrapper mở
   cổng với timeout=0 để read() trả về ngay.
2. Log "W3E1 ..." của MCU đi chung cổng COM. Script gốc đọc rồi bỏ đi, còn đóng cổng
   rồi mở lại bằng app khác có thể reset board (DTR/RTS) và xóa bộ đếm. Wrapper giữ lại
   mọi byte đọc được, chờ thêm --drain-sec giây để bắt dòng RX_QUIET, rồi lưu file và
   in bảng tóm tắt CPU load trước khi nhả cổng.

Usage (cùng tham số với uart_stress.py, thêm hai tùy chọn của wrapper):
    py tools/uart_stress_w3e1.py --port COM19 --baud 460800 --payload-size 59 \\
        --rate-hz 67 --duration-sec 600 --log-csv w3e1_run.csv \\
        --mcu-log w3e1_mcu.log --drain-sec 8
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import uart_stress  # noqa: E402

_original_open_serial = uart_stress.open_serial

FIELD_RE = re.compile(r"(\w+)=(\d+)")
ERROR_FIELDS = ("crc", "lost", "len_err", "old_seq", "dropped", "dma_late",
                "dma_err", "ore", "fe", "ne", "clear_busy")


def parse_fields(line):
    """Trả dict {tên: số} từ một dòng log "W3E1 ... key=value ...". """
    return {k: int(v) for k, v in FIELD_RE.findall(line)}


def summarize(lines):
    """In bảng CPU load của các giây có nhận dữ liệu và kiểm bộ đếm lỗi dòng cuối."""
    rows = [parse_fields(ln) for ln in lines if ln.startswith(("W3E1 SAMPLE", "W3E1 RX_QUIET"))]
    if not rows:
        print("MCU: khong co dong SAMPLE/RX_QUIET nao (kiem tra day TX/RX, baud, va reset).")
        return False

    active = []
    prev_bytes = 0
    for row in rows:
        if row.get("bytes", 0) > prev_bytes:
            active.append(row)
        prev_bytes = row.get("bytes", prev_bytes)

    print(f"MCU: {len(rows)} dong thong ke, {len(active)} giay co nhan du lieu")
    if active:
        print("CPU load luc nhan du lieu (%):  min    tb     max")
        for key, label in (("parser_pct_x100", "Parser"), ("report_pct_x100", "Report"),
                           ("busy_pct_x100", "Busy  ")):
            vals = [r[key] / 100.0 for r in active if key in r]
            if vals:
                print(f"  {label}                     {min(vals):5.2f}  {sum(vals) / len(vals):5.2f}  "
                      f"{max(vals):5.2f}")
        print(f"  peak Stream Buffer (byte): {max(r.get('peak', 0) for r in active)}")

    final = rows[-1]
    bad = {k: final[k] for k in ERROR_FIELDS if final.get(k, 0) != 0}
    print(f"MCU cuoi: bytes={final.get('bytes')} ok={final.get('ok')} "
          f"restarts={final.get('restarts')} last_seq={final.get('last_seq')}")
    print("MCU RESULT:", "PASS (moi bo dem loi = 0)" if not bad else f"FAIL {bad}")
    return not bad


class TeeSerial:
    """Bọc đối tượng Serial: chuyển tiếp mọi thứ và giữ bản sao byte nhận được."""

    def __init__(self, ser, log_path, drain_sec):
        self._ser = ser
        self._log_path = log_path
        self._drain_sec = drain_sec
        self._rx = bytearray()

    def write(self, data):
        return self._ser.write(data)

    def read(self, size=1):
        chunk = self._ser.read(size)
        if chunk:
            self._rx += chunk
        return chunk

    @property
    def in_waiting(self):
        return self._ser.in_waiting

    def close(self):
        end = time.monotonic() + self._drain_sec
        while time.monotonic() < end:
            if not self.read(4096):
                time.sleep(0.01)
        self._ser.close()

        text = self._rx.decode("ascii", errors="ignore")
        # Tìm "W3E1 " trong dòng thay vì startswith, để byte rác dính đầu dòng không làm mất dòng.
        lines = [ln[ln.index("W3E1 "):].strip() for ln in text.splitlines() if "W3E1 " in ln]
        with open(self._log_path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        print(f"MCU log: {len(lines)} dong -> {self._log_path}")
        summarize(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--mcu-log", default="w3e1_mcu.log")
    ap.add_argument("--drain-sec", type=float, default=8.0)
    own, rest = ap.parse_known_args(sys.argv[1:] if argv is None else argv)

    if sys.platform == "win32" and sys.version_info < (3, 11):
        print("warning: Python < 3.11 tren Windows lam tron time.sleep(0.001) len ~15,6 ms, "
              "toc do gui co the thap hon --rate-hz. Kiem tra gia tri sent= sau khi chay.",
              file=sys.stderr)

    def open_serial_wrapped(port, baud, timeout=0.1):
        del timeout  # Bỏ timeout gốc: read() phải trả về ngay để vòng gửi giữ đúng nhịp.
        ser = _original_open_serial(port, baud, timeout=0)
        return TeeSerial(ser, own.mcu_log, own.drain_sec)

    uart_stress.open_serial = open_serial_wrapped
    return uart_stress.main(rest)


if __name__ == "__main__":
    sys.exit(main())
