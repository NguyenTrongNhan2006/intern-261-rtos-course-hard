# W3.2 – REPORT

## Cách đo

- Nạp: `cmake --build build/w3e2 --target w3e2_flash`. Firmware ngủ STOP gần hết thời gian nên khi nạp lại phải giữ nút RESET.
- Đo trôi giờ: `py tools/w3e2_drift.py --port COM19 --bound-ppm 390 --csv w3e2_drift.csv`, rồi nhấn RESET board để bắt dòng `START`.
  Script đóng dấu giờ PC khi nhận xong mỗi dòng log (mỗi phút một dòng `SAMPLE`, dòng `DONE` ở 600 s) và so với `tick_ms` của MCU.
  Hằng số trễ truyền dòng log bị triệt tiêu khi lấy hiệu giữa hai dòng, nhiễu đóng dấu giờ PC cỡ vài chục ms.
- Nguồn RTC: **LSI** (Blue Pill không có thạch anh 32,768 kHz), danh định 40 kHz, đo thực tế lúc khởi động ≈ 40.110,8 Hz.
  Dung sai LSI theo datasheet: 30/40/60 kHz (min/điển hình/max), nên bắt buộc phải hiệu chuẩn theo HSE.

## Số đo

- Bảng dòng tiêu thụ: **chưa đo**, cần đồng hồ vạn năng nối tiếp đường 3,3 V (tháo ST-Link và USB-UART khi đo).

| Trạng thái | Dòng đo (mA / µA) |
|---|---|
| Active | (chưa đo) |
| Idle (STOP mode) | (chưa đo) |

- Bảng trôi tick trong `W3E2_DRIFT_TEST_WINDOW_MIN` = 10 phút. Dữ liệu gốc:
  [measurements/w3e2_drift_run2_recal.csv](measurements/w3e2_drift_run2_recal.csv).

**Lần 2 (có hiệu chuẩn lại LSI mỗi ~61 s)**: kết quả **đạt**.

| Giờ MCU (s) | Giờ PC (s) | Chênh lệch (ms) | Trôi thêm trong phút (ms) | ppm tích lũy | `cal_freq_millihz` | `recals` |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0,000 | 0,0 | – | – | 40.110.809 | 0 |
| 60 | 60,029 | −28,8 | −28,8 | −479 | 40.110.809 | 0 |
| 120 | 120,046 | −46,0 | −17,2 | −383 | 40.099.417 | 1 |
| 180 | 180,060 | −60,2 | −14,2 | −334 | 40.097.586 | 2 |
| 240 | 240,070 | −69,8 | −9,6 | −291 | 40.095.492 | 3 |
| 300 | 300,083 | −83,2 | −13,4 | −277 | 40.091.241 | 4 |
| 360 | 360,096 | −95,9 | −12,7 | −266 | 40.090.783 | 5 |
| 420 | 420,106 | −106,2 | −10,3 | −253 | 40.091.175 | 6 |
| 480 | 480,118 | −118,0 | −11,8 | −246 | 40.091.437 | 7 |
| 540 | 540,129 | −128,8 | −10,8 | −238 | 40.092.418 | 8 |
| **600** | 600,145 | **−144,8** | −16,0 | **−241** | 40.089.213 | 9 |

- Kết thúc: `rtc_faults=0`, `late_wakes=0`, `sleep_entries=600`.

**Lần 1 (hiệu chuẩn LSI một lần lúc khởi động)**: **không đạt**. Không còn file CSV vì bị ghi đè, số liệu lấy từ output của lần chạy.

| Giờ MCU (s) | 60 | 120 | 180 | 240 | 300 | 360 | 420 | 480 | 540 | 600 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Chênh lệch (ms) | −36,3 | −84,5 | −140,9 | −199,7 | −259,9 | −319,0 | −379,0 | −443,3 | −515,5 | **−585,7** |

- Kết thúc lần 1: −975,2 ppm, `sleep_entries=600`, `rtc_faults=0`, `late_wakes=0`. Mức trôi mỗi phút tăng dần từ ≈36 ms lên ≈70 ms.

- Logic analyzer đối chiếu thời điểm vào/ra STOP: Saleae Logic (bản sao), PulseView, lấy mẫu 1 MHz (độ phân giải 1 µs),
  5 M mẫu. Đầu dò ở kênh D1 nối **PA0**: mức thấp = từ ngay trước `WFI` đến khi đã khôi phục 72 MHz, mức cao = đang chạy.

