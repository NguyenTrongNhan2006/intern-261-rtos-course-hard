# W1.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Bảng task set (từ PARAMS.md)
| Task | T (ms) | D (ms) | C (us) | Priority (RM) |
|---|---|---|---|---|
| τ0 | | | | |
| τ1 | | | | |
| τ2 | | | | |
| τ3 | | | | |

## Utilization
- Uᵢ từng task: ___
- U tổng: ___
- Giới hạn Liu–Layland (n=4): 4(2^(1/4) − 1) ≈ 0.7568. So sánh: ___

## Response Time Analysis (bắt buộc nếu U tổng > giới hạn LL, khuyến khích làm luôn cả khi U thấp để đối chiếu)
Với từng task, trình bày các vòng lặp Rⁿ⁺¹ᵢ = Cᵢ + Σⱼ<ᵢ⌈Rⁿᵢ/Tⱼ⌉Cⱼ đến khi hội tụ hoặc vượt Dᵢ.

| Task | R hội tụ (hoặc MISS) | So với D |
|---|---|---|

## Kết luận
Task set này **schedulable / KHÔNG schedulable** theo RM. Nếu không, đề xuất sửa: ___ (và chứng minh lại bằng RTA).

## Dự đoán WCRT đo được
Dự đoán WCRT thực tế mỗi task sẽ lớn hơn RTA lý thuyết bao nhiêu do overhead context switch (lấy số từ W1.2): ___
