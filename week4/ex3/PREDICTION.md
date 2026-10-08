# W4.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán WCRT/CPU/stack (chạy ổn định, chưa fault)
- Dự đoán task nào lệch WCRT đo được so với RTA ở W4.1 nhiều nhất, trong ngưỡng `W4E3_WCRT_TOLERANCE_PERCENT`: ___.
- Dự đoán CPU load tổng đo được: ___ %.

## Dự đoán từng kịch bản fault (xem `W4E3_ASSIGNED_FAULT_NAMES` trong PARAMS.md)
Với mỗi kịch bản được giao, dự đoán TRƯỚC khi chạy:
- Kịch bản 1 (___): control loop có trễ deadline không: ___, nếu có đây có phải suy giảm đã công bố ở W4.1
  không: ___.
- Kịch bản 2 (___): tương tự.
- Kịch bản 3 (___): tương tự.
- (Nếu được giao 4 kịch bản) Kịch bản 4 (___): tương tự.

## Rủi ro đã lường trước
- Nếu watchdog không phục hồi đúng `W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS`: dự đoán hệ quả quan sát được: ___.
