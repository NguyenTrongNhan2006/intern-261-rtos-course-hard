# W2.4 – Build, API và kiểm tra dữ liệu ngày 30/10/2026

Kiểm tra source hiện tại bằng Arm GNU GCC 14.2.1, CMake/Ninja, Cortex-M3, `-O2`, `-Wall -Wextra -Werror`. Không nạp/đo lại board trong bước này.

## Build

```powershell
cmake --preset debug -B build/w2e4-review '-DCMAKE_C_FLAGS_DEBUG=-O2 -g3' -DW2E4_BURST_PERIOD_MS=10 -DW2E4_BARRIER_LATE_MS=0
cmake --build build/w2e4-review --target w2e4 w2e4_fixed
```

| Target | USE_FIXED | Compile/link | FLASH (B) | RAM (B) |
|---|---:|---|---:|---:|
| w2e4 | 0 | PASS | 11608 | 6800 |
| w2e4_fixed | 1 | PASS | 11624 | 6800 |

Hai bản vẫn có binary semaphore dùng riêng cho benchmark. Cùng RAM linker không mâu thuẫn với dự toán RAM_SYNC giảm 80 byte khi thực sự bỏ đối tượng semaphore.

## API check

```text
tools/check_api.sh week2/ex4
check_api: PASS (4 files)
```

Phạm vi gồm sync_app.c, sync_app.h, FreeRTOSConfig.h và params.h đang được sinh cho MSSV. PASS kiểm tra quy tắc API/static allocation, không phải chứng minh đầy đủ hành vi runtime.

## Dữ liệu và báo cáo

`python tools/analyze_w2e4.py --figures` kiểm tra mỗi file có một biến thể đúng và một END; timestamp EV tăng; đủ S=1…1000; 500 nhóm S,S,R có giá trị đúng mô hình. Buggy có 500 R trả sequence chẵn, fixed có 500 R count=2. Counter cuối và status khớp với phép đếm độc lập từ record; TRACE_DROPPED=0.

Mỗi Gantt có đúng 254 record như metadata, không overflow và không có IN/OUT ghép sai. Gantt dừng sớm; event trace vẫn đầy đủ. Các con số timing/cycle/RAM trong REPORT lấy từ raw log và audit.json, không sửa prediction để hợp số đo.

Kiểm tra liên kết Markdown, cú pháp tool, XML của SVG và khoảng trắng trong phạm vi thay đổi. Đọc hình render của hai biểu đồ để kiểm tra nhãn và trục. Chỉ sửa comment header để khớp implementation; không đổi luồng thực thi firmware đã đo.

Chưa có log thử BARRIER_LATE_MS>101 hoặc BURST_PERIOD_MS khác 10. Không suy ra những trường hợp đó đã chạy từ một build PASS. Raw UART không có firmware hash; không khẳng định binary hash của lần build mới trùng image đã đo.
