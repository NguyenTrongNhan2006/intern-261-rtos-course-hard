# W2.4 – Dữ liệu đo và kiểm toán

Hai file gốc thu bằng UART 115200 8N1 trong Serial Debug Assistant, mỗi file có một lượt hoàn chỉnh tới `# END`. Giữ nguyên file, không chèn/sửa record đo.

| Lượt | Raw UART | Notification EV | Task-switch trace | Gantt đoạn đầu |
|---|---|---|---|---|
| Buggy, USE_FIXED=0 | [log](buggy.log.txt) | [CSV](buggy.events.csv) | [CSV](buggy.gantt.trace.csv) | [SVG](buggy.gantt.svg) |
| Fixed, USE_FIXED=1 | [log](fixed.log.txt) | [CSV](fixed.events.csv) | [CSV](fixed.gantt.trace.csv) | [SVG](fixed.gantt.svg) |

[summary.csv](summary.csv) chứa counter/RAM/cycle của hai lượt. [audit.json](audit.json) ghi các metadata, record count, nhóm đầu và thống kê timing tính lại từ trace.

Biểu đồ từ các EV thực tế:

- [first_pair.svg](first_pair.svg): marker hai lần gửi và một lần nhận ở burst đầu. R của buggy là sequence mới nhất; R của fixed là count.
- [event_counts.svg](event_counts.svg): tổng S so với số sự kiện R nhận được trong toàn lượt khoảng 5 s. Không cộng sequence payload của buggy như số sự kiện.

EV có 1500 record/lượt và TRACE_DROPPED=0. Gantt có 254 record ghép IN/OUT đầy đủ nhưng dừng khoảng 0.29 s vì tracer W1.2 chủ động dừng trước khi buffer đầy. Không diễn giải overflow=0 là Gantt đủ cả 5 s.

Chạy lại từ repo root, dùng `py` thay `python` nếu máy dùng Python Launcher:

```powershell
python tools/analyze_w2e4.py
python tools/analyze_w2e4.py --figures
```

Lệnh đầu dùng Python standard library và renderer W1.2, kiểm tra log trước khi xuất CSV/JSON/Gantt. Lệnh `--figures` cần thêm matplotlib để vẽ hai biểu đồ sự kiện. Script không viết lại raw log.

Tái tạo riêng một Gantt bằng tool W1.2:

```powershell
python tools/trace_gantt.py week2/ex4/measurements/fixed.gantt.trace.csv --freq-hz 72000000 --svg week2/ex4/measurements/fixed.gantt.svg
```

Diễn giải kết quả ở [REPORT.md](../REPORT.md); build/API check ở [validation.md](validation.md). Hai lượt dùng BURST_PERIOD_MS=10, BARRIER_LATE_MS=0; chưa có phép đo ép timeout hoặc tăng tốc độ gửi.
