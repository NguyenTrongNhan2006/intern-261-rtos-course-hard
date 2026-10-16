# Dữ liệu đo W1E3

Các file trong thư mục này là dữ liệu đã có từ các phép đo W1E3 V1/V2.
Việc gom thư mục giữ nguyên từng byte của 16 file dữ liệu và kết quả.

| Nhóm | V1 | V2 |
| :--- | :--- | :--- |
| UART | trace_O0.csv, trace_O2.csv | trace_v2_O0.csv, trace_v2_O2.csv |
| Logic Analyzer | capture_O0.sr, capture_O2.sr | capture_v2_O0.sr, capture_v2_O2.sr |
| Gantt | Không có SVG riêng trong bộ dữ liệu này | [O0](gantt_v2_O0.svg), [O2](gantt_v2_O2.svg) |
| Phân tích UART | Bảng trong [REPORT.md](../REPORT.md) | results_v2_O0.json, results_v2_O2.json |
| Đối chiếu LA/UART | Bảng trong [REPORT.md](../REPORT.md) | correlation_v2_O0.json, correlation_v2_O2.json |
| Log nạp | Theo bằng chứng lịch sử trong report | flash_v2_O0.txt, flash_v2_O2.txt |
| SHA256/provenance | BASELINE_V1_MANIFEST.json | V2_MANIFEST.json |

BASELINE_V1_MANIFEST.json là snapshot lịch sử tại commit a624a01, giữ
nguyên nội dung. Các key week1/ex3/trace_*.csv và capture_*.sr trong đó
được tìm theo cùng tên ở thư mục này nếu file còn trong bản nộp hiện tại.
Capture lỗi capture_O2_first.sr chỉ có trong lịch sử Git như report nêu.
Hash nguồn/report/ELF trong manifest lịch sử thuộc thời điểm snapshot,
không phải cam kết rằng working tree hiện tại giống toàn bộ snapshot đó.
V2_MANIFEST.json cập nhật đường dẫn dữ liệu nhưng giữ nguyên SHA256.

Hai prediction ở thư mục cha giữ nguyên nội dung đã ghi, kể cả tên file
trước khi gom. Gantt chỉ bao phủ scheduler ring 254 event; CSV JOB/JOB2
mới chứa đủ 205 job. LA và UART là các lượt RESET riêng.

Từ repo root, dùng Python 3 để tái phân tích:

```powershell
$w1e3Python = 'C:\Users\Nhan\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $w1e3Python tools/analyze_w1e3_v2.py week1/ex3/measurements/trace_v2_O0.csv
& $w1e3Python tools/analyze_w1e3_v2.py week1/ex3/measurements/trace_v2_O2.csv
& $w1e3Python tools/trace_correlate.py week1/ex3/measurements/capture_v2_O0.sr week1/ex3/measurements/trace_v2_O0.csv --channels 0,1,2,3,4
& $w1e3Python tools/trace_correlate.py week1/ex3/measurements/capture_v2_O2.sr week1/ex3/measurements/trace_v2_O2.csv --channels 0,1,2,3,4
```

Lượt thu tiếp theo lưu ở đây với tên mới để không ghi đè bằng chứng đã
được kiểm chứng. File .pvs chỉ là bố cục PulseView và đã được Git bỏ qua.
