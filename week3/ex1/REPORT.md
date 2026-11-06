# W3.1 – REPORT

## Cách chạy stress

- Gọi `tools/uart_stress.py` qua wrapper, script gốc giữ nguyên. Script gốc mở cổng với `timeout=0.1`, nên
  `ser.read(4096)` chặn 100 ms mỗi vòng và tốc độ thật chỉ ≈ 9,9 Hz. Wrapper mở cổng với `timeout=0`:
  `py tools/uart_stress_w3e1.py --port COM19 --baud 460800 --payload-size 59 --rate-hz 67 --duration-sec 600 --log-csv w3e1_run.csv --mcu-log w3e1_mcu.log`
- Wrapper cũng giữ lại log MCU trong cùng một lần mở cổng: chờ thêm 8 s (`--drain-sec`) để bắt `RX_QUIET`, lưu
  vào `w3e1_mcu.log` và in bảng CPU load (min/tb/max) cùng kết quả `MCU RESULT`.
- Reset MCU trước mỗi lần chạy. Nếu không reset, `restarts=` trong log MCU tăng lên và các bộ đếm là số cộng dồn.
- `received=0` ở dòng cuối của script là bình thường vì firmware không echo khung. Vì vậy dòng `RESULT: PASS` của
  script không phải bằng chứng. Bằng chứng là các bộ đếm trên MCU trong `measurements/w3e1_mcu.log`.
- Không mở terminal serial khác để đọc log sau khi chạy: mở/đóng cổng có thể reset board qua DTR/RTS và xóa bộ
  đếm. Dùng file log do wrapper lưu.

## Số đo

- Lần chạy 600 s, baud 460800, payload 59 B, 67 Hz. Log đầy đủ: [measurements/w3e1_mcu.log](measurements/w3e1_mcu.log)
  (604 dòng: 601 dòng `SAMPLE` mỗi giây và 3 dòng `RX_QUIET`).
- Tổng khung gửi = **40.200** (`sent=` của script), OK = **40.200**, lỗi CRC = **0**, mất = **0**.

| Bộ đếm trên MCU (dòng `RX_QUIET` cuối) | Giá trị |
|---|---:|
| `bytes` | 2.814.000 (= 40.200 × 70) |
| `ok` / `last_seq` | 40.200 / 40.199 |
| `crc` / `lost` / `len_err` / `old_seq` / `restarts` | 0 / 0 / 0 / 0 / 0 |
| `dropped` (byte bị bỏ ở Stream Buffer) | 0 |
| `dma_late` / `dma_err` | 0 / 0 |
| `ore` / `fe` / `ne` / `clear_busy` | 0 / 0 / 0 / 0 |
| `peak` (đỉnh Stream Buffer) | 122 B trên 511 B hữu dụng |

- Bảng CPU load (run-time stats, TIM2 100 kHz) lấy từ dòng `SAMPLE` ở mỗi mốc 60 s của lúc nhận dữ liệu:

| Thời điểm (s) | Parser (%) | Report (%) | Busy (%) | Idle (%) |
|---|---:|---:|---:|---:|
| 60 | 0,79 | 0,52 | 1,33 | 98,67 |
| 120 | 0,77 | 0,53 | 1,31 | 98,69 |
| 180 | 0,78 | 0,53 | 1,32 | 98,68 |
| 240 | 0,78 | 0,53 | 1,33 | 98,67 |
| 300 | 0,79 | 0,53 | 1,34 | 98,66 |
| 360 | 0,79 | 0,53 | 1,33 | 98,67 |
| 420 | 0,77 | 0,54 | 1,32 | 98,68 |
| 480 | 0,79 | 0,54 | 1,33 | 98,67 |
| 540 | 0,79 | 0,54 | 1,33 | 98,67 |
| 600 | 0,78 | 0,53 | 1,33 | 98,67 |
| **Trung bình 601 mẫu** | **0,78** (max 0,80) | **≈0,53** | **1,32** (max 1,34) | **98,68** |

- Mẫu đầu tiên (Parser 0,45%, Busy 0,49%) thấp hơn vì chỉ phủ một phần giây đầu khi stress bắt đầu.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- **Tràn buffer**: **0 lần**. `dropped=0`, `dma_late=0`, `dma_err=0`, `ore=fe=ne=0` trong cả 600 s. Đỉnh Stream Buffer
  là 70 B (đúng một khung) trong 354 giây đầu và nhảy lên 122 B một lần ở giây ≈355, rồi giữ nguyên đến hết. Đỉnh
  122 B chỉ bằng 23,9% của 511 B, cách mức tràn 389 B. Dự đoán ở PREDICTION cho ngân sách trễ Parser 5 ms ở tốc độ dây là
  453 B, và thực tế không tiến gần mốc đó vì khung đến cách đều 14,9 ms. Mình không có trace để xác định nguyên nhân
  của đợt 122 B (thêm 52 B ≈ 1,1 ms dữ liệu ở tốc độ dây), nên chỉ ghi nhận là một lần trễ ngắn, không tái diễn.
