# Syllabus – Kế hoạch chi tiết 4 tuần

## W1.1 – Bring-up bare-metal
- **Nội dung:** Viết tay từ con số 0: linker script (FLASH/RAM, .data/.bss/.noinit, stack), startup bằng C (vector table, Reset_Handler copy .data, xóa .bss), clock 72 MHz (HSE + PLL, flash latency) bằng thanh ghi RCC, driver GPIO, map SVC/PendSV/SysTick sang port FreeRTOS, NVIC priority group, toolchain file + CMakeLists.txt + CMakePresets.json. Xác minh 72 MHz bằng TIM xuất xung 1 kHz đo trên logic analyzer.
- **API bắt buộc:** xTaskCreate, vTaskStartScheduler, configASSERT, vApplicationMallocFailedHook
- **Tài liệu:** RM0008 (memory map, RCC, FLASH, GPIO, TIM); PM0056 (exception model, vector table); tài liệu GNU ld; tài liệu CMake (toolchain file); Mastering ch.1–2
- **Output:** CMake build sạch với -Werror (mỗi bài là 1 target riêng, có thêm target flash qua OpenOCD); blinky 2 task; ảnh đo xác minh tần số; SETUP.md giải thích từng thanh ghi đã ghi.
- **Tiêu chí đạt:** Giải thích được từng dòng linker script, flash wait state, và vì sao PendSV phải có priority thấp nhất.
- **Giờ:** 6

## W1.2 – Tracer tự viết + đo context switch
- **Nội dung:** Hiện thực trace hook (traceTASK_SWITCHED_IN/OUT) ghi tag task ra 3 chân GPIO + ring buffer có timestamp DWT CYCCNT; dump qua UART (driver thanh ghi tự viết); script Python vẽ Gantt.
- **API bắt buộc:** vTaskSetApplicationTaskTag, xTaskGetApplicationTaskTag, trace hook macros
- **Tài liệu:** freertos.org 'Trace Hook Macros'; PM0056 (DWT)
- **Output:** Tracer + Gantt chart + bảng thời gian context switch (min/max, cycle) ở -O0 và -O2.
- **Tiêu chí đạt:** Gantt từ tracer khớp ảnh logic analyzer; giải thích chênh lệch -O0/-O2.
- **Giờ:** 4

## W1.3 – Phân tích lập lịch (RTA)
- **Nội dung:** Task set 4 task (C, T, D) sinh theo MSSV; C tạo bằng busy-wait DWT chính xác. Tính tay: utilization, giới hạn Liu–Layland, WCRT bằng RTA. Đo WCRT thực bằng tracer. Một số MSSV nhận task set KHÔNG schedulable → phải tự phát hiện và đề xuất sửa.
- **API bắt buộc:** xTaskDelayUntil, vTaskPrioritySet
- **Tài liệu:** Liu & Layland; RTA; Mastering ch.4
- **Output:** Bảng tính tay + bảng WCRT đo được + phân tích nguồn sai số (tick, context switch, ISR).
- **Tiêu chí đạt:** Sai lệch dự đoán–đo được giải thích định lượng bằng số đo context switch ở W1.2.
- **Giờ:** 5

## W1.4 – 100% cấp phát tĩnh + ngân sách RAM
- **Nội dung:** Thí nghiệm phân mảnh heap_4 (chuỗi create/delete theo MSSV), sau đó chuyển toàn hệ thống sang configSUPPORT_DYNAMIC_ALLOCATION=0. Phân tích file .map, stack painting.
- **API bắt buộc:** xTaskCreateStatic, xQueueCreateStatic, xSemaphoreCreateMutexStatic, vApplicationGetIdleTaskMemory, vApplicationGetTimerTaskMemory, uxTaskGetStackHighWaterMark
- **Tài liệu:** Mastering ch.3; tài liệu GNU ld (đọc .map)
- **Output:** Log phân mảnh + hệ thống chạy 0 byte heap + bảng RAM khớp .map đến từng section.
- **Tiêu chí đạt:** Giải thích được vì sao phân mảnh xảy ra với chuỗi của chính mình.
- **Giờ:** 3

## W2.1 – Driver I2C ngắt + phục hồi bus
- **Nội dung:** Driver I2C1 mức thanh ghi, interrupt-driven, báo hoàn tất bằng task notification, có timeout và bus recovery (toggle SCL). Xử lý theo errata I2C của F103. 2 task client dùng chung qua mutex.
- **API bắt buộc:** xTaskNotifyFromISR, ulTaskNotifyTake, xSemaphoreTake/Give (mutex static)
- **Tài liệu:** RM0008 (I2C – đọc kỹ phần trình tự ngắt); errata sheet F103; datasheet MPU6050
- **Output:** Driver + test rút dây SDA giữa transaction: hệ thống tự phục hồi không reset; ảnh đo bus recovery.
- **Tiêu chí đạt:** Phục hồi được ≥10 lần liên tiếp; giải thích được trình tự EV5–EV8.
- **Giờ:** 5

