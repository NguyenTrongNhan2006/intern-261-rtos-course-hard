# Kiểm tra W2.1–W2.3 ngày 30/10/2026

Kiểm tra source sau khi rà báo cáo, không nạp firmware hoặc tạo thêm phép đo board. Toolchain: Arm GNU GCC 14.2.1, CMake/Ninja, Cortex-M3, `-O2`, `-Wall -Wextra -Werror`. Build directory riêng: `build/w2-review`.

## API check

```text
tools/check_api.sh week2/ex1 week2/ex2 week2/ex3
check_api: PASS (14 files)
```

## Compile và link

| Target / cấu hình | Kết quả | FLASH (B) | RAM (B) |
|---|---|---:|---:|
| W2E1: FAULT_INJECT=OFF, TRACE_EV=OFF | PASS | 10512 | 3260 |
| W2E1: FAULT_INJECT=ON, TRACE_EV=ON | PASS | 10856 | 3852 |
| W2E2 | PASS | 8084 | 6116 |
| W2E3: TEST_CASE=0 | PASS | 9920 | 7644 |
| W2E3: TEST_CASE=1 | PASS | 9872 | 7644 |
| W2E3: TEST_CASE=2 | PASS | 9888 | 7644 |
| W2E3: TEST_CASE=3 | PASS | 10144 | 7660 |
| W2E3: TEST_CASE=4 | PASS | 10208 | 7660 |
| W2E3: TEST_CASE=5 | PASS | 10224 | 7660 |

Lệnh configure/build từ repo root, ví dụ:

```powershell
cmake --preset debug -B build/w2-review '-DCMAKE_C_FLAGS_DEBUG=-O2 -g3' -DW2E1_FAULT_INJECT=ON -DW2E1_TRACE_EV=ON -DW2E3_TEST_CASE=0
cmake --build build/w2-review --target w2e1 w2e2 w2e3
```

Lần lượt đổi `W2E3_TEST_CASE=1…5`, configure lại rồi build target `w2e3`. Đổi hai option W2E1 sang OFF và build `w2e1` để kiểm tra bản không chèn lỗi/marker.

## Kiểm tra dữ liệu và hồ sơ

- W2E1: đếm lại các dòng đủ 13 cột sau banner; 38/38/40 RECOVERED, mỗi dòng có OK kế tiếp tăng streak. Đọc sample `.sr` để kiểm tra EV/SCL, thời điểm ER và START/STOP GPIO. PULSES luôn 0, nên còn thiếu phép đo nhánh phát clock khi SDA-stuck.
- W2E2: đọc trực tiếp ảnh UART gốc; đối chiếu số stress/benchmark, params và điều kiện API trong source. Không tạo raw UART giả từ nội dung ảnh.
- W2E3: lấy block hoàn chỉnh đầu tiên mỗi case, số trace lần lượt 23/27/15/17/19/15; mọi overflow=0. Đọc lại các block lặp; không gán chúng thành các mẫu thống kê độc lập. Summary CSV khớp header và dòng số đo gốc.
- Gantt W2E3 dùng renderer `tools/trace_gantt.py`. Mỗi case có đúng một unmatched IN cuối của M1 do dừng trace khi M1 đang chạy; không thêm OUT giả.
- Hình logic W2E1 và Gantt chú thích W2E3 được dựng từ dữ liệu gốc; kiểm tra lại bằng hình render. Link nội bộ và bảng đối chiếu được kiểm tra sau biên tập.
- `git diff --check` áp dụng phạm vi W2.1–W2.3, AI_LOG, CMake và tool capture; không lấy lỗi khoảng trắng của bài khác làm kết quả cho phạm vi này.

Các file UART không chứa firmware commit hash; không tuyên bố ánh xạ hash binary của lượt đo cũ chỉ từ lần build mới. Số FLASH/RAM ở bảng này là số linker hiện tại; kết quả trên board vẫn dựa vào raw log/capture đã lưu.
