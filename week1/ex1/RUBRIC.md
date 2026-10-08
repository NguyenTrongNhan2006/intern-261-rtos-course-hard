# W1.1 – RUBRIC (trọng số lấy từ SYLLABUS.md § Rubric)

Trọng số tuần 1 trong tổng điểm: **20%**. Trong bài này:

| Thành phần | Trọng số | Tiêu chí |
|---|---|---|
| Code | 25% | Build `-Wall -Wextra -Werror` sạch, đúng API FreeRTOS gốc, không HAL/LL/CMSIS-OS, driver thanh ghi đúng (`tools/check_api.sh` PASS) |
| Số đo + báo cáo | 40% | `PREDICTION.md` commit trước khi chạy, ảnh đo logic analyzer, `SETUP.md`, `REPORT.md` giải thích sai lệch định lượng |
| Vấn đáp | 35% | Giải thích từng dòng linker script, flash wait-state, lý do `PendSV` priority thấp nhất; xử lý được mutation/fault tại chỗ |

Ngưỡng đạt: tổng ≥ 7/10. Dùng CMSIS-OS/HAL/LL hoặc file do công cụ sinh ra → phần Code = 0.
