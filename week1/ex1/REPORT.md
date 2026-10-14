W1.1 – REPORT

## Số đo

- Tần số TIM đo được (ảnh logic analyzer đính kèm): **1000 Hz** (PulseView hiển thị **1.000000 kHz**, tương đương chu kỳ **1000 µs**; dự đoán: **999.774–1000.226 Hz**).
- Sai lệch so với dự đoán (Hz và %): **0 Hz, 0% so với giá trị danh định 1000 Hz**. Sai số nằm trong khoảng cho phép **±0.226 Hz (±226 ppm)**.
- Duty cycle TIM đo được: **50%** (xấp xỉ **500 µs mức cao + 500 µs mức thấp**).
- Chu kỳ blink đo thực tế của task A/B: **task A = 263.936 ms/lần toggle** (xấp xỉ 264 ms); **task B = 704 ms/lần toggle theo cấu hình**. Hai task dùng chung PB3 nên waveform tổng hợp không thể tách riêng mọi cạnh của task B chỉ bằng một kênh đo.
- Các khoảng giữa cạnh liên tiếp trên PB3 quan sát được gồm khoảng **263.936 ms** và khoảng xen **88 ms**. Khoảng 88 ms là khoảng cách giữa hai sự kiện trong waveform tổng hợp, không phải chu kỳ độc lập của task A hoặc task B.
-Hình sẽ được em show cho mấy anh ạ  

## Giải thích sai lệch (ĐỊNH LƯỢNG)

Giá trị dự đoán của TIM là 1000 Hz với dung sai ±226 ppm, tức **999.774–1000.226 Hz**. Logic analyzer đo được **1.000000 kHz**, tương đương **1000 Hz**, nên sai lệch so với giá trị danh định là **0 Hz (0%)** và đạt yêu cầu.

Cấu hình tạo xung là HSE 8 MHz, PLL x9 tạo clock hệ thống 72 MHz; TIM2 dùng clock timer 72 MHz với PSC = 71 và ARR = 999:

`f_TIM = 72,000,000 / ((71+1)(999+1)) = 1000 Hz`

CCR1 = 500 tạo mức cao khoảng 500 µs, vì vậy duty cycle đo được xấp xỉ 50%. Độ phân giải hiển thị của PulseView và sai số tinh thể có thể làm giá trị hiển thị lệch vài mẫu, nhưng ảnh đo cho kết quả đúng 1 kHz.

Đối với PB3, task A có lịch 264 ms và task B có lịch 704 ms. Vì cả hai cùng toggle một chân GPIO, các cạnh quan sát được là tổng hợp của hai lịch chạy; do đó không được lấy một khoảng xen kẽ như 88 ms để kết luận đó là chu kỳ của task B. Kết quả **263.936 ms ≈ 264 ms** xác nhận lịch của task A; chu kỳ 704 ms của task B được xác định từ tham số cấu hình và cần một chân tag riêng nếu muốn đo độc lập.

## Giải thích thanh ghi (tham chiếu SETUP.md)

1. **FLASH->ACR**: đặt prefetch và **2 wait-state** trước khi chuyển CPU lên 72 MHz. Nếu thiếu wait-state, CPU có thể đọc Flash không ổn định, dẫn tới treo hoặc chạy sai.
2. **RCC->CR / RCC->CFGR**: bật HSE 8 MHz, chọn PLL nguồn HSE với hệ số x9, đặt prescaler AHB/APB phù hợp rồi chọn PLL làm SYSCLK để đạt 72 MHz.
3. **TIM2->PSC / ARR / CCR1**: PSC = 71 chia clock 72 MHz xuống 1 MHz; ARR = 999 tạo chu kỳ 1000 µs (1 kHz); CCR1 = 500 tạo xung mức cao khoảng 500 µs.
4. **GPIOA->CRL**: cấu hình chân PA0 ở chế độ alternate-function output để đưa tín hiệu TIM2_CH1 ra chân đo.
5. **GPIOB->CRL và ODR/BSRR/BRR**: cấu hình PB3 làm GPIO output và đổi trạng thái bằng thanh ghi set/reset nguyên tử; PB3 là chân tag dùng chung cho hai task.
6. **SCB->SHPR3**: đặt PendSV ở mức ưu tiên thấp nhất để chuyển context chỉ xảy ra sau các exception/interrupt quan trọng hơn; SysTick vẫn đảm nhiệm nhịp tick của FreeRTOS.

## Trả lời câu hỏi trade-off

PendSV phải có priority thấp nhất vì nó thực hiện context switch, công việc có thể trì hoãn. Nếu PendSV được ưu tiên cao, nó có thể chen vào ISR quan trọng, làm tăng độ trễ và phá vỡ thứ tự xử lý interrupt. Trong bài đo này, OpenOCD xác nhận Cortex-M3 được nhận diện và chương trình được nạp/verify thành công; các lần đọc `uxTickCount`, `uxSchedulerRunning` và `uxCurrentNumberOfTasks` cho thấy scheduler đã chạy. Vì vậy PendSV cần đứng sau các exception/interrupt khác để scheduler chuyển task an toàn, còn tín hiệu TIM 1 kHz vẫn giữ đúng nhịp độc lập với việc chuyển context.
