# W1.1 – SETUP

## 1. Phần cứng và công cụ

- MCU: STM32F103C8T6, Cortex-M3.
- FLASH: 64 KiB; SRAM: 20 KiB.
- Clock đầu vào: HSE 8 MHz, theo cấu hình board.
- Debug/programming: ST-Link qua SWD.
- Toolchain: Arm GNU Toolchain, CMake và Ninja.
- FreeRTOS-Kernel: V11.1.0, port GCC/ARM_CM3.
- CMSIS dùng để khai báo thanh ghi và Cortex-M intrinsics.
- GPIO, RCC và TIM được cấu hình trực tiếp bằng thanh ghi.

Các chân sử dụng:

- PA13/PA14: SWDIO/SWCLK.
- PB3: chân tag dùng chung cho BlinkA và BlinkB.
- PA0: TIM2_CH1, xuất PWM kiểm tra clock.

## 2. Memory map và Reset Sequence

Linker script đặt FLASH tại 0x08000000 và RAM tại 0x20000000.

- .isr_vector: Vector Table trong FLASH; dùng KEEP để tránh bị
  loại bỏ bởi --gc-sections.
- .text: mã chương trình; linker script hiện tại gộp .rodata vào đây.
- .data: biến có giá trị khởi tạo, chạy trong RAM; giá trị ban đầu
  được lưu trong FLASH.
- .bss: biến cần khởi tạo về 0 trong RAM.
- .noinit: vùng startup không chủ động khởi tạo; không bảo đảm
  giữ dữ liệu sau khi mất nguồn.
- Initial MSP: 0x20005000, đỉnh SRAM.
- Linker dự trù 1 KiB cho MSP và kiểm tra vùng dữ liệu không
  chồng lấn vùng này.

Khi reset, CPU nạp MSP và địa chỉ Reset_Handler từ Vector Table.
Reset_Handler copy .data từ FLASH sang RAM, xóa .bss,
gọi SystemInit rồi gọi main.

Stack của FreeRTOS task được cấp riêng từ heap_4.
Mỗi Blink task yêu cầu 256 StackType_t, tương ứng 1024 byte
trên Cortex-M3. Đây chưa bao gồm bộ nhớ TCB.
FreeRTOS heap được cấu hình 8 KiB và nằm trong .bss.

## 3. Clock Tree và Flash

RCC->CR:

- HSEON bật HSE; chờ HSERDY xác nhận oscillator sẵn sàng.
- PLLON bật PLL; chờ PLLRDY trước khi chọn PLL làm SYSCLK.
- Các vòng chờ có giới hạn số lần lặp; nếu thất bại,
  chương trình dừng tại vòng lặp xử lý lỗi.

FLASH->ACR:

- LATENCY = 2: đặt 2 wait-state trước khi tăng SYSCLK.
- PRFTBE = 1: bật prefetch.

RCC->CFGR:

- PLLSRC = 1: lấy HSE làm nguồn PLL.
- PLLXTPRE = 0: không chia đôi HSE.
- PLLMUL = 0111: nhân 9, tạo clock danh định 72 MHz.
- HPRE = 0000: HCLK = SYSCLK = 72 MHz.
- PPRE1 = 100: PCLK1 = HCLK / 2 = 36 MHz.
- PPRE2 = 000: PCLK2 = HCLK = 72 MHz.
- ADCPRE = 10: ADC clock = PCLK2 / 6 = 12 MHz.
- SW = 10: chọn PLL làm SYSCLK; chờ SWS = 10 xác nhận.

SystemCoreClock được cập nhật thành 72000000 sau khi
chuyển SYSCLK thành công. Đây là biến mô tả clock,
không phải thanh ghi điều khiển clock.

## 4. GPIO và AFIO

RCC->APB2ENR bật clock GPIOA, GPIOB và AFIO.

AFIO->MAPR:

- SWJ_CFG = 010: tắt JTAG để giải phóng PB3, giữ SWD.
- Các remap khác được đặt về mặc định.

GPIOB->CRL, trường cấu hình PB3:

- MODE3 = 10: output speed 2 MHz.
- CNF3 = 00: general-purpose push-pull.
- Nibble cấu hình = 0x2.
- Output speed 2 MHz không phải tần số toggle của task.

Driver lựa chọn CRL cho pin 0–7 và CRH cho pin 8–15.
Mỗi pin chiếm 4 bit; driver chỉ thay đổi trường của pin được chọn.

GPIO->BSRR đặt output HIGH; GPIO->BRR đặt output LOW.
Main chuẩn bị output latch LOW trước khi bật output driver.
ODR được đọc để xác định trạng thái latch khi toggle.

Hai task cùng thay đổi PB3 nên thao tác đọc ODR và ghi
BSRR/BRR được đặt trong critical section để tránh Race condition
giữa hai task.

Thanh ghi được truy cập qua volatile để compiler thực hiện
các truy cập cần thiết. Volatile không thay thế critical section
và không biến chuỗi read-modify-write thành thao tác atomic.

## 5. TIM2 tạo xung kiểm tra clock

RCC->APB1ENR bật clock TIM2.
RCC->APB1RSTR reset TIM2 trước khi cấu hình.

GPIOA->CRL, trường PA0:

- MODE0 = 10; CNF0 = 10.
- Nibble = 0xA: Alternate function push-pull, speed 2 MHz.

Do APB1 prescaler chia 2, TIM2CLK = 2 × PCLK1 = 72 MHz.

- PSC = 71: counter clock = 72 MHz / 72 = 1 MHz.
- ARR = 999: chu kỳ gồm 1000 count.
- CCR1 = 500: duty cycle danh định 50%.
- CCMR1: CC1S = 00, OC1M = 110 (PWM mode 1), OC1PE = 1.
- CCER: CC1E = 1, CC1P = 0.
- CR1: ARPE = 1; CEN = 1 sau khi cấu hình.
- DIER = 0: không dùng interrupt hoặc DMA.
- EGR: UG = 1 để nạp cấu hình.
- SR = 0: xóa các cờ sau bước khởi tạo.

f_PWM = 72000000 / ((71 + 1) × (999 + 1)) = 1000 Hz.

Xung được tạo bởi hardware timer, không cần task toggle PA0.

## 6. NVIC và FreeRTOS

STM32F103 có 4 bit interrupt priority.

SCB->AIRCR:

- VECTKEY = 0x5FA để cho phép ghi.
- PRIGROUP = 3: 4 bit preemption priority, 0 bit subpriority.

Sau khi ghi AIRCR:

- DSB bảo đảm các truy cập trước đó hoàn tất theo yêu cầu barrier.
- ISB đồng bộ luồng thực thi sau thay đổi cấu hình.

FreeRTOS port đặt PendSV và SysTick ở priority thấp nhất.
Với 4 bit priority, mức thấp nhất là 15, mã hóa thành 0xF0.
MAX_SYSCALL_INTERRUPT_PRIORITY cấu hình mức 5, mã hóa thành 0x50.

Vector Table được nối với các handler của port:

- SVC_Handler: khởi động task đầu tiên.
- PendSV_Handler: chuyển context.
- SysTick_Handler: cập nhật tick và yêu cầu scheduling.

configTICK_RATE_HZ = 1000: một tick danh định bằng 1 ms.
BlinkA và BlinkB cùng priority 1; Idle task priority 0.
Hai task dùng xTaskDelayUntil với khoảng toggle 264 ms và 704 ms.

## 7. Build và nạp

Configure:
cmake --preset debug -B build/w1e1-check

Build:
cmake --build build/w1e1-check --target w1e1

Build và nạp:
cmake --build build/w1e1-check --target w1e1_flash
