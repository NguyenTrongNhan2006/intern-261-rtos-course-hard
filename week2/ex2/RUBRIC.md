# W2.2 – RUBRIC (trọng số lấy từ SYLLABUS.md § Rubric)

Trọng số tuần 2 trong tổng điểm: **25%**. Trong bài này:

| Thành phần | Trọng số | Tiêu chí |
|---|---|---|
| Code | 25% | Ring buffer tĩnh (không `malloc`), memory barrier đúng vị trí, dùng đúng `xQueueSendFromISR`/`xQueueReceive`/`portYIELD_FROM_ISR` cho phần đối chứng, `tools/check_api.sh` PASS |
| Số đo + báo cáo | 40% | PREDICTION trước khi chạy, log stress test 1 triệu phần tử không mất/lặp, bảng cycle, giải thích định lượng |
| Vấn đáp | 35% | Lập luận đúng vì sao SPSC an toàn không cần lock và chỉ ra đúng ranh giới khi nào không còn đúng |

Ngưỡng đạt: tổng ≥ 7/10.