| Phép đo (đặt con trỏ trong PulseView) | Giá trị đo | Dự đoán |
|---|---:|---:|
| Độ rộng xung cao (cạnh lên +957 ms → cạnh xuống +979 ms) | **21,826 ms** | ≈ 20–25 ms |
| Chu kỳ, cạnh lên → cạnh lên (+957 → +1957 ms) | **1000,152 ms** | 1000 ms |
| Chu kỳ, cạnh xuống → cạnh xuống (+979 → +1979 ms) | **1000,555 ms** | 1000 ms |
| Độ rộng xung thấp (STOP + khôi phục xung), suy ra = chu kỳ − xung cao | ≈ **978,3–978,7 ms** | ≈ 975–980 ms |

  + MCU nằm ở mức thấp khoảng 97,8% mỗi chu kỳ, đúng mô hình 20 ms active / 980 ms idle trong PREDICTION.
  + Xung cao 21,826 ms = 20 ms chạy bận của task + ≈1,8 ms còn lại (in log, các tick SysTick trước hạn kế tiếp và phần chuẩn bị ngủ).
  + Hai chu kỳ đo được dài hơn 1000 ms khoảng 0,15–0,56 ms (+152…+555 ppm theo đồng hồ của analyzer), cùng chiều với việc MCU chạy chậm
    so với PC (−241 ppm). Chỉ có hai chu kỳ và chưa biết sai số thạch anh của analyzer nên đây chỉ là dấu hiệu khớp, không phải phép đo độc lập.
  + Chân PA0 không tách được riêng thời gian thoát STOP và khôi phục HSE/PLL (dự báo ≈1,7–2,2 ms), vì chân chỉ lên mức cao sau khi PLL đã khóa.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- **So với ngưỡng**: lần 2 trôi −144,8 ms = −241,3 ppm, ngưỡng 234,0 ms (390 ppm). **Đạt**, dư 89,2 ms (149 ppm).
  Dự đoán trong PREDICTION ("hiệu chuẩn theo HSE: 100–300 ppm, 60–180 ms") nằm đúng khoảng kết quả. Lần 1 (−975 ppm) vượt ngưỡng.
- **Phân tách nguồn trôi** (phần nào đo được, phần nào là giả thuyết):
  + *Dao động LSI đổi theo thời gian (đã chứng minh bằng số đo)*: tần số LSI đo lại giảm từ 40.110.809 mHz (lúc khởi động) xuống
    40.089.213 mHz sau 10 phút, tức giảm 21,6 Hz = **−538 ppm**. Vì firmware ngủ gần 98% thời gian bằng LSI, nếu dùng
    mãi tần số cũ thì MCU tính thiếu thời gian, khớp với lần 1 (−975 ppm, mức trôi mỗi phút tăng dần). Hiệu chuẩn lại mỗi ~61 s đưa
    trôi từ −975 xuống −241 ppm, tức giảm khoảng 734 ppm.
  + *Phần dư không phải do LSI (đo được số, nguyên nhân chưa kiểm chứng)*: từ phút thứ 5 đến 10, mức trôi vẫn đều khoảng
    −12,5 ms/phút (≈ −208 ppm) trong khi `cal_freq_millihz` gần như không đổi (40.091.241 → 40.092.418). Phần dư này
    không đến từ LSI. Giả thuyết hợp lý nhất là sai số tần số thạch anh HSE 8 MHz, vì DWT, SysTick và phép đo LSI đều lấy HSE
    làm chuẩn nên mọi sai lệch HSE biến thành sai số cố định. Một lỗi cộng cố định ≈0,2 ms mỗi chu kỳ ngủ cũng cho cùng hiện
    tượng, và dữ liệu hiện có không phân biệt được hai khả năng này. Cách kiểm chứng: chạy 10 phút khi không vào STOP
    (chỉ dùng SysTick), nếu vẫn trôi ≈ −200 ppm thì là HSE. Mình chưa làm phép thử này.
  + *Phút đầu tiên*: −28,8 ms (−479 ppm) vì lần hiệu chuẩn lại đầu tiên diễn ra ở ≈61 s, trước đó dùng giá trị đo lúc khởi động.
  + *Làm tròn khi bù tick (`vTaskStepTick`)*: phần lẻ dưới 1 tick được giữ qua `s_subtick_rem` và qua đoạn nạp lại SysTick,
    nên không tích lũy. Cận lý thuyết ≤ 1 ms tại mọi thời điểm (≤ 1,7 ppm trên 600 s). Đây là suy luận từ code, chưa có phép đo riêng.
  + *Thời gian vào/ra STOP*: nằm trong khoảng giữa hai mẫu RTC nên được bù tự động. Sai số còn lại là độ phân giải đọc RTC
    (1 chu kỳ LSI ≈ 25 µs mỗi lần ngủ, cận trên 15 ms sau 600 lần nếu lệch cùng dấu, tức ≤ 25 ppm). Đây cũng là cận lý thuyết.
    Logic analyzer xác nhận mỗi giây vào STOP đúng một lần với mức thấp ≈ 978 ms, nhưng không tách được riêng phần thoát STOP.
  + Tổng kết: trôi −241 ppm của lần 2 gồm phần dư ≈ −208 ppm (đo từ phút 5–10, nguyên nhân chưa chứng minh) và phần còn lại
    ≈ −33 ppm đến từ phút đầu tiên chưa được hiệu chuẩn lại cùng nhiễu đo LSI (tần số đo lại dao động trong khoảng ≈3.200 mHz ≈ 80 ppm).
    Đây là cách chia theo số liệu, không phải phép phân rã chính xác.
