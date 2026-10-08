# W2.2 – REPORT

## Số đo
- Log stress test `W2E2_STRESS_COUNT` phần tử: số lỗi mất/lặp phát hiện = ___ (phải bằng 0 để đạt).
- Bảng cycle:

| Cơ chế | Cycle trung bình (push/send) | Cycle trung bình (pop/receive) |
|---|---|---|
| SPSC ring buffer tự viết | | |
| `xQueueSendFromISR` / `xQueueReceive` | | |

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- So dự đoán cycle trong PREDICTION.md với số đo; giải thích chênh lệch bằng những gì queue gốc làm thêm (copy
  struct, critical section `taskENTER_CRITICAL`, kiểm tra task đang chờ để đánh thức) mà ring buffer của bạn không
  làm.
- Nếu có lỗi mất/lặp trong stress test: định vị bằng chính số `seq` sai lệch đầu tiên, giải thích do thiếu
  `__DMB()`/`__DSB()` ở đâu.

## Trả lời câu hỏi trade-off
Lập luận vì sao SPSC (1 producer ISR, 1 consumer task) trên Cortex-M3 đơn nhân an toàn không cần lock. Chỉ ra 1
tình huống cụ thể (nêu rõ: thêm producer thứ 2? phần tử không còn là 1 word atomic? cần SMP?) khi lập luận đó
không còn đúng, và chứng minh bằng số đo hoặc lập luận kỹ thuật của chính bạn.
