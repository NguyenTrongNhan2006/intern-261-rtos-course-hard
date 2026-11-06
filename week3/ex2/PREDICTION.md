# W3.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Tham số và chu kỳ đo

- `W3E2_DRIFT_TEST_WINDOW_MIN = 10` và `W3E2_DRIFT_BOUND_PPM = 390` theo `include/params.h` và `PARAMS.md`. ppm (parts per million) là phần triệu. `window_ms = 10 * 60 * 1000 = 600000 ms` (600 giây). `max_drift_ms = 600000 * 390 / 1000000.0 = 234.0 ms`. Theo `README.md`, điều kiện đạt là `abs(drift_ms) < 234.0`.
- `configTICK_RATE_HZ = 1000` là cấu hình dự kiến cho W3.2, cùng mức với W3.1. `tick_ms = 1000.0 / 1000 = 1.0 ms`. `configUSE_TICKLESS_IDLE = 2` chọn cơ chế ngủ không nhịp do ứng dụng cung cấp. Thư mục `include/` của W3.2 hiện có `params.h` và `tickless_rtc.h`.
- `README.md` yêu cầu luân phiên Active và Idle STOP với một tác vụ bận định kỳ. Mô hình dự báo dùng chu kỳ 1.000 ms, bận 20 ms (2%) hoặc 50 ms (5%), nhường 980 ms hoặc 950 ms cho nhánh idle, gồm cả thời gian chuyển trạng thái. Chu kỳ này là giả định tính toán. Lấy mốc tick đầu/cuối và log mỗi phút qua cổng nối tiếp để đối chiếu thời gian đã trôi trên máy tính (PC).

## Nguồn xung và hệ số chia RTC

`RTC_PRL` (Real-Time Clock Prescaler Load Register, giá trị nạp bộ chia của đồng hồ thời gian thực) lưu giá trị `prl` 20 bit. `divisor = prl + 1` và `f_cnt_hz = f_rtc_input_hz / (double)divisor` là tần số tăng của `RTC_CNT` (Real-Time Clock Counter Register, bộ đếm 32 bit).

| Nguồn | Hệ số chia và cách quy đổi |
|---|---|
| LSI (Low-Speed Internal RC oscillator, bộ dao động điện trở–tụ điện nội tốc độ thấp), danh định 40 kHz | `prl = 40000 / 1000 - 1 = 39`. Một bước đếm bằng 1 ms ở đúng 40 kHz. |
| LSI đã hiệu chuẩn, phương án chọn cho Blue Pill thiếu thạch anh 32,768 kHz | Dùng `f_cal_mhz` (tần số LSI đo được, đơn vị milli-hertz) để chọn `prl = f_cal_mhz / 1000000U - 1U`, với phép chia nguyên. Ví dụ `f_cal_mhz = 40320000` cho `prl = 39` và `f_cnt_hz = 1008`. Tỷ lệ đúng là `1000.0 * 40 / 40320` tick FreeRTOS mỗi bước đếm. |
| LSE (Low-Speed External oscillator, thạch anh ngoại tốc độ thấp), 32,768 kHz | `prl = 31` cho `f_cnt_hz = 32768 / 32 = 1024 Hz`. Quy đổi `1000.0 / 1024` tick FreeRTOS mỗi bước đếm. Nếu dùng `prl = 0`, bộ đếm chạy theo xung nguồn và tỷ lệ là `1000.0 / 32768`. |

Phương án hiệu chuẩn khi khởi động dùng HSE (High-Speed External oscillator, thạch anh ngoại tốc độ cao) 8 MHz và PLL (Phase-Locked Loop, vòng khóa pha) nhân 9 lên 72 MHz làm chuẩn. Với bộ chia tạm `divisor_cal = 40`, đo `delta_core_cycles` giữa hai cạnh tăng cách nhau `n_counts` bước của `RTC_CNT`, ví dụ `n_counts = 1024`, bằng bộ đếm chu kỳ lõi. Tính `f_cal_mhz = (72000000ULL * 1000 * n_counts * divisor_cal + delta_core_cycles / 2) / delta_core_cycles`. Phép đo bám theo độ chính xác của thạch anh 8 MHz và điều kiện nhiệt, áp tại lúc hiệu chuẩn.