- **Dự đoán 0 byte mất, 0 CRC, 0 khung mất**: đúng. `bytes = 2.814.000` khớp chính xác `40.200 × 70`, và dãy SEQ
  `0 … 40.199` liên tục (`ok = last_seq + 1`, `lost = 0`, `restarts = 0`).
- **CPU load so với dự đoán** (PREDICTION mục 4, phương án DMA + IDLE + HT/TC dự đoán bận 1,24%, Idle 98,76%):
  + Parser đo 0,78% so với dự đoán 0,80%, lệch −2,5%. Dự đoán Parser là phần chính xác nhất, vì nó bám theo ngân sách
    110 chu kỳ/byte + 180 chu kỳ/khung.
  + Cột Report (≈0,53%) là phần PREDICTION không tính (chỉ định dạng/in báo cáo). Một dòng `SAMPLE` dài 233 ký tự + `\r\n`
    ≈ 235 ký tự × 10 bit / 460800 = 5,1 ms phát bằng polling TX mỗi giây = 0,51%, khớp với 0,53% đo được. Vì vậy
    `busy = 1,32%` gộp cả tải in log. Tải thuần của việc nhận (Parser) là 0,78%.
  + Thời gian ngắt DMA/USART và tick (dự đoán 0,45%) không hiện thành cột riêng, vì kernel tính thời gian ngắt vào
    task đang bị ngắt, chủ yếu là Idle. Do đó con số Idle 98,68% không thể tách riêng phần ngắt ra. Không có số đo riêng
    cho ISR trong lần chạy này.
  + Độ phân giải bộ đếm là 10 µs (100 kHz), sai số lượng tử không đáng kể với 601 mẫu.
- Tải CPU nhận ở mức ~0,8% nên chương trình còn dư khoảng 99% CPU: DMA + IDLE không phải là điểm nghẽn ở 460800 baud.

## Trả lời câu hỏi trade-off

Vì sao chọn kích thước DMA buffer / stream buffer như trong PREDICTION.md thay vì lớn hơn (tốn RAM) hay nhỏ hơn
(rủi ro tràn)? Chứng minh bằng chính số đo CPU load và log stress test của bạn.

- **DMA vòng 256 B (mốc HT/TC mỗi 128 B)**:
  + Nửa vòng cho 2,78 ms ở tốc độ dây, gấp ≈5,6 lần ngân sách ISR 0,5 ms đặt ở PREDICTION. Log thực tế `dma_late = 0`
    nghĩa là chưa từng có cả HT lẫn TC cùng chờ, tức ISR luôn chạy kịp trong nửa vòng.
  + Nhỏ hơn (ví dụ 64 B, mốc 32 B) vẫn lớn hơn cận tối thiểu 50 B, nhưng nửa vòng chỉ còn 0,69 ms và số ngắt HT/TC lúc dây
    bận liên tục tăng 4 lần (từ 360/s lên 1.440/s), trong khi mức tải hiện tại đã rất thấp (0,78%). Đây là tính toán, không
    phải số đo.
  + Lớn hơn (1 KB) chỉ làm tăng độ trễ phát hiện và tốn SRAM, trong khi `dma_late = 0` cho thấy 256 B đã dư.
- **Stream Buffer 512 B (511 B hữu dụng)**:
  + Mức đỉnh đo được 122 B. Ngân sách thiết kế ở PREDICTION cho Parser bị trễ 5 ms ở tốc độ dây là 453 B, nên 512 B là mảng
    nhỏ nhất theo lũy thừa của 2 thỏa điều kiện đó. Cả hai mảng chiếm 768 B = 3,75% SRAM 20 KB, và toàn bộ firmware dùng
    4.304 B (21,0%) RAM theo `--print-memory-usage`.
  + Nhỏ hơn (256 B) đã đủ cho mức đỉnh 122 B của lần chạy này, nhưng sẽ vi phạm ngân sách 453 B khi host gửi dồn ở tốc độ dây
    hoặc Parser bị trễ thêm, và `dropped` sẽ tăng. Lần chạy này chỉ chứng minh 256 B đủ với host gửi đều 67 Hz, không chứng
    minh cho kịch bản gửi dồn.
  + Lớn hơn (1 KB) không có lợi ích đo được: đỉnh thực tế chỉ dùng 23,9% của 511 B, còn thêm 512 B RAM tốn thêm 2,5% SRAM.
- **Kết luận**: lựa chọn 256 B / 512 B có `dropped = 0`, `dma_late = 0`, CPU nhận 0,78%. Dư địa CPU và RAM vẫn lớn, nên
  nếu mentor đổi baud hoặc tốc độ gửi tại chỗ thì kiểm lại `peak`, `dropped` và `dma_late` trước tiên.
