# W2.3 – PREDICTION

MSSV **2452882**. Priority `[L,M1,M2,H] = [2,3,5,7]`; workload CPU trong vùng găng A/B là **19/15 ms**.

> Rà cách trình bày ngày 30/10/2026. Giữ mô hình và số dự đoán trong commit `e5c3234` ngày 23/10/2026; số đo sáu case nằm trong [REPORT.md](REPORT.md). Dự đoán bỏ qua overhead và sai số tick. Workload là thời gian CPU thực thi, không tính lúc bị preempt. M1 chỉ chạy báo cáo sau cửa sổ thử nghiệm.

## STEP 1 — Case 0: binary semaphore, không inheritance

L lấy A tại t=0 và cần 19 ms CPU. H yêu cầu A ở t=2 rồi Blocked. M2 không dùng A, chạy 30 ms từ t=4 đến t=34, preempt L. L đã làm 4 ms, còn 15 ms nên nhả A gần t=49. H dự kiến chờ **49−2=47 ms**.

Đây là lượt hữu hạn minh họa cơ chế inversion. Nếu workload task trung gian tiếp tục tăng hoặc chiếm CPU liên tục, thời gian H chờ có thể tăng mà không bị chặn bởi độ dài riêng vùng găng A.

## STEP 2 — Case 1: mutex inheritance, chained blocking

L giữ A từ t=0; M2 lấy B ở t=1. H yêu cầu A ở t=2, L kế thừa priority 7 và chạy nốt 18 ms, nhả A gần t=20. H lấy/nhả A rồi yêu cầu B; M2 kế thừa priority 7, chạy nốt khoảng 14 ms và nhả B gần t=34.

H dự kiến Blocked **hai lần nối tiếp**, khoảng **18+14=32 ms**. Inheritance giúp owner chạy nhưng không loại lần chờ một tài nguyên khác do owner khác giữ.

## STEP 3 — Case 2: thứ tự khóa chéo

L lấy A ở t=0. H lấy B ở t=2, làm 1 ms rồi yêu cầu A ở t=3 và Blocked. L kế thừa priority 7, đến khoảng t=4 yêu cầu B và cũng Blocked. Hai bên giữ khóa bên kia cần; `portMAX_DELAY` không tự phá vòng chờ.

Dự đoán không hoàn tất nếu không can thiệp. Idle và M1 vẫn có thể chạy. Quan sát hữu hạn kết hợp trạng thái khóa và code mới cho phép xác định circular wait; không dùng riêng khoảng trống Gantt làm bằng chứng.

## STEP 4 — Case 3/4: immediate priority ceiling

A/B có ceiling 7; tất cả người dùng đi qua wrapper. Tắt time slicing, không delay/yield khi giữ khóa, khóa lồng được nhả LIFO. L được nâng ngay lên 7 lúc lấy A. H Ready ở tick 2 có cùng effective priority với L, nên không giành CPU của L trong cấu hình này; M2 cũng không chen vào giữ B.

L nhả A gần t=19. H dự kiến chờ **17 ms ở Ready**, sau đó lấy khóa nhanh. Chỉ đo `xSemaphoreTake` có thể ra gần 0 dù H vẫn chịu chờ CPU. Với các vùng găng không lồng, blocking do một task thấp hơn được giới hạn bởi một vùng găng phù hợp, ở đây khoảng 19 ms cộng overhead.

## STEP 5 — Case 5: ceiling và khóa chéo

Giữ cùng thứ tự yêu cầu như case 2: L lấy A→B, H lấy B→A. L không bị H chen vào giữ B, nên không hình thành circular wait dưới các điều kiện STEP 4. Vùng găng ngoài của L bao trùm cả workload A và B, khoảng **34 ms**, nên không áp dụng mốc 19 ms của vùng găng không lồng. Với H Ready ở t=2, suy ra thời gian chờ khoảng **34−2=32 ms**; đây là phép triển khai phép tính 34 ms đã có trong bản dự đoán gốc, không lấy từ log.

H còn tự làm 1 ms CPU sau khi lấy B; thời gian đó thuộc thời gian hoàn tất của H, không phải thời gian H chờ khóa.

## Bảng dự đoán trước đo

| Case | Cơ chế | Khoảng chờ dự kiến | Hoàn tất? |
|---:|---|---|---|
| 0 | Binary semaphore | 1 lần Blocked A, khoảng 47 ms | Có nếu M2 dừng sau 30 ms |
| 1 | Mutex inheritance | 2 lần Blocked, A≈18 ms + B≈14 ms | Có |
| 2 | Mutex khóa chéo | H chờ A từ≈3 ms; vòng chờ hình thành≈4 ms | Không |
| 3 | Ceiling, inversion | 1 khoảng Ready≈17 ms; take A gần 0 | Có |
| 4 | Ceiling, chained | Ready≈17 ms; không có lần chờ B dài kế tiếp | Có |
| 5 | Ceiling, khóa chéo | Ready≈32 ms vì vùng găng lồng≈34 ms | Có |

`READY_TICKS` có độ phân giải 1 ms; `TAKE_*_US` dùng DWT và gồm overhead của lời gọi. `WAIT_US_EST` cộng hai nguồn nên vẫn là ước lượng khi có Ready delay. Phần này làm rõ cách đọc công cụ đo khi rà hồ sơ; không xem sai lệch vài µs ở cột tổng là độ chính xác µs của Ready delay.
