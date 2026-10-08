# W4.3 – RUBRIC (trọng số lấy từ SYLLABUS.md § Rubric)

Trọng số tuần 4 trong tổng điểm: **30%**. Trong bài này:

| Thành phần | Trọng số | Tiêu chí |
|---|---|---|
| Code | 25% | `include/fault_injection.h` đúng interface, dùng đúng API bắt buộc (`vTaskGetRunTimeStats`, `uxTaskGetStackHighWaterMark`, `xEventGroupWaitBits`) |
| Số đo + báo cáo | 40% | PREDICTION trước khi chạy, bảng RTA dự đoán vs đo, log đủ mọi kịch bản fault được giao |
| Vấn đáp | 35% | Giải thích đúng suy giảm có kiểm soát vs không kiểm soát, xử lý khi mentor tiêm thêm fault tại chỗ |

Ngưỡng đạt: tổng ≥ 7/10. Control loop trễ deadline ở kịch bản KHÔNG được công bố là suy giảm → phần Số đo + báo cáo tối đa 50%.