## W2.2 – Ring buffer lock-free vs queue
- **Nội dung:** Tự viết SPSC ring buffer (ISR producer, task consumer) có memory barrier; stress test bằng sequence counter; benchmark cycle so với xQueueSendFromISR.
- **API bắt buộc:** xQueueSendFromISR, xQueueReceive, portYIELD_FROM_ISR
- **Tài liệu:** PM0056 (DMB/DSB, exclusive access); Mastering ch.5
- **Output:** Code + chứng minh đúng (1 triệu phần tử không mất/lặp) + bảng cycle.
- **Tiêu chí đạt:** Lập luận được vì sao SPSC an toàn không cần lock, và khi nào thì không.
- **Giờ:** 3

## W2.3 – Inversion, chained blocking, deadlock
- **Nội dung:** 4 task, 2 mutex: tái hiện unbounded inversion (binary semaphore), chained blocking dù có inheritance, deadlock do thứ tự lock. Tự hiện thực wrapper Immediate Priority Ceiling trên API gốc; so sánh thời gian block.
- **API bắt buộc:** xSemaphoreCreateBinaryStatic, xSemaphoreCreateMutexStatic, vTaskPrioritySet, uxTaskPriorityGet
- **Tài liệu:** Mastering ch.8; tài liệu về Priority Ceiling Protocol
- **Output:** Gantt (tracer W1.2) cho từng hiện tượng + bảng thời gian block: inheritance vs ceiling.
- **Tiêu chí đạt:** Chỉ đúng điểm chained blocking trên Gantt; giải thích vì sao ceiling chặn deadlock.
- **Giờ:** 4

## W2.4 – Event group, barrier, notification
- **Nội dung:** Barrier N task bằng xEventGroupSync; thay toàn bộ binary semaphore bằng notification; tìm bug cài sẵn: mất sự kiện do eSetValueWithOverwrite.
- **API bắt buộc:** xEventGroupSync, xEventGroupWaitBits, xTaskNotify, xTaskNotifyWait
- **Tài liệu:** Mastering ch.9–10
- **Output:** Bảng RAM/cycle trước–sau + báo cáo bug có bằng chứng từ tracer.
- **Tiêu chí đạt:** Tìm được bug bằng tracer, không đoán.
- **Giờ:** 4

## W3.1 – UART DMA + IDLE line, 0 mất byte
- **Nội dung:** USART1 RX qua DMA circular + ngắt IDLE + half/full transfer → stream buffer → parser lệnh. Baud theo MSSV (460800–1 Mbps). Script PC gửi liên tục có CRC trong 10 phút.
- **API bắt buộc:** xStreamBufferCreateStatic, xStreamBufferSendFromISR, xStreamBufferReceive
- **Tài liệu:** RM0008 (USART, DMA); Mastering (Stream Buffers)
- **Output:** Log 10 phút 0 lỗi CRC + CPU load (run-time stats) + giải thích chọn kích thước buffer.
- **Tiêu chí đạt:** 0 byte mất ở baud được giao; CPU load đo được.
- **Giờ:** 5

## W3.2 – Tickless idle + STOP mode
- **Nội dung:** configUSE_TICKLESS_IDLE=2, tự viết portSUPPRESS_TICKS_AND_SLEEP dùng RTC alarm đánh thức từ STOP. Bù tick chính xác: test trôi giờ 10 phút so với giờ PC. Đo dòng idle vs active.
- **API bắt buộc:** portSUPPRESS_TICKS_AND_SLEEP, vTaskStepTick, eTaskConfirmSleepModeStatus
- **Tài liệu:** freertos.org 'Low Power Support'; RM0008 (PWR, RTC, EXTI)
- **Output:** Bảng dòng tiêu thụ + bảng trôi tick + ảnh đo thời điểm thức dậy.
- **Tiêu chí đạt:** Trôi < ngưỡng theo MSSV; giải thích được nguồn trôi.
- **Giờ:** 5

