# W4.1 – PREDICTION (điền TRƯỚC khi nộp design doc cho mentor duyệt)

## Dự đoán RTA
- Bảng T/D/C/R cho 5 vai trò (xem `include/mixed_criticality_design.h`), điền bằng tay TRƯỚC khi mentor duyệt:

| Role | T (us) | D (us) | C (us) | R (us) dự đoán | Priority |
|---|---|---|---|---|---|
| control | | | | | |
| imu | | | | | |
| telemetry | | | | | |
| shell | | | | | |
| idle_aux | | | | | |

- Task nào có R_i gần D_i nhất (margin nhỏ nhất)? ___, margin dự đoán: ___ us.

## Dự đoán ngân sách
- Tổng RAM tĩnh dự đoán (bytes), so với `W4E1_RAM_BUDGET_BYTES`: ___ / ___ bytes.
- Tổng CPU utilization dự đoán (%), so với `W4E1_CPU_BUDGET_PERCENT_MAX`: ___ / ___ %.

## Rủi ro đã lường trước
- Nếu I2C1 bus-recovery (W2.1) xảy ra đúng lúc control loop tới hạn: dự đoán ảnh hưởng tới WCRT của `control`: ___.
- Nếu mentor không duyệt thiết kế lần đầu: điểm nào trong bảng trên bạn nghĩ sẽ bị yêu cầu sửa, vì sao: ___.
