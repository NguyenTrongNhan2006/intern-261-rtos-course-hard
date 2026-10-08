# W4.2 – Hiện thực

## Mục tiêu
Hiện thực hệ thống mixed-criticality đã được mentor DUYỆT ở W4.1: control loop 1 kHz (TIM), IMU 500 Hz
(driver I2C W2.1), telemetry UART DMA (W3.1), shell lệnh, tickless khi rảnh (W3.2). Static only, không
`malloc`/API động nào, `check_api.sh` sạch, `-Wall -Wextra -Werror`.

## Đọc trước
- Design doc đã được mentor duyệt ở W4.1 (bảng RTA, ngân sách RAM/CPU, failure mode) — bám sát, không tự ý đổi
  priority/chu kỳ mà không chạy lại RTA.
- Ôn lại driver/cơ chế của chính bạn: W2.1 (I2C1), W3.1 (UART DMA + stream buffer), W3.2 (tickless + STOP mode).

## Việc phải làm
1. Tạo đúng `W4E2_NUM_TASKS` task với priority đúng thứ tự trong `params.h` (`W4E2_TASK_PRIORITY`), khớp bảng
   đã duyệt ở W4.1 (`mixed_criticality_design.h`).
2. Control task: chạy ở 1 kHz, kích bằng so sánh TIM + notification (không dùng `vTaskDelay` để trigger theo
   tick vì tick không đủ chính xác và không cho phép tickless hoạt động đúng khi hệ thống rảnh).
3. IMU task: đọc MPU6050 qua driver I2C1 (W2.1) ở 500 Hz, đẩy dữ liệu cho control qua queue/notification.
4. Telemetry: tái dùng driver UART DMA (W3.1) để gửi dữ liệu trạng thái định kỳ, không block control loop.
5. Shell: nhận lệnh qua UART (polling hoặc dùng lại stream buffer rảnh), timeout parse theo
   `W4E2_SHELL_CMD_TIMEOUT_MS`; tối thiểu lệnh đọc trạng thái hệ thống và lệnh đổi 1 tham số runtime (vd. chu
   kỳ telemetry) phải có.
6. Tickless: khi không có task nào ready ngoài idle, vào STOP mode (W3.2), đảm bảo control loop 1 kHz không bị
   trễ khi thức dậy.
7. Implement `include/mixed_criticality_system.h` (chỉ khai báo API, thân hàm sinh viên tự viết trong `.c` KHÔNG
   nộp ở hard-track này) cho init/hook của mỗi task.
8. Đo `uxTaskGetStackHighWaterMark()` mỗi task, so với ngân sách đã khai ở W4.1, lệch không quá
   `W4E2_STACK_MARGIN_TOLERANCE_WORDS`.

## Sản phẩm nộp
- `include/mixed_criticality_system.h` khai báo đủ API.
- Bảng priority/stack thực đo so với thiết kế đã duyệt.
- `tools/check_api.sh` PASS, build sạch `-Wall -Wextra -Werror`.

## Tiêu chí đạt
Build sạch, 0 heap, 0 HAL/LL/CMSIS-OS, priority/API khớp thiết kế đã duyệt ở W4.1.

## Giờ dự kiến
7 giờ.
