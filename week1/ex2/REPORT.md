# W1.2 – REPORT

## Số đo
- Gantt từ tracer (đính kèm ảnh xuất bởi `tools/trace_gantt.py`) và ảnh logic analyzer tương ứng.
- Bảng context switch:

| Optimization | min (cycle) | max (cycle) | jitter = max-min |
|---|---|---|---|
| -O0 | | | |
| -O2 | | | |

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- Sai lệch Gantt vs logic analyzer: ___ us, so với ngưỡng `W1E2_CORRELATION_TOLERANCE_US` trong PARAMS.md.
- Giải thích chênh lệch `-O0` vs `-O2` bằng assembly thực tế (đính kèm đoạn disassembly nếu cần).
- So `W1E2_CTXSW_JITTER_MAX_CYCLES_O2` (PARAMS.md) với jitter đo được ở `-O2`: đạt/không đạt, vì sao.

## Trả lời câu hỏi trade-off
Nguồn sai số nào (tick resolution, overhead ghi ring buffer, GPIO toggle) ảnh hưởng tracer nhiều nhất? Chứng minh bằng số đo của bạn.
