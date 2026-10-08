# W4.2 – REPORT

## Số đo
- Bảng stack high-water-mark thực đo mỗi task (bytes/words) so với ngân sách đã khai ở W4.1:

| Task | Stack budget (W4.1) | High-water-mark đo được | Chênh lệch |
|---|---|---|---|
| control | | | |
| imu | | | |
| telemetry | | | |
| shell | | | |
| idle_aux | | | |

- CPU load đo được (run-time stats) khi hệ thống chạy ổn định: ___ %.
- `tools/check_api.sh` kết quả: ___.

## Giải thích sai lệch (ĐỊNH LƯỢNG)
So PREDICTION.md với số đo thực tế; nếu stack/CPU lệch nhiều so với dự đoán ở W4.1/W4.2, định lượng nguồn gốc
(ước lượng WCET/stack ban đầu sai, overhead ISR thực tế lớn hơn...).

## Trả lời câu hỏi trade-off
Vì sao control task dùng TIM + notification thay vì `vTaskDelay` theo tick? Chứng minh bằng chính số đo jitter
của bạn (so với thiết kế jitter bound ở `W4E1_CONTROL_LOOP_JITTER_MAX_US`).
