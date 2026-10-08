# W4.3 – Kiểm chứng + fault injection

## Mục tiêu
Đo WCRT/jitter/CPU/stack thực tế của hệ thống W4.2 và so với RTA dự đoán ở W4.1. Chạy các kịch bản fault
injection (treo 1 task, flood UART, rút cảm biến, quá tải CPU): hệ thống phải suy giảm CÓ KIỂM SOÁT, watchdog
phục hồi, ghi lại lý do reset.

## Đọc trước
- Mastering FreeRTOS – chương Run-Time Stats, Stack Usage, chương Software Timers/Watchdog pattern.
- API bắt buộc: `vTaskGetRunTimeStats`, `uxTaskGetStackHighWaterMark`, `xEventGroupWaitBits`.
- Bảng failure mode đã lập ở W4.1 và `include/mixed_criticality_system.h` (W4.2).

## Việc phải làm
1. Đo WCRT thực tế của mọi task (dùng DWT cycle counter quanh điểm vào/ra, hoặc tracer tự viết ở W1.2) trong ít
   nhất 10 phút chạy ổn định; so với R dự đoán ở W4.1, sai số cho phép `W4E3_WCRT_TOLERANCE_PERCENT`.
2. Đo CPU load bằng `vTaskGetRunTimeStats`, đo stack bằng `uxTaskGetStackHighWaterMark` định kỳ (không chỉ 1 lần).
3. Thiết lập watchdog phần cứng (IWDG) với timeout `W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS`; ghi lý do reset lần
   cuối vào vùng `.noinit` (tái dùng kỹ thuật W3.3) để REPORT.md có bằng chứng.
4. Lệnh fault injection qua shell (W4.2) hoặc script host cho từng kịch bản trong `W4E3_ASSIGNED_FAULT_NAMES`
   (xem `PARAMS.md`): ví dụ treo 1 task (busy-wait vô hạn ở priority thấp), flood UART (gửi liên tục nhanh hơn
   `W4E2_SHELL_CMD_TIMEOUT_MS`), rút cảm biến (ngắt I2C1 SDA/SCL), quá tải CPU (thêm 1 task busy-wait priority cao).
5. Với mỗi kịch bản: dùng `xEventGroupWaitBits` để đồng bộ việc quan sát trạng thái suy giảm (ví dụ chờ bit
   "degraded_mode_entered" trước khi coi là đã phản ứng), đo xem control loop có trễ deadline hay không và tại
   sao (có nằm trong thiết kế suy giảm đã công bố ở W4.1 hay không).
6. Lập bảng dự đoán RTA vs đo cho mọi kịch bản; ghi log watchdog reset reason từng lần.

## Sản phẩm nộp
- Bảng WCRT dự đoán vs đo (mọi task, chạy ổn định, không fault).
- Log từng kịch bản fault: hành vi quan sát, có trễ deadline không, lý do reset nếu có.
- Implement `include/fault_injection.h` khai báo API điều khiển từng kịch bản fault.

## Tiêu chí đạt
Control loop không trễ deadline ở mọi kịch bản fault, TRỪ KHI đã được thiết kế (ở W4.1) để suy giảm có kiểm
soát; watchdog phục hồi đúng trong mọi trường hợp treo.

## Giờ dự kiến
4 giờ.
