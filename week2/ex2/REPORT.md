# W2.2 – SPSC ring buffer và FreeRTOS Queue

MSSV **2452882**. Rà soát bằng chứng ngày **30/10/2026**.

## 1. Kết quả và cấu hình

**Đã đủ bằng chứng đo theo yêu cầu W2.2:** stress test nhận đúng 1,000,000 phần tử, không overrun; benchmark Ring/Queue đều hoàn thành 7,048 mẫu. Bằng chứng là ảnh UART trên board, không phải CSV thô xuất từ cổng serial.

| Thành phần | Cấu hình |
|---|---|
| MCU / clock / optimization | STM32F103C8T6, 72 MHz, `-O2` |
| Dữ liệu | `spsc_elem_t`: một `uint32_t seq`, 4 byte |
| Dung lượng | Ring và Queue đều 512 phần tử, bộ nhớ static |
| Producer / consumer | Một ISR TIM2 push/send, một task pop/receive |
| TIM2 | Mỗi 100 µs; 10 kHz theo cấu hình timer |
| Stress | `W2E2_STRESS_COUNT=1000000` |
| Benchmark | `W2E2_BENCH_ITERATIONS=7048` mẫu thành công mỗi cơ chế |

## 2. Bằng chứng và số đo

![UART: Stress, Ring và Queue đều PASS](measurements/uart_ring_queue_pass.png)

Chép từ [ảnh gốc](measurements/uart_ring_queue_pass.png):

```text
PHASE,STATUS,PRODUCED,RECEIVED,OVERRUN,MEAN_PUSH,MEAN_POP
# DWT_OVERHEAD=1
STRESS,PASS,1000000,1000000,0,NA,NA
RING,PASS,7048,7048,0,29,24
QUEUE,PASS,7048,7048,0,210,197
```

Ảnh hiển thị hai bộ kết quả hoàn chỉnh có cùng số liệu. Không suy từ việc lặp trên màn hình thành hai phép đo độc lập có kiểm soát; báo cáo sử dụng bộ giá trị hiển thị và ghi rõ nguồn ảnh.

| Lượt | Produced | Received | Overrun | Kiểm tra |
|---|---:|---:|---:|---|
| STRESS | 1,000,000 | 1,000,000 | 0 | PASS; consumer kiểm tra seq 0…999999 |
| RING | 7,048 | 7,048 | 0 | PASS |
| QUEUE | 7,048 | 7,048 | 0 | PASS |

| Cơ chế | Push/send (cycle) | Pop/receive (cycle) | Tổng từ log (cycle/cặp) | Thời gian ở 72 MHz (µs/cặp) |
|---|---:|---:|---:|---:|
| SPSC ring | 29 | 24 | 53 | 0.736 |
| FreeRTOS Queue | 210 | 197 | 407 | 5.653 |

Tổng Queue/Ring khoảng **7.679 lần**; Ring dùng ít hơn khoảng **86.98% cycle/cặp** trong thiết lập này. Trung bình được firmware chia nguyên làm tròn xuống, nên 53 và 407 là tổng các giá trị hiển thị, không phải hai trung bình tổng có đầy đủ phần lẻ. `NA` ở stress nghĩa là lượt đó không xuất benchmark cycle.

## 3. Cách benchmark và giới hạn

DWT đo quanh `spsc_ring_push_from_isr` / `spsc_ring_pop` hoặc `xQueueSendFromISR` / `xQueueReceive`. Hiệu chỉnh phép đo rỗng là **1 cycle**. Log UART và kiểm tra seq nằm ngoài khoảng đo; `portYIELD_FROM_ISR` cũng nằm ngoài phần đo API send.

Ở lượt benchmark, producer đợi consumer lấy mẫu trước rồi mới tạo mẫu mới. Queue receive dùng timeout 0; consumer không Blocked trên Queue, waiting list rỗng và không đo chi phí đánh thức task. Cả hai cơ chế dùng cùng kiểu phần tử và dung lượng, nhưng lượt này không đo throughput khi đầy queue hoặc độ trễ khi consumer blocking. Ngắt khác vẫn có thể chen vào thời gian DWT.

