# W1.2 – Tracer tự viết + đo context switch

## Mục tiêu
Hiện thực trace hook ghi lại lịch sử chuyển task, đối chiếu với logic analyzer, và đo thời gian context switch.

## Đọc trước
- freertos.org: *Trace Hook Macros* (`traceTASK_SWITCHED_IN`, `traceTASK_SWITCHED_OUT`, `vTaskSetApplicationTaskTag`)
- PM0056 chương 12 (DWT – CYCCNT, cách bật `DWT->CTRL.CYCCNTENA`)
- Mastering chương 2 (scheduler) để hiểu thời điểm hook được gọi

## Việc phải làm
1. Trong `FreeRTOSConfig.h` (của bạn, trong `platform/`), bật `configUSE_TRACE_FACILITY` và định nghĩa
   `traceTASK_SWITCHED_IN` / `traceTASK_SWITCHED_OUT` gọi vào `tracer_on_switched_in/out()` (xem `include/tracer.h`).
2. Mỗi hook: toggle 1 trong 3 chân GPIO đại diện cho task đang chạy (dùng `vTaskSetApplicationTaskTag` để map
   TCB → chân), và ghi một bản ghi (tag, timestamp = `DWT->CYCCNT`) vào ring buffer tĩnh (không `malloc`).
3. Dump ring buffer qua UART (driver thanh ghi tự viết ở W1.1) khi đầy hoặc theo lệnh.
4. `tools/trace_gantt.py` (đã có sẵn trong repo – đọc, đừng viết lại) nhận log UART, vẽ Gantt.
5. Đo thời gian context switch (cycle DWT) min/max ở hai mức tối ưu hoá `-O0` và `-O2`; so sánh.

## Sản phẩm nộp
- Tracer hoạt động, Gantt chart xuất ra từ `tools/trace_gantt.py`.
- Bảng thời gian context switch (min/max, cycle) ở `-O0` và `-O2`.
- Ảnh logic analyzer đối chiếu với Gantt từ tracer.

## Tiêu chí đạt
Gantt từ tracer khớp ảnh logic analyzer (sai số ≤ `W1E2_CORRELATION_TOLERANCE_US`, xem `PARAMS.md`); giải thích được
chênh lệch thời gian context switch giữa `-O0` và `-O2`.

## Giờ dự kiến
4 giờ.
