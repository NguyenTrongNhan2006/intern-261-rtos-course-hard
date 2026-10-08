# W3.4 – PREDICTION (điền TRƯỚC khi chạy GDB, commit trước)

Trước khi kết nối GDB lần đầu, chỉ dựa vào `nm w3e4.elf`/`readelf -s w3e4.elf` (symbol table, KHÔNG chạy board):

- Danh sách symbol liên quan kernel bạn tìm thấy (tên + địa chỉ): ___.
- Dự đoán ban đầu (trước khi xem trạng thái RAM) về khả năng cao nhất trong 3 loại lỗi (deadlock / stack
  overflow / sai priority), chỉ dựa trên số lượng task/mutex/ISR thấy được qua symbol: ___, vì sao: ___.
- Dự đoán số lệnh GDB bạn sẽ cần để xác nhận chắc chắn (so với ngưỡng tối thiểu
  `W3E4_MIN_GDB_EVIDENCE_COMMANDS` trong PARAMS.md): ___.
