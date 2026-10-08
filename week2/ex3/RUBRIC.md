# W2.3 – RUBRIC (trọng số lấy từ SYLLABUS.md § Rubric)

Trọng số tuần 2 trong tổng điểm: **25%**. Trong bài này:

| Thành phần | Trọng số | Tiêu chí |
|---|---|---|
| Code | 25% | Dùng đúng `xSemaphoreCreateBinaryStatic`/`xSemaphoreCreateMutexStatic`/`vTaskPrioritySet`/`uxTaskPriorityGet`, wrapper ceiling tự viết trên API gốc (không sửa kernel), `tools/check_api.sh` PASS |
| Số đo + báo cáo | 40% | PREDICTION trước khi chạy, Gantt cho cả 3 hiện tượng + sau ceiling, bảng thời gian block, giải thích định lượng |
| Vấn đáp | 35% | Chỉ đúng điểm chained blocking trên Gantt, giải thích đúng cơ chế ceiling loại bỏ deadlock, xử lý khi mentor đổi thứ tự lock/priority tại chỗ |

Ngưỡng đạt: tổng ≥ 7/10.
