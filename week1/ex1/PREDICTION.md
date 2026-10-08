# W1.1 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán tần số
- Flash latency dự kiến ở 72 MHz (theo bảng wait-state RM0008 §4): ___ wait-state.
- Tần số xung TIM đo được dự đoán: 1000 Hz ± `W1E1_CLOCK_TOLERANCE_PPM` (xem PARAMS.md) = ___ Hz đến ___ Hz.

## Dự đoán hành vi blinky
- Chu kỳ blink task A / B dự kiến (xem PARAMS.md): ___ ms / ___ ms.
- Ảnh hưởng của priority 2 task tới thứ tự chạy khi cùng ready: ___

## Rủi ro đã lường trước
- Nếu quên set flash latency trước khi tăng PLL: dự đoán hệ quả ___
- Nếu PendSV priority không thấp nhất: dự đoán hệ quả ___