- **Về `sleep_entries`**: mình dự đoán 591 (9 lượt dùng để đo lại thay vì vào STOP) nhưng đo được 600. Suy luận: sau khi lượt đo lại
  kết thúc, vòng lặp Idle gọi lại hook với phần còn lại của khoảng rảnh (còn ≈ 700 tick) và vào STOP ngay trong cùng chu kỳ. Chi phí
  của mỗi lần đo lại là ≈0,26 s chạy 72 MHz, không mất lượt ngủ.
- **So dự đoán dòng tiêu thụ**: chưa có số đo dòng nên chưa so sánh được.

## Trả lời câu hỏi trade-off

Nếu giảm tần số đánh thức RTC (ngủ lâu hơn mỗi lần) để tiết kiệm điện hơn nữa, điều gì đánh đổi về độ chính xác
bù tick và về khả năng phản ứng với các task khác? Chứng minh bằng số đo của chính bạn.

- **Độ chính xác bù tick**: số đo cho thấy độ chính xác phụ thuộc vào việc tần số LSI được cập nhật thường xuyên đến đâu.
  Cùng một firmware, cập nhật một lần (lúc khởi động) trôi −975 ppm, cập nhật mỗi ~61 s trôi −241 ppm. Do đó càng ít đánh thức và càng ít đo lại thì
  sai số LSI càng tích lũy, vì LSI đổi −538 ppm chỉ trong 10 phút. Nếu mỗi lần ngủ dài hơn thì lỗi tần số nhân với thời gian ngủ dài hơn, và
  làm tròn mỗi lần ngủ giảm nhưng không bù được phần này.
- **Điện năng**: mỗi lần đo lại tốn khoảng 0,26 s chạy 72 MHz (27–36 mA theo PREDICTION) mỗi ~61 s, tương ứng khoảng +0,43% thời gian active,
  cộng thêm dòng trung bình cỡ 0,12–0,15 mA so với mô hình hai trạng thái (≈0,55–0,74 mA ở chu kỳ 20/980 ms). Đó là cái giá của việc giữ
  trôi dưới ngưỡng bằng LSI. Con số này là tính từ dự báo, chưa đo bằng đồng hồ.
- **Khả năng phản ứng**: firmware chỉ đánh thức theo mốc của task kế tiếp nên các task theo lịch vẫn đúng giờ (`late_wakes=0`).
  Ngủ lâu hơn (tăng `MAX_STOP_TICKS`) chỉ ảnh hưởng nguồn đánh thức không theo lịch. Hiện chỉ có RTC alarm làm nguồn đánh thức nên
  không đo được độ trễ phản ứng ngắt ngoài. Mình chưa thử các giá trị `MAX_STOP_TICKS` khác nên chưa có số đo cho
  đường cong trade-off, chỉ có hai điểm (1 lần và mỗi ~61 s).
