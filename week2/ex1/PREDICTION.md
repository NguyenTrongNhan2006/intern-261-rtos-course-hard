# W2.1 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán trình tự ngắt bình thường
- Số lần ngắt EV (event) cho 1 transaction write len=N byte: ___ lần.
- Thời điểm (theo DWT cycle hoặc us) dự kiến giữa START và EV5: ___.

## Dự đoán bus recovery
- Số xung SCL cần toggle để slave nhả SDA trong trường hợp xấu nhất: ___ (tối đa lý thuyết: 9).
- Dự đoán: hệ thống có phục hồi được đủ `W2E1_RECOVERY_REPEAT_COUNT` lần liên tiếp không? Vì sao: ___.
- Dự đoán thời gian (ms) từ lúc phát hiện bus-stuck tới khi transaction tiếp theo thành công: ___.

## Dự đoán timeout
- Với `W2E1_I2C_TIMEOUT_MS` cho trong `PARAMS.md`, tình huống nào sẽ KHÔNG kịp timeout trước khi watchdog (nếu có)
  can thiệp? ___.
