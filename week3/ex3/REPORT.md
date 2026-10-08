# W3.3 – REPORT

## Số đo
Với MỖI loại fault trong `W3E3_ASSIGNED_FAULT_NAMES`:

| Loại fault | CFSR (hex) | HFSR (hex) | BFAR/MMFAR (hex, valid?) | PC stacked | Dòng code (`addr2line`) |
|---|---|---|---|---|---|
| | | | | | |

- Canary đọc lại sau mỗi lần reset: khớp / không khớp (ghi rõ nếu có lần không khớp).

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- So dự đoán CFSR bit / dòng code trong PREDICTION.md với thực tế đo được cho từng loại fault; nếu lệch, giải
  thích vì sao (ví dụ: trình biên dịch tối ưu hoá làm dịch chuyển PC, hoặc fault thực chất kích hoạt 1 cờ CFSR
  khác với dự đoán ban đầu).
- Nếu canary không khớp ở lần nào: giải thích nguyên nhân (brown-out reset thay vì warm reset, hay lỗi bố trí
  linker script).

## Trả lời câu hỏi trade-off
Với loại fault mà BFAR/MMFAR không hợp lệ (`BFARVALID`/`MMARVALID` = 0), bạn còn cách nào khác để xác định địa
chỉ gây lỗi ngoài CFSR/HFSR? Chứng minh bằng chính log của bạn.
