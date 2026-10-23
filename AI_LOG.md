# AI_LOG – Nhật ký sử dụng AI

Nguyễn Trọng Nhân – MSSV 2452882. Cập nhật: 23/10/2026.

AI sử dụng: Codex; Claude hỗ trợ review W1E2. Các prompt dưới đây được tóm tắt từ yêu cầu và nhật ký đã lưu, không phải trích nguyên văn. AI hỗ trợ giải thích, đề xuất code, rà lỗi và trình bày tài liệu; em chịu trách nhiệm đọc lại, kiểm tra và giải thích bài nộp.

## W1E1 – Clock, GPIO và FreeRTOS

Prompt tóm tắt: hướng dẫn cấu hình clock, tạo xung TIM 1 kHz và kiểm tra lịch task. AI giải thích startup, linker, RCC, Flash, GPIO và scheduler; hỗ trợ code và lệnh build/flash. Em nối dây, nạp và đo bằng PulseView. Nhật ký đã ghi nhận Verified OK, TIM khoảng 1 kHz và chu kỳ task A khoảng 264 ms.

## W1E2 – Tracer và đo context switch

Prompt tóm tắt: xây dựng tracer, rà lỗi và hướng dẫn đo lại O0/O2. AI hỗ trợ UART, tracer, cấu hình FreeRTOS, phân tích CSV/logic analyzer và rà báo cáo. Em nạp firmware, thu UART và PulseView. Bằng chứng đo lại được lưu trong week1/ex2; kết quả chỉ áp dụng cho cấu hình đã đo.

## W1E3 – RM, RTA và WCRT

Prompt tóm tắt: phân tích RTA, chuẩn bị prediction, workload và hướng dẫn build O0/O2. AI hỗ trợ code, DWT calibration, IRQ profiling, parser và bố cục báo cáo. Theo xác nhận mới nhất của em: W1E3 mới build, chưa nạp lên board. Các số liệu/capture đã lưu trước đó chưa được xác nhận cho lượt hiện tại; chưa kết luận WCRT hoặc timing phần cứng.

## W1E4 – Heap và cấp phát tĩnh

Prompt tóm tắt: hướng dẫn thử heap_4, chuyển sang static allocation, tinh chỉnh stack và gom log. AI hỗ trợ code, tính RAM, đọc high-water mark/linker map và rà báo cáo. Em build, nạp và thu UART. Các log heap cùng ba firmware static được lưu trong measurements/; khả năng an toàn stack chỉ được đánh giá trong các lượt đã chạy.

## W2E1 – Driver I2C1 và bus recovery

Prompt tóm tắt: viết driver theo STEP, tối ưu code và rút gọn note thanh ghi. AI hỗ trợ FSM, ISR, mutex, task notification, timeout, recovery, hai task MPU6050 và host test. Em thực hiện nối dây, nạp và cung cấp ảnh UART INIT/ACCEL/GYRO OK. Ảnh chưa chứng minh recovery 11 lần liên tiếp; mock trên PC không thay thế phép đo board.

## W2E2 – SPSC ring buffer và Queue

Prompt tóm tắt: viết prediction và code tối giản để tự gõ, chạy stress test và so sánh cycle với Queue. AI hỗ trợ spsc_ring.h, DMB, ISR producer, task consumer, DWT benchmark và CMake. Em build, nạp và cung cấp ảnh UART: stress 1.000.000 phần tử PASS; benchmark 7.048 mẫu PASS cho cả Ring và Queue. Ảnh nằm trong week2/ex2/measurements/.

## W2E3 – Blocking và Priority Ceiling

Prompt tóm tắt: cung cấp blocking_app.c, priority_ceiling.c, FreeRTOSConfig.h và prediction ngắn gọn. AI hỗ trợ các kịch bản inversion, chained blocking, deadlock, wrapper ceiling và tích hợp CMake/tracer. Đã kiểm tra build và API; chưa có kết quả nạp/đo trên board. Các thời gian trong PREDICTION.md vẫn là dự đoán.

## Giới hạn kiểm chứng

Build, API check và host test không chứng minh firmware chạy đúng trên phần cứng. Các kết quả phải gắn với đúng phiên bản, log hoặc ảnh đã thu. Em cần tự giải thích code và đối chiếu dự đoán với phép đo khi bảo vệ bài.
