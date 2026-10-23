# W1.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

- MSSV: 2452882. Nguyễn Trọng Nhân

## Giả định và phạm vi phép đo

- CPU chạy ở 72 MHz; 72 cycle tương ứng 1 us.
- Đo khoảng từ timestamp trong hook OUT đến timestamp trong hook IN kế tiếp, cùng cách đặt timestamp ngay đầu hai hook.
- Khoảng OUT → IN phản ánh scheduler work và phần instrumentation nằm giữa hai timestamp; không bao phủ toàn bộ exception entry/exit và save/restore của PendSV.
- Hook chỉ đọc DWT, cập nhật GPIO và ghi static ring buffer; không malloc, không blocking, không gửi UART trong hook.
- So sánh -O0/-O2 với cùng task workload, tick rate, GPIO mapping và ring capacity; không có ISR tải nặng trong capture baseline.
- Capture ngắn, không có CYCCNT wrap, không mất record; timestamp và GPIO edge được đối chiếu sau khi căn cùng time origin.

## Dự đoán thời gian context switch

- Dự đoán min/max cycle context switch ở `-O0`: 300/700 cycle, tương ứng khoảng 4.17/9.72 us.
- Dự đoán min/max cycle context switch ở `-O2`: 120/350 cycle, tương ứng khoảng 1.67/4.86 us.
- Đây là budget sơ bộ cho C scheduler code, function-call overhead, GPIO access và buffer write trong khoảng đo. Chưa có implementation/disassembly để xác lập cycle count chính xác.
- Dự đoán `-O2` nhanh hơn vì compiler có thể giảm stack access, loại phép tính dư, giữ biến trong register và inline một số hàm. Phần assembly PendSV và volatile peripheral access không tự biến mất khi đổi optimization.
- Jitter dự đoán ở `-O0`: 700 - 300 = 400 cycle.
- Jitter dự đoán ở `-O2`: 350 - 120 = 230 cycle, khoảng 3.19 us.
- So với `W1E2_CTXSW_JITTER_MAX_CYCLES_O2 = 355`: dự đoán đạt vì 230 <= 355; còn 125 cycle dự phòng. Kết luận thực tế phải dựa trên số đo.
- Max có thể tăng khi scheduler chọn task theo đường xử lý khác, hook có overhead lớn hơn giả định hoặc ISR priority cao chen vào. Nếu xảy ra, giữ nguyên prediction và giải thích sai lệch trong REPORT.md.

## Dự đoán đối chiếu Gantt

- Sai số kỳ vọng lớn nhất giữa tracer và logic analyzer: 4 us.
- So với `W1E2_CORRELATION_TOLERANCE_US = 16`: dự đoán đạt vì 4 <= 16.
- Budget sai số giả định: timestamp-to-GPIO offset <= 2 us; clock drift <= 1 us trong cửa sổ 10 ms nếu chênh lệch clock <= 100 ppm; sampling khoảng 0.042 us ở 24 MHz; phần còn lại dành cho alignment uncertainty.
- Sampling rate 24 MHz, clock difference 100 ppm và GPIO offset 2 us là giả định cần kiểm chứng khi đo, không phải thông số đã được xác nhận của thiết bị.
- Tick 1 ms quyết định thời điểm task trở lại Ready, nhưng timestamp lấy từ DWT nên không bị lượng tử hóa theo tick 1 ms.
- REPORT.md sẽ đối chiếu các cạnh GPIO tương ứng, min/max/jitter ở hai optimization level và disassembly thực tế để giải thích sai lệch so với prediction.
