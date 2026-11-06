#ifndef W3E2_TICKLESS_RTC_H
#define W3E2_TICKLESS_RTC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* STEP 1: Khởi tạo và hiệu chuẩn RTC trước khi bộ lập lịch bắt đầu. */
void tickless_rtc_init(void);

/* STEP 2: Hook chạy trong Idle khi bộ lập lịch đang tạm ngưng.
 * expected_idle_ticks giới hạn số tick có thể cộng bù trước hạn tác vụ kế tiếp. */
void tickless_suppress_ticks_and_sleep(uint32_t expected_idle_ticks);

/* STEP 3: Đọc số liệu từ tác vụ đo. Counter có đơn vị bước RTC và có thể tràn.
 * late_wakes đếm số lần thời gian đo vượt khoảng tick được phép cộng bù. */
typedef struct {
    uint32_t rtc_cnt;
    uint32_t sleep_entries;
    uint32_t cal_freq_millihz;
    uint32_t rtc_faults;
    uint32_t late_wakes;
    uint32_t recalibrations;       /* Số lần đo lại tần số RTC giữa lúc chạy. */
    bool uses_lse;
} tickless_rtc_stats_t;

tickless_rtc_stats_t tickless_rtc_get_stats(void);

/* Phần dư quy đổi có đơn vị phần triệu chu kỳ lõi, luôn nhỏ hơn một chu kỳ.
 * Dùng để kiểm tra phép quy đổi, còn độ trôi 10 phút lấy từ tick so với máy tính. */
int32_t tickless_rtc_get_accumulated_drift(void);

#ifdef __cplusplus
}
#endif

#endif
