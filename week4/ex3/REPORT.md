# W4.3 – REPORT

## Bảng dự đoán RTA vs đo (chạy ổn định)

| Task | WCRT dự đoán (W4.1, us) | WCRT đo được (us) | Sai số (%) | Trong ngưỡng? |
|---|---|---|---|---|
| control | | | | |
| imu | | | | |
| telemetry | | | | |
| shell | | | | |
| idle_aux | | | | |

CPU load đo được: ___ %. Stack high-water-mark thấp nhất còn lại (tất cả task): ___ words.

## Log từng kịch bản fault
Với mỗi kịch bản trong `W4E3_ASSIGNED_FAULT_NAMES`: thời điểm kích hoạt, hành vi quan sát, control loop có trễ
deadline không (số ms/us trễ nếu có), có phải suy giảm đã công bố ở W4.1 không, watchdog có phục hồi không và
lý do reset ghi được (nếu có reset).

## Giải thích sai lệch (ĐỊNH LƯỢNG)
So PREDICTION.md với bảng/log thực tế; định lượng mọi chỗ lệch và nguồn gốc.

## Trả lời câu hỏi trade-off
Với kịch bản gây suy giảm NHIỀU NHẤT trong số các kịch bản được giao: giải thích bằng chính số đo của bạn vì
sao thiết kế suy giảm ở W4.1 là hợp lý (hoặc không, và nếu không thì đề xuất sửa gì).
