# W2.3 – REPORT

## Số đo
- Gantt (tracer W1.2) – hiện tượng 1: unbounded inversion.
- Gantt (tracer W1.2) – hiện tượng 2: chained blocking (có inheritance).
- Gantt (tracer W1.2) – hiện tượng 3: deadlock (đánh dấu rõ điểm cả 2 task dừng vĩnh viễn).
- Gantt (tracer W1.2) – sau khi áp dụng Immediate Priority Ceiling.
- Bảng thời gian block của τ_H:

| Cơ chế | Số lần bị block nối tiếp | Tổng thời gian block (ms) |
|---|---|---|
| Binary semaphore (không inheritance) | | |
| Mutex (inheritance) | | |
| Priority Ceiling (wrapper) | | |

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- So dự đoán trong PREDICTION.md với số đo thực; giải thích chênh lệch bằng jitter context-switch đo ở W1.2 của
  chính bạn (không nói chung chung "do overhead hệ thống").
- Chỉ rõ trên Gantt điểm chính xác nào là chained blocking (khác với inversion đơn thuần): τ_H bị chặn bởi quá
  nhiều hơn 1 task cấp thấp hơn liên tiếp dù có inheritance.

## Trả lời câu hỏi trade-off
Giải thích vì sao Immediate Priority Ceiling Protocol về mặt lý thuyết LOẠI BỎ được deadlock do thứ tự lock (không
chỉ giảm thời gian chờ), dựa trên đúng cơ chế ceiling bạn hiện thực (không task nào có thể lock thành công một tài
nguyên nếu đang giữ tài nguyên khác có ceiling cao hơn). Chứng minh bằng Gantt sau cùng của bạn rằng deadlock
không còn xảy ra với cùng kịch bản thứ tự lock đã gây deadlock ở hiện tượng 3.
