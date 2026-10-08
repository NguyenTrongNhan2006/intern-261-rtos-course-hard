# W4.1 – REPORT

## Kết quả duyệt thiết kế
- Ngày nộp design doc cho mentor: ___. Ngày được duyệt: ___. Số vòng sửa: ___.
- Nếu bị yêu cầu sửa: liệt kê từng điểm mentor chỉ ra và cách bạn sửa (định lượng, không chỉ "đã sửa").

## Số đo / đối chiếu
- Bảng RTA cuối cùng (sau khi sửa nếu có) so với PREDICTION.md: khác nhau ở đâu, vì sao (WCET ước lượng ban
  đầu sai lệch so với thực tế đo ở tuần 1–3, hay đổi priority...).
- Ngân sách RAM/CPU cuối cùng so với `PARAMS.md` (`W4E1_RAM_BUDGET_BYTES`, `W4E1_CPU_BUDGET_PERCENT_MAX`):
  đạt với margin bao nhiêu (bytes / %).

## Giải thích sai lệch (ĐỊNH LƯỢNG)
So PREDICTION.md với bảng được duyệt cuối cùng; định lượng từng chỗ lệch và nguồn gốc (ước lượng WCET sai,
quên tính thời gian ISR dùng chung, ...).

## Trả lời câu hỏi trade-off
Vì sao chọn priority/thứ tự ưu tiên như trong thiết kế được duyệt thay vì một hoán vị khác? Chứng minh bằng
chính bảng RTA của bạn rằng hoán vị khác sẽ làm ít nhất 1 task vi phạm deadline hoặc tốn RAM/CPU nhiều hơn.
