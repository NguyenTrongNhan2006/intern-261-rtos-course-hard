# W1.1 – REPORT

## Số đo

- Tần số TIM đo được (ảnh logic analyzer đính kèm): chưa đo. Dự đoán: 1000 Hz, khoảng cho phép 999.774–1000.226 Hz theo PREDICTION.md.
- Sai lệch so với dự đoán (Hz và %): chưa xác định vì chưa có số đo thực tế.
- Chu kỳ blink đo thực tế của task A/B: chưa đo / chưa đo. Khoảng toggle cấu hình trong code là 264 ms / 704 ms.

Hai task cùng toggle PB3 nên waveform tại chân này là kết quả
kết hợp của cả hai task, không phải hai waveform độc lập.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

Chưa có logic analyzer nên chưa đủ dữ liệu để đánh giá
sai lệch tần số hoặc kết luận đạt yêu cầu ±226 ppm.

Theo cấu hình TIM2:

f_PWM = 72000000 / ((71 + 1) × (999 + 1)) = 1000 Hz.

Sai số cho phép:

1000 × 226 / 1000000 = 0.226 Hz, tương ứng 0.0226%.

Sau khi có số đo f_measured, tính:

- Sai lệch có dấu: Delta_f = f_measured - 1000 Hz.
- Sai lệch phần trăm: Delta_f / 1000 × 100%.
- Sai lệch ppm: Delta_f / 1000 × 1000000.

Các nguồn sai lệch cần xem xét gồm sai số HSE crystal,
timebase accuracy và sampling resolution của logic analyzer.
PLL nhân 9 là hệ số nguyên trong cấu hình này, không có
sai số do làm tròn hệ số PLL.

Trong lần kiểm tra scheduler trước khi bổ sung TIM2,
xTickCount tăng 1013 tick trong khoảng chờ một giây bằng
OpenOCD. Kết quả xác nhận tick đang tăng, nhưng không dùng
để đánh giá clock theo ppm vì có thời gian xử lý lệnh/debug.

## Giải thích thanh ghi (tham chiếu SETUP.md)

- FLASH->ACR: đặt LATENCY = 2 và bật prefetch trước khi
  chuyển SYSCLK lên 72 MHz, đáp ứng thời gian truy cập FLASH.

- RCC->CFGR: chọn nguồn PLL từ HSE 8 MHz, nhân 9; AHB chia 1,
  APB1 chia 2 và APB2 chia 1. Clock danh định tương ứng:
  SYSCLK/HCLK = 72 MHz, PCLK1 = 36 MHz, PCLK2 = 72 MHz.

- TIM2->CCMR1: đặt CC1S = 00, OC1M = 110 và OC1PE = 1,
  chọn Channel 1 làm output PWM mode 1 và bật CCR1 preload.
  Kết hợp PSC = 71, ARR = 999, CCR1 = 500 để tạo PWM
  danh định 1 kHz, duty cycle 50% trên PA0.

Chi tiết các thanh ghi còn lại được trình bày trong SETUP.md.

## Trả lời câu hỏi trade-off

PendSV thực hiện context switch giữa các task. Đặt PendSV
ở priority thấp nhất giúp context switch được trì hoãn
đến khi các interrupt có priority cao hơn hoàn tất,
phù hợp với cơ chế lưu và khôi phục context của FreeRTOS port.

Trade-off là task vừa được đánh thức có thể phải chờ ISR
hoàn tất trước khi chạy. Vì vậy, ISR kéo dài sẽ làm tăng
độ trễ đáp ứng của task.

Bằng chứng hiện có:

- ELF có SVC_Handler, PendSV_Handler và SysTick_Handler
  được liên kết với handler của FreeRTOS port.
- Kiểm tra trên board cho thấy uxCurrentNumberOfTasks = 3,
  xSchedulerRunning = 1 và xTickCount tăng.

Những bằng chứng này xác nhận kernel đã hoạt động,
chưa chứng minh trực tiếp thứ tự xử lý exception.
Chưa có traceback hoặc phép đo minh họa PendSV chờ ISR;
cần bổ sung bằng chứng này khi thực hiện kiểm thử.
