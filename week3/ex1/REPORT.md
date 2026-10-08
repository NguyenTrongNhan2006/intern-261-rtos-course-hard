# W3.1 – REPORT

## Số đo
- Log 10 phút `tools/uart_stress.py` (đính kèm file/CSV): tổng khung gửi = ___, OK = ___, lỗi CRC = ___, mất = ___.
- Bảng CPU load (run-time stats) lấy mẫu trong lúc stress test:

| Thời điểm | CPU load (%) |
|---|---|
| | |

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- So dự đoán kích thước buffer trong PREDICTION.md với thực tế: có tràn buffer lần nào không (log lại số lần);
  nếu có, giải thích tại sao dự đoán sai.
- Nếu có khung mất/lỗi CRC: định vị bằng SEQ bị nhảy số, giải thích do ngắt IDLE trễ, DMA half/full tràn, hay
  priority của task parser quá thấp so với tốc độ gửi.
- So CPU load đo được với dự đoán; giải thích phần chênh lệch bằng overhead ISR + parser.

## Trả lời câu hỏi trade-off
Vì sao chọn kích thước DMA buffer / stream buffer như trong PREDICTION.md thay vì lớn hơn (tốn RAM) hay nhỏ hơn
(rủi ro tràn)? Chứng minh bằng chính số đo CPU load và log stress test của bạn.
