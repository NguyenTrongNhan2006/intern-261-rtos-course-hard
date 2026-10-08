# W1.3 – Phân tích lập lịch (RTA)

## Mục tiêu
Tính tay utilization, giới hạn Liu–Layland, Response Time Analysis cho 1 task set riêng của bạn, rồi đo WCRT thực và so sánh.

## Đọc trước
- Liu & Layland (1973) – Rate Monotonic, điều kiện đủ U ≤ n(2^(1/n) − 1)
- Response Time Analysis (Joseph & Pandya, 1986) – công thức lặp Rⁿ⁺¹ᵢ = Cᵢ + Σⱼ<ᵢ ⌈Rⁿᵢ/Tⱼ⌉Cⱼ
- Mastering chương 4 (priority, scheduling)

## Việc phải làm
1. Chạy `python3 tools/gen_params.py <MSSV> --only w1e3` → lấy 4 task (T, D, C busy-wait) trong `include/params.h`/`PARAMS.md`.
2. Tính tay (nộp trong PREDICTION.md): utilization Uᵢ từng task, U tổng, so với giới hạn Liu–Layland cho n=4.
3. Nếu U tổng > giới hạn LL: **không được kết luận vội** – chạy RTA chính xác (công thức lặp) để kết luận chắc chắn
   schedulable/không (LL chỉ là điều kiện đủ, không phải điều kiện cần).
4. Nếu kết luận KHÔNG schedulable: đề xuất và chứng minh lại (bằng RTA) một cách sửa (đổi T, giảm C, đổi priority...).
5. Hiện thực 4 task bằng `xTaskCreate` + `xTaskDelayUntil`, priority giảm dần theo thứ tự T tăng dần (Rate Monotonic).
   `C` hiện thực bằng busy-wait hiệu chỉnh qua `DWT->CYCCNT` (quy đổi `W1E3_BUSY_WAIT_US` sang số chu kỳ ở 72 MHz).
6. Dùng tracer từ W1.2 đo WCRT thực tế của từng task qua nhiều hyperperiod.
7. So sánh WCRT tính tay (RTA) với WCRT đo được; giải thích sai lệch bằng số đo context switch của W1.2.

## Sản phẩm nộp
- Bảng tính tay (utilization, LL bound, RTA từng vòng lặp) trong PREDICTION.md.
- Bảng WCRT đo được (từ tracer) trong REPORT.md.
- Nếu task set không schedulable: đề xuất sửa + chứng minh lại bằng RTA.

## Tiêu chí đạt
Sai lệch dự đoán–đo được giải thích định lượng bằng chính số đo context switch ở W1.2.

## Giờ dự kiến
5 giờ.
