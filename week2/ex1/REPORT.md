# W2.1 – REPORT

MSSV: 2452882. STM32F103C8T6, I2C1 PB6/PB7, 100 kHz, MPU6050 0x68, timeout 28 ms, yêu cầu 11 lần phục hồi liên tiếp.
Lỗi `INIT,BUS` ở lần chạy đầu đã khắc phục: khởi tạo và đọc mẫu OK (`measurements/uart_mpu6050_ok.png`).

## Cách test
- Không rút dây tay; dùng GPIO mô phỏng (README cho phép): PB12 open-drain → 220 Ω → SDA, kéo SDA LOW 30 µs mỗi giây (bật bằng `-DW2E1_FAULT_INJECT=ON`).
- Điểm đánh dấu cho logic analyzer (bật bằng `-DW2E1_TRACE_EV=ON`): PB13 = ISR EV, PB14 = ISR ER. PB12 nối LA nên kênh FAULT bám theo SDA qua điện trở.
- Logic analyzer: PulseView, 12 MHz (giao dịch thường) và 4 MHz (chèn lỗi). Hai lần chụp đầu sai dây đo đã bị bỏ.

## Số đo

**Chuỗi ngắt bình thường** (`measure_v1.sr`, `measure_v1_boot.sr`):

| Giao dịch | Xung EV | Dự đoán | Thời gian bus | Xung SCL |
|---|---|---|---|---|
| Ghi 1 byte (chọn register) | 4 | N+3 = 4 | 196,6 µs | 19 |
| Đọc 6 byte | 7 | 7 | 646,6 µs | 64 |
| Đọc 1 byte (`WHO_AM_I`) | 3 | 3 | 196,7 µs | 19 |
| Ghi 2 byte (`PWR_MGMT_1`) | 5 | N+3 = 5 | 286,6 µs | 28 |

START → EV5: 6,4–6,6 µs (khoảng 5 µs là thời gian giữ START của 100 kHz). Mỗi lần vào ISR EV: 2,1–2,2 µs. Không có xung ER nào.

**Phục hồi liên tiếp** (log UART, 1 banner trong mỗi file ⇒ MCU không reset):

| File | RECOVERED | Streak lớn nhất | Trạng thái khác |
|---|---|---|---|
| `w2e1_inject_la.log.txt` | 38 | 38 | không có |
| `w2e1_inject_la2.log.txt` | 38 | 38 | không có |
| `w2e1_inject_la4.log.txt` | 40 | 40 | không có |

Mỗi `RECOVERED` đều được một dòng `OK` kế tiếp xác nhận mới tính vào streak. Yêu cầu 11 lần: đạt. Thời gian một lượt `RECOVERED` (cột `US`, `la4`): 14 lần nhanh 153–427 µs, 26 lần chậm 28 050 µs. `PULSES` luôn bằng 0.

**Dạng sóng recovery** (`measure_v4_inject.sr`, 4 sự cố trong 5 s):

| Thời điểm xung lỗi | Vị trí | Phục hồi |
|---|---|---|
| 1097,7 / 2120,8 / 3150,9 ms | bus rảnh, SCL HIGH (START+STOP giả, không có xung SCL) | chậm: START/STOP GPIO sau 92 / 31 / 59 ms |
| 4176,95 ms | giữa giao dịch ghi, sau 4 xung SCL | nhanh: ER sau 1 µs, START/STOP GPIO sau 38 µs, dài 17 µs |

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- Một cặp ghi + đọc đo bằng DWT là 917 µs, trên bus là 196,6 + 36 + 646,6 = 879 µs. Phần dư khoảng 38 µs là thời gian phần mềm (lấy mutex, `wait_bus_free`, nhả bus). Bật PB13 trong ISR làm số này từ 916 lên 917 µs.
- `US` = 28 050 µs ≈ 28 ms timeout + khoảng 50 µs recovery. Ca chậm: xung lỗi rơi lúc bus rảnh, client kế tiếp không thấy bus rảnh trong 28 ms rồi recovery; trễ 31–92 ms của 3 sự cố khác nhau chỉ vì client kế tiếp bắt đầu sớm hay muộn (sự cố 2: 3 ms + 28 ms). Đây là suy luận từ dạng sóng, chưa đọc trực tiếp cờ BUSY.
- Xung chèn lỗi đặt 30 µs đo được 32 µs; thời điểm rơi trễ khoảng 16 µs so với giá trị đặt do cách chờ SCL LOW.
- So với PREDICTION: số xung EV (N+3) và START → EV5 vài µs đúng; "1–5 ms từ lỗi đến giao dịch OK" đúng cho ca nhanh (khoảng 0,04 ms), sai cho ca chậm (≥ 28 ms vì chờ hết timeout). Dự đoán tối đa 9 xung SCL chưa kiểm chứng vì `PULSES` luôn 0 (MPU6050 chưa giữ SDA).

## Sửa sau kiểm toán
- Nhánh ARLO trước đây trả `BUS` mà không recovery, làm đứt streak ngay ca lỗi phổ biến nhất; nay chạy `i2c1_recover_locked()` (`i2c1_driver.c`).
- Thêm đếm số xung SCL (`i2c1_last_recovery_pulses()`) và hai cột log `PULSES,INJ`.

## Trả lời câu hỏi trade-off

Khi ngoại vi I2C bị kẹt, tiếp tục yêu cầu START không bảo đảm tạo được chuỗi clock cần thiết. Driver tắt I2C, chuyển PB6/PB7 sang GPIO open-drain để điều khiển SCL thủ công, đọc IDR để xác nhận chân thực sự lên HIGH. Sau khi SDA nhả, driver tạo START/STOP rồi trả chân về AF và cấu hình lại I2C.

STOP là chuyển SDA từ LOW lên HIGH khi SCL HIGH, giúp slave kết thúc trạng thái giao tiếp. Nếu bỏ STOP, slave có thể còn chờ phần còn lại của giao dịch khiến lần sau lỗi hoặc BUSY chưa được giải phóng. Đây là lập luận thiết kế, chưa làm phép thử bỏ STOP.
