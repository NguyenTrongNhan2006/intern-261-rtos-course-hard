# W1.1 – REPORT

**Sinh viên:** Nguyễn Trọng Nhân  
**MSSV:** 2452882

## 1. Cấu hình và số đo

- HSE 8 MHz, PLL ×9 tạo SYSCLK 72 MHz.
- APB1 = 36 MHz; clock TIM2 = 72 MHz.
- TIM2: PSC = 71, ARR = 999, CCR1 = 500.
- PA0 xuất PWM 1 kHz, duty 50%.
- Task A/B cùng toggle PB3, chu kỳ danh định 264/704 ms.

Kết quả đã ghi nhận trước khi sửa code:

| Nội dung | Kết quả |
| --- | --- |
| Flash bằng OpenOCD | Verified OK |
| TIM2 trên PulseView | Hiển thị 1.000000 kHz, duty khoảng 50% |
| Khoảng cạnh PB3 | Có khoảng 263.936 ms và 88 ms |
| PendSV / SysTick | Đọc được 0xF0 / 0xF0 |

## 2. Phân tích định lượng

Tần số PWM danh định:

`f = 72,000,000 / ((71+1) × (999+1)) = 1000 Hz`

Duty cycle:

`D = 500 / (999+1) = 50%`

Dung sai ±226 ppm tương ứng khoảng 999.774–1000.226 Hz.
Giá trị hiển thị 1.000000 kHz chưa đủ chứng minh sai số bằng
0 ppm. Cần bổ sung sample rate, đo trung bình nhiều chu kỳ
và xét độ chính xác timebase của analyzer.

Khoảng 263.936 ms lệch so với 264 ms:

`ΔT = −64 µs`, tương đương khoảng `−242.4 ppm`.

Đây là sai lệch một khoảng cạnh, chưa đủ kết luận lỗi HSE
hoặc analyzer vì còn ảnh hưởng việc chọn cạnh và độ trễ lập lịch.

Hai task dùng chung PB3 nên waveform là tổng hợp. Khoảng 88 ms
không phải chu kỳ task B. Hai lịch trùng nhau mỗi 2112 ms;
hai lần toggle gần nhau có thể tạo xung hẹp.

## 3. Giải thích thiết kế

- Dùng `xTaskDelayUntil()` để giữ mốc đánh thức định kỳ,
  tránh tích lũy dịch chuyển do thời điểm gọi delay thay đổi.
  Cạnh GPIO vẫn có jitter do độ trễ lập lịch.
- Đặt Flash 2 wait-state trước khi chuyển lên 72 MHz.
- Bật clock GPIO trước khi cấu hình CRL/CRH.
- Tắt JTAG, giữ SWD để sử dụng PB3.
- Bảo vệ chuỗi đọc ODR và ghi BSRR/BRR bằng critical section.
- Bật `vApplicationMallocFailedHook()` để dừng debug khi
  cấp phát heap thất bại.

## 4. PendSV và kiểm chứng

FreeRTOS port đặt priority PendSV/SysTick khi scheduler chạy.
Hàm cấu hình NVIC của em chỉ đặt priority grouping.

Byte 0xF0 tương ứng priority 15, thấp nhất trên STM32F103.
PendSV ở mức này để chuyển context chờ các ISR ưu tiên cao hơn.

Sau khi sửa code, cần build, flash và đo lại; chạy
`check_api.sh platform week1/ex1`, lưu log PASS và ảnh đo.
Chưa kết luận phiên bản mới đã đạt toàn bộ yêu cầu.
