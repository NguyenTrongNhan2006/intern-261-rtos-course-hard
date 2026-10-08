# W1.4 – REPORT

## Log phân mảnh heap_4
Đính kèm bảng/đồ thị `free_bytes_now` và `free_bytes_min_ever` theo từng bước, `W1E4_REPEAT_CYCLES` vòng.

## Giải thích phân mảnh (ĐỊNH LƯỢNG)
So khớp log thực tế với dự đoán trong PREDICTION.md; nếu lệch, giải thích bằng cơ chế coalescing thực của heap_4
(block liền kề theo địa chỉ gộp lại, không theo thời gian cấp phát).

## Bảng RAM tĩnh vs .map

| Section/đối tượng | Tính tay (bytes) | Từ .map (bytes) | Lệch |
|---|---|---|---|
| .data | | | |
| .bss | | | |
| Idle task stack | | | |
| Timer task stack | | | |
| ... | | | |
| **Tổng** | | | |

Lệch tổng so với `W1E4_RAM_BUDGET_TOLERANCE_BYTES` (PARAMS.md): đạt/không đạt.

## Trả lời câu hỏi trade-off
Vì sao chuyển sang 100% cấp phát tĩnh loại bỏ hẳn rủi ro phân mảnh long-run, đổi lại cái giá nào (tính linh hoạt, RAM
tối đa phải cấp trước)? Trả lời bằng số đo của chính bạn.