## W3.3 – HardFault handler + post-mortem
- **Nội dung:** Handler (asm + C) lưu stacked frame (PC, LR, xPSR), CFSR/HFSR/BFAR/MMFAR và tên task hiện tại vào vùng RAM .noinit; in ra sau reset. Mentor cài sẵn các lỗi gây fault khác nhau.
- **API bắt buộc:** pcTaskGetName, xTaskGetCurrentTaskHandle
- **Tài liệu:** PM0056 (fault handling, CFSR); RM0008
- **Output:** Crash log cho từng lỗi + chỉ ra dòng code gây lỗi từ PC bằng addr2line.
- **Tiêu chí đạt:** Tìm đúng dòng lỗi cho mọi fault cài sẵn.
- **Giờ:** 3

## W3.4 – Debug hộp đen (chỉ có .elf)
- **Nội dung:** Mentor đưa file .elf có lỗi ẩn (deadlock / stack overflow / sai priority), KHÔNG có source. Dùng OpenOCD + GDB đọc TCB, danh sách ready/blocked, stack để tìm nguyên nhân.
- **API bắt buộc:** (đọc cấu trúc TCB, pxReadyTasksLists, xDelayedTaskList qua GDB)
- **Tài liệu:** Mã nguồn tasks.c của FreeRTOS-Kernel; tài liệu GDB
- **Output:** Báo cáo nguyên nhân kèm lệnh GDB và output làm bằng chứng.
- **Tiêu chí đạt:** Chỉ đúng nguyên nhân gốc, có bằng chứng từ GDB.
- **Giờ:** 3

## W4.1 – Thiết kế hệ thống mixed-criticality
- **Nội dung:** Control loop 1 kHz kích bằng TIM (jitter tối đa theo MSSV, cỡ µs), IMU 500 Hz qua driver I2C W2.1, telemetry UART DMA, shell lệnh, tickless khi rảnh. Static only.
- **API bắt buộc:** Tự chọn (chỉ API gốc)
- **Tài liệu:** Tổng hợp tuần 1–3
- **Output:** Design doc: task diagram, RTA dự đoán WCRT mọi task, ngân sách RAM/CPU, bảng failure mode & cách xử lý. Mentor duyệt trước khi code.
- **Tiêu chí đạt:** RTA chứng minh schedulable trên giấy trước khi viết code.
- **Giờ:** 4

## W4.2 – Hiện thực
- **Nội dung:** Code theo design; check_api.sh sạch; -Werror; AI_LOG.md đầy đủ; không có file nào do công cụ sinh ra.
- **Output:** Repo hoàn chỉnh.
- **Tiêu chí đạt:** Build sạch, 0 heap, 0 HAL/LL/CMSIS-OS.
- **Giờ:** 7

## W4.3 – Kiểm chứng + fault injection
- **Nội dung:** Đo WCRT, jitter, CPU, stack. Lệnh fault injection: treo 1 task, flood UART, rút cảm biến, quá tải CPU → hệ thống phải suy giảm có kiểm soát, watchdog phục hồi, ghi lý do reset.
- **API bắt buộc:** vTaskGetRunTimeStats, uxTaskGetStackHighWaterMark, xEventGroupWaitBits
- **Output:** Bảng dự đoán RTA vs đo + log từng kịch bản fault.
- **Tiêu chí đạt:** Control loop không trễ deadline ở mọi kịch bản trừ khi đã được thiết kế để suy giảm.
- **Giờ:** 4

## W4.4 – Bảo vệ
- **Nội dung:** 15 phút; mentor đổi thông số tại chỗ VÀ tiêm 1 lỗi SV chưa biết trước.
- **Output:** Buổi bảo vệ.
- **Tiêu chí đạt:** Dự đoán đúng hướng và chẩn đoán được lỗi tại chỗ.
- **Giờ:** 1

## Rubric
- 1 | Bare-metal, tracer, RTA, static | 20%
- 2 | Driver I2C, lock-free, đồng bộ nâng cao | 25%
- 3 | DMA, low-power, fault, hộp đen | 25%
- 4 | Mixed-criticality + bảo vệ | 30%
- Code | Đúng API gốc, driver thanh ghi, -Werror sạch | 25%
- Số đo + báo cáo | PREDICTION.md, ảnh đo, REPORT.md, phân tích sai số định lượng | 40%
- Vấn đáp | Trả lời + xử lý thay đổi / lỗi tại chỗ | 35%
- Ngưỡng đạt (thang 10) |  | 7
- Điểm liệt: Dùng CMSIS-OS / HAL / LL hoặc bất kỳ file do công cụ sinh ra → phần code bài đó = 0. Không có AI_LOG.md hoặc không giải thích được code tự nộp → phần vấn đáp = 0. Dùng heap từ W1.4 trở đi → trừ 50% phần code.
