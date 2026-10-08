# W2.2 – Ring buffer lock-free vs queue

## Mục tiêu
Tự viết SPSC (single-producer single-consumer) ring buffer lock-free dùng memory barrier đúng cách, chứng minh
không mất/lặp phần tử với 1 triệu phần tử, rồi benchmark cycle so với `xQueueSendFromISR`/`xQueueReceive`.

## Đọc trước
- PM0056 chương 10 (DMB/DSB/ISB, Load-Store Exclusive) – đọc kỹ phần memory ordering trên Cortex-M3.
- Mastering ch.5 (Queue) để hiểu cơ chế `xQueueSendFromISR` làm gì bên trong (copy + wake).
- freertos.org *Queue API*.

## Việc phải làm
1. Hiện thực `include/spsc_ring.h`: ring buffer tĩnh (mảng cố định, không `malloc`), chỉ số head/tail kiểu
   `volatile`, dùng `__DMB()`/`__DSB()` đúng chỗ (publish dữ liệu trước khi publish chỉ số).
2. ISR (producer) gọi hàm push; 1 task (consumer) gọi hàm pop. Không dùng mutex/semaphore nào cho ring buffer này
   (đó chính là điều phải chứng minh là an toàn).
3. Stress test: ISR định kỳ (timer) ghi 1 sequence counter tăng dần `W2E2_STRESS_COUNT` lần (`params.h`); consumer
   kiểm tra không có giá trị nào bị mất hoặc lặp, log lỗi đầu tiên nếu có.
4. Benchmark: đo cycle DWT trung bình của 1 lần push/pop trên ring buffer tự viết, so với `xQueueSendFromISR` +
   `xQueueReceive` trên 1 queue có cùng kích thước phần tử, lặp `W2E2_BENCH_ITERATIONS` lần.
5. Viết trong REPORT.md lập luận vì sao SPSC (1 producer, 1 consumer, không cần SMP) an toàn không cần lock trên
   Cortex-M3 đơn nhân, và chỉ ra 1 tình huống (ví dụ nhiều producer, hoặc producer ghi lại struct nhiều field không
   atomic) khi nào lập luận đó KHÔNG còn đúng.

## Sản phẩm nộp
- Code ring buffer + stress test + benchmark.
- Bằng chứng đúng: log stress test 1 triệu phần tử không mất/lặp.
- Bảng cycle so sánh ring buffer tự viết vs queue gốc.

## Tiêu chí đạt
Lập luận được vì sao SPSC an toàn không cần lock, và chỉ ra chính xác khi nào thì không còn đúng.

## Giờ dự kiến
3 giờ.
