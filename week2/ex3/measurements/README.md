# W2.3 – Bằng chứng đo ngày 30/10/2026

Thu bằng Serial Debug Assistant, CP210x COM19, 115200 8N1. Nạp/reset sau khi đã mở UART. File gốc giữ nguyên, kể cả các block reset bị lặp; không sửa dòng đo để khớp prediction.

| Case | Raw UART | Trace của block đầu | Gantt từ tool W1.2 | Gantt chú thích |
|---:|---|---|---|---|
| 0 | [log](case0.log) | [CSV](case0.trace.csv) | [SVG](case0.svg) | [SVG](case0_annotated.svg) |
| 1 | [log](case1.log.txt) | [CSV](case1.trace.csv) | [SVG](case1.svg) | [SVG](case1_annotated.svg) |
| 2 | [log](case2.log.txt) | [CSV](case2.trace.csv) | [SVG](case2.svg) | [SVG](case2_annotated.svg) |
| 3 | [log](case3.log.txt) | [CSV](case3.trace.csv) | [SVG](case3.svg) | [SVG](case3_annotated.svg) |
| 4 | [log](case4.log.txt) | [CSV](case4.trace.csv) | [SVG](case4.svg) | [SVG](case4_annotated.svg) |
| 5 | [log](case5.log.txt) | [CSV](case5.trace.csv) | [SVG](case5.svg) | [SVG](case5_annotated.svg) |

[summary.csv](summary.csv) chứa số đo block đầu, số record, overflow và số block trong file. `BLOCKS_IN_FILE` chỉ là đếm đoạn log hoàn chỉnh, không phải xác nhận số phép đo độc lập. Case 0–4 có 5 block; case 5 có 4 block.

Tách duy nhất các hàng `cycles,tag,IN|OUT` giữa `# TRACE` và `# RECORDS` của block đầu. Xác nhận số hàng khớp `# RECORDS`, `overflow=0` và có `# END`. Không đưa nhiều lần reset có CYCCNT quay về 0 vào một lần render.

Tái tạo Gantt gốc từ repo root (dùng `py` thay `python` nếu máy dùng Python Launcher):

```powershell
python tools/trace_gantt.py week2/ex3/measurements/case1.trace.csv --freq-hz 72000000 --svg week2/ex3/measurements/case1.svg
```

Tool báo `trace ended with 2 still switched IN`: M1/tag 2 gọi `tracer_stop()` khi đang chạy rồi mới dump UART. Giữ nguyên sự kiện đó; không thêm OUT giả. Các Gantt chú thích dùng cùng trace, thêm tên task, trục ms và khoảng chờ. Điểm Ready ở tick 2 trên hình ceiling là danh định vì hook chỉ ghi Running IN/OUT.

Kết quả, cách đối chiếu và giới hạn độ phân giải nằm trong [REPORT.md](../REPORT.md). [validation.md](validation.md) ghi build/API check sau khi rà hồ sơ, tách biệt với bằng chứng chạy board.