Lấy ảnh chụp nhất quán bộ đếm và `RTC_DIV` (Real-Time Clock Prescaler Divider Register, giá trị bộ chia đang đếm xuống) khi dừng và khi khôi phục SysTick (System Tick Timer, bộ định thời tick hệ thống). Dùng `phase = prl - rtc_div` để tính phần đã đi qua của mỗi bước đếm. Hiệu hai mốc cho số xung nguồn đã trôi `delta_input = (int64_t)(uint32_t)(cnt_after - cnt_before) * divisor + (int64_t)phase_after - phase_before`. Phép trừ bộ đếm 32 bit xử lý lần tràn khi khoảng đo nhỏ hơn một vòng. Với mẫu hợp lệ và `f_src_mhz` là tần số nguồn đã chọn tính theo milli-hertz:

```c
accum = (uint64_t)delta_input * 1000000ULL + subtick_rem;
whole_ticks = accum / f_src_mhz;
subtick_rem = accum % f_src_mhz;
```

`subtick_rem` giữ phần lẻ dưới một tick cho lần sau. Giữ cả pha của SysTick và khôi phục phần thời gian còn lại đến tick tiếp theo. `vTaskStepTick()` (hàm cộng bù tick của nhân FreeRTOS) nhận số tick nguyên đã trôi trong giới hạn đến tác vụ kế tiếp. Cơ chế này xử lý cả lần thức sớm và tránh mất phần lẻ ở mỗi lần ngủ.

## Độ trễ vào và ra STOP

| Pha | Thành phần dự báo | Thời gian |
|---|---|---:|
| Vào STOP | Chờ `RTOFF` (RTC Operation Off, cờ báo hoàn tất ghi của RTC) quanh thao tác nạp `RTC_ALR` (Real-Time Clock Alarm Register, thanh ghi báo thức). Sau khi thoát cấu hình, RM0008 quy định quá trình ghi cần ít nhất 3 chu kỳ nguồn RTC. `sync_us = 3 * 1000000.0 / f_rtc_input_hz` cho 75,0 µs ở 40 kHz hoặc 91,6 µs ở 32,768 kHz. Cộng thao tác PWR (Power Control, khối điều khiển nguồn) và SCB (System Control Block, khối điều khiển hệ thống). | ~0,1 ms |
| Ra STOP đến lệnh đầu tiên | `LPDS = 1` (Low-Power Deepsleep, bộ ổn áp nội ở chế độ công suất thấp). Giá trị điển hình từ sự kiện đánh thức đến lệnh đầu tiên, khi hệ thống trở lại bằng HSI (High-Speed Internal RC oscillator, bộ dao động nội tốc độ cao). | ~5,4 µs |
| Khôi phục 72 MHz | Khởi động HSE 8 MHz mất 1,5–2,0 ms theo giả định trên board. DS5319 cho 2 ms điển hình và thời gian thay đổi theo thạch anh. Dự trù thêm 0,2 ms cho khóa PLL theo bảng 27. | ~1,7–2,2 ms |
| Tổng | `entry_ms + wake_ms + hse_ms + pll_ms = 0.1 + 0.0054 + hse_ms + 0.2`. | ~1,81–2,31 ms |

Riêng pha thoát và khôi phục xung khoảng 1,71–2,21 ms, tương đương khoảng 2 tick. Dự trù ban đầu 3 tick nguyên cho cả vào và ra (`ceil(2.3054 / 1.0) = 3`). Đặt mốc báo thức sớm hơn hạn tác vụ theo khoản dự trù và đổi thời gian còn lại sang bước đếm RTC bằng cách làm tròn xuống. Bỏ qua STOP nếu khoảng rảnh quá ngắn để nạp báo thức an toàn. Dùng mốc đo thực tế qua đường 17 của EXTI (External Interrupt/Event Controller, bộ điều khiển ngắt/sự kiện ngoài) và tín hiệu trên chân đo để kiểm tra khoản dự trù, kể cả đồng bộ lại RTC sau STOP. Khi bù tick, đo toàn khoảng SysTick bị dừng, gồm cả thời gian thiết lập và khôi phục xung.

## Dự đoán trôi tick sau 600 giây

`abs_drift_ms = 600000 * abs_error_ppm / 1000000.0`. Bảng quy sai số tần số về toàn cửa sổ 600 giây. Khi đo luân phiên, đóng góp thực tế của mỗi nguồn được tính theo thời gian nhánh đó giữ nhịp.