Stress test tách khỏi lượt benchmark: producer phát sequence tăng dần, consumer đối chiếu từng giá trị. Nếu đầy hoặc sai sequence, firmware ghi lỗi và dừng; không bỏ qua phần tử đầy rồi kết luận PASS.

## 4. Đối chiếu PREDICTION

| Đại lượng | Dự đoán (cycle) | Đo từ ảnh (cycle) | Đo − dự đoán |
|---|---:|---:|---:|
| Ring push | 30 | 29 | −1 |
| Ring pop | 30 | 24 | −6 |
| Ring một cặp | 60 | 53 | −7 (−11.67%) |
| Queue send | 100 | 210 | +110 |
| Queue receive | 140 | 197 | +57 |
| Queue một cặp | 240 | 407 | +167 (+69.58%) |

Tỷ lệ dự đoán là 4 lần, số hiển thị cho khoảng 7.679 lần. Ring xử lý một phần tử 4 byte, mask chỉ số bằng 511 và các DMB. Queue có critical section BASEPRI, barrier của port, cập nhật trạng thái/con trỏ, copy tổng quát, kiểm tra waiting list và assert. Ring cũng copy payload nên không giải thích lợi thế bằng việc Ring không copy.

Chưa đo tách từng nhóm lệnh; phân bổ 60/60/20/100 cycle trong bản dự đoán vẫn là giả thuyết. Không thể quy chính xác +167 cycle cho một nhóm hoặc bỏ ảnh hưởng ngắt chỉ từ các trung bình trên.

## 5. Vì sao SPSC an toàn và khi nào không còn đúng

Chỉ producer ghi `head`, chỉ consumer ghi `tail`. Load/store `uint32_t` căn chỉnh 4 byte là atomic trên Cortex-M3. Producer quan sát `tail`, dùng DMB trước khi tái sử dụng slot, ghi payload rồi DMB trước khi công bố `head`. Consumer quan sát `head`, DMB trước khi đọc payload, đọc xong rồi DMB trước khi trả slot bằng `tail`. `volatile` giúp truy cập chỉ số được thực hiện; nó không thay DMB để giữ thứ tự bàn giao.

Nếu ISR chen khi task đang đọc, consumer chưa trả slot qua `tail`, nên producer chưa được ghi đè slot đó. Mỗi thao tác có số bước hữu hạn và không cần mutex. Bộ đếm uint32_t dùng phép trừ unsigned và mask; trong mô hình SPSC, số phần tử đang giữ bị giới hạn bởi 512.

Nếu có producer thứ hai, A có thể đọc `head=0` rồi bị B chen vào; B ghi slot 0 và công bố `head=1`, sau đó A ghi đè slot 0 rồi cũng công bố `head=1`: mất dữ liệu. DMB không phân xử quyền ghi. Payload nhiều field vẫn có thể đúng nếu producer ghi xong trước publish và consumer chưa trả slot; riêng việc payload lớn hơn một word chưa đủ kết luận SPSC sai.

Stress test là bằng chứng không phát hiện lỗi trong cấu hình đã chạy; lập luận an toàn còn phụ thuộc một producer, một consumer, quyền sở hữu slot và memory ordering trên Cortex-M3 đơn nhân. Không tự suy rộng sang nhiều producer/consumer, DMA hoặc SMP.

## 6. Kiểm tra hồ sơ

`PREDICTION.md` đã có commit `b236159` ngày 21/10/2026; bản rà chỉ sửa bố cục và làm rõ giả định. Source hiện tại qua `tools/check_api.sh week2/ex1 week2/ex2 week2/ex3`: **PASS (14 files)**. Build và phạm vi kiểm chứng ghi trong [validation.md](../ex3/measurements/validation.md). Chưa có raw UART file cho W2.2; ảnh gốc đã lưu là bằng chứng hiện có và đủ đọc các số yêu cầu.
