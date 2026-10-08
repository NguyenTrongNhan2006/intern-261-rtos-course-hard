# W3.2 – Tickless idle + STOP mode

## Mục tiêu
Bật `configUSE_TICKLESS_IDLE=2`, tự viết `portSUPPRESS_TICKS_AND_SLEEP` dùng RTC alarm để đưa MCU vào STOP mode
khi idle, đánh thức đúng lúc và bù tick chính xác. Trôi giờ so với PC phải nằm trong ngưỡng được giao.

## Đọc trước
- freertos.org *Low Power Support* (tickless idle, `portSUPPRESS_TICKS_AND_SLEEP`, `vTaskStepTick`,
  `eTaskConfirmSleepModeStatus`).
- RM0008 chương PWR (STOP mode, wake-up), RTC (alarm, prescaler), EXTI (line kết nối RTC alarm).

## Việc phải làm
1. Cấu hình RTC chạy từ LSE (hoặc LSI nếu không có thạch anh 32.768 kHz, ghi rõ lựa chọn + sai số oscillator
   trong REPORT.md), tính prescaler để 1 tick RTC tương ứng đúng 1 tick FreeRTOS (`configTICK_RATE_HZ`).
2. Hiện thực `portSUPPRESS_TICKS_AND_SLEEP(xExpectedIdleTime)`: kiểm `eTaskConfirmSleepModeStatus()`, tắt
   SysTick, đặt RTC alarm sau đúng `xExpectedIdleTime` tick (trừ hao thời gian entry/exit), vào STOP mode (WFI +
   PWR_CR), sau khi thức dậy tính số tick RTC thực sự đã trôi và gọi `vTaskStepTick()` bù lại.
3. Khôi phục clock về 72 MHz (HSE+PLL) ngay sau khi thoát STOP (STOP mode tắt PLL, MCU chạy lại bằng HSI).
4. Test trôi giờ: so thời điểm MCU (in ra UART mỗi phút, ví dụ) với giờ PC trong 10 phút liên tục, hệ thống xen
   kẽ idle (vào STOP) và active (1 task busy-wait định kỳ).
5. Đo dòng tiêu thụ (current meter) ở trạng thái idle (STOP mode) và active, ghi vào bảng.

## Sản phẩm nộp
- Code tickless idle + RTC wake + khôi phục clock.
- Bảng dòng tiêu thụ idle vs active.
- Bảng trôi tick đo trong `W3E2_DRIFT_TEST_WINDOW_MIN` phút + ảnh đo thời điểm thức dậy (logic analyzer trên
  chân toggle lúc vào/ra STOP).

## Tiêu chí đạt
Trôi tick < `W3E2_DRIFT_BOUND_PPM` (xem PARAMS.md) trong cửa sổ đo; giải thích được nguồn trôi (dung sai
oscillator RTC, làm tròn khi bù tick, thời gian entry/exit STOP).

## Giờ dự kiến
5 giờ.