| Kịch bản | Sai số dùng để dự báo | Độ trôi tuyệt đối | So với 234,0 ms |
|---|---:|---:|---|
| LSI danh định, `prl = 39`, chưa hiệu chuẩn | ±1% đến ±5% (10.000–50.000 ppm), giả định cho mẫu gần 40 kHz | 6.000–30.000 ms | FAIL dự báo |
| LSI hiệu chuẩn theo HSE, cộng dồn phần dư và đo đủ thời gian dừng SysTick | 100–300 ppm sai số hiệu dụng giả định trong điều kiện nhiệt và áp ổn định | 60–180 ms | PASS dự báo, cần đo đủ 10 phút |
| LSE 32,768 kHz, quy đổi theo tỷ lệ và giữ phần dư | ±20–50 ppm theo giả định về thạch anh và tải phù hợp | 12–30 ms | PASS dự báo, cần xác nhận trên board |

DS5319 bảng 25 ghi LSI 30/40/60 kHz (min/điển hình/max), tương ứng `-250000` đến `+500000` ppm so với 40 kHz. Ở hai biên đó, độ lệch theo mô hình toàn cửa sổ là `-150000` hoặc `+300000` ms. Khoảng ±1–5% chỉ là kịch bản gần danh định. Hiệu chuẩn đầu kỳ xử lý sai lệch tĩnh. Độ trôi LSI do nhiệt và điện áp sau hiệu chuẩn, sai số chuẩn HSE và thời gian mất ở các mốc chuyển đều cần đối chiếu khi đo.

Ở mức dự báo 300 ppm, phần dư so với ngưỡng là `234.0 - 180.0 = 54.0 ms`. Với 600 chu kỳ và một lần STOP mỗi chu kỳ, phần sai số tích lũy chưa đưa vào dự báo phải dưới `54.0 / 600 = 0.09 ms` mỗi lần nếu cùng dấu và dùng hết phần dư cho nguồn này. Ví dụ, bỏ sót 2 ms ở mỗi lần sẽ cộng `600 * 2 = 1200 ms`. Giữ phần dư quy đổi giúp tránh mất đến gần 1 tick mỗi lần lấy phần nguyên. Sai số bắt mốc và độ trễ truyền log vẫn cần tách khi đối chiếu với thời gian trên máy tính.

## Dự đoán dòng tiêu thụ

Dự báo cho riêng STM32F103 ở khoảng 3,3 V và 25 °C. `I_avg_mA = (I_active_mA * t_active_ms + I_stop_mA * t_stop_ms) / period_ms` trong mô hình hai trạng thái, với `t_active_ms + t_stop_ms = period_ms`.

| Trạng thái | Dòng dự báo | Điều kiện hoặc phép tính |
|---|---:|---|
| Active | ~27–36 mA | DS5319 bảng 17 tại 72 MHz, nguồn ngoài 8 MHz và PLL. Hai giá trị điển hình ứng với tắt/bật xung ngoại vi trong bài đo của ST. |
| Idle STOP | ~14–24 µA (0,014–0,024 mA) | Dải tham chiếu từ DS5319 bảng 16. `SLEEPDEEP = 1` (Sleep Deep, chọn ngủ sâu), `PDDS = 0` (Power Down Deepsleep, chọn STOP) và `LPDS = 1` (Low-Power Deepsleep, ổn áp công suất thấp). Mốc 14 µA ứng với ổn áp công suất thấp, mốc 24 µA ứng với ổn áp chạy thường. Cả hai mốc đo khi LSI tắt. Bảng 25 cho riêng LSI khoảng 0,65 µA điển hình khi bật. |
| Trung bình chu kỳ 20/980 ms, chưa tách phần chuyển trạng thái | ~0,554–0,744 mA | `(27 * 20 + 0.014 * 980) / 1000` đến `(36 * 20 + 0.024 * 980) / 1000`. Với 30 mA và 20 µA, `I_avg_mA = (30 * 20 + 0.020 * 980) / 1000 = 0.6196 mA`. |
| Trung bình chu kỳ 50/950 ms, cùng mô hình | ~1,363–1,823 mA | `(27 * 50 + 0.014 * 950) / 1000` đến `(36 * 50 + 0.024 * 950) / 1000`. |

Tỉ lệ giảm dòng tức thời theo giá trị đại diện là `30.0 / 0.020 = 1500` lần. Nếu 20 ms chỉ tính thời gian chạy tác vụ và tạm lấy 2 ms chuyển trạng thái ở 30 mA, ví dụ trung bình thành `(30 * 22 + 0.020 * 978) / 1000 = 0.67956 mA`. Khi đo cả board, dòng qua đèn báo nguồn, bộ ổn áp trên board và thiết bị nối ngoài có thể chi phối số đo lúc STOP.

Đối chiếu với [ST RM0008, mục 5.3.4 và chương 18](https://www.st.com/resource/en/reference_manual/cd00171190.pdf) · [ST DS5319, bảng 16, 17, 22 và 25–27](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf).
