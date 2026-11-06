#include "FreeRTOS.h"
#include "task.h"
#include "tickless_rtc.h"
#include "gpio.h"

#ifndef W3E2_TRY_LSE
#define W3E2_TRY_LSE 0
#endif

#define CORE_HZ                72000000u
#define TICK_CYCLES            (CORE_HZ / configTICK_RATE_HZ)
#define WAKEUP_OVERHEAD_TICKS  3u
#define MAX_STOP_TICKS         1000u
#define CAL_COUNTS             1024u
#define CAL_DIVISOR            40u
#define RTC_TIMEOUT_CYCLES     (CORE_HZ / 100u)
#define CLOCK_TIMEOUT_CYCLES   800000u
#define RTC_EXTI_LINE          (1u << 17)
#define RTC_STATUS_FLAGS       (RTC_CRL_RSF | RTC_CRL_OWF | RTC_CRL_ALRF | RTC_CRL_SECF)
#define TRACE_PIN              0u
#define SYSTICK_MIN_CYCLES     32u
#define MAX_CARRY_CYCLES       (MAX_STOP_TICKS * TICK_CYCLES)
/* LSI là dao động RC nên tần số đổi theo nhiệt độ sau lần đo lúc khởi động. Cứ
 * RECAL_EVERY_SLEEPS lần vào STOP, một lần ngủ được thay bằng phép đo RECAL_COUNTS
 * bước RTC (khoảng 0,26 s với bước 1 ms). Cần khoảng rảnh dài hơn phép đo này. */
#define RECAL_EVERY_SLEEPS     60u
#define RECAL_COUNTS           256u
#define RECAL_MIN_IDLE_TICKS   300u
#define RECAL_TIMEOUT_CYCLES   (CORE_HZ / 2u)

/* Ngân sách báo thức trừ WAKEUP_OVERHEAD_TICKS, nên ngưỡng ngủ phải lớn hơn để không tràn số. */
_Static_assert(configEXPECTED_IDLE_TIME_BEFORE_SLEEP > WAKEUP_OVERHEAD_TICKS,
               "Nguong ngu phai lon hon so tick du phong luc thuc day");

typedef struct {
    uint32_t count;
    uint32_t phase;
    uint32_t core_cycles;
} rtc_sample_t;

static tickless_rtc_stats_t s_stats;
static uint32_t s_rtc_divisor;
static uint64_t s_subtick_rem;
static uint32_t s_carry_cycles;
static uint32_t s_sleeps_since_recal;
static bool s_rtc_ready;

/* STEP 1: Giới hạn thời gian chờ bằng bộ đếm chu kỳ DWT. Khi vừa thoát STOP,
 * ngưỡng chờ khôi phục xung được đặt theo số chu kỳ chạy bằng HSI 8 MHz. */
static bool wait_mask(volatile const uint32_t *reg, uint32_t mask, uint32_t value,
                      uint32_t timeout_cycles)
{
    const uint32_t start = DWT->CYCCNT;
    while ((*reg & mask) != value) {
        if ((uint32_t)(DWT->CYCCNT - start) >= timeout_cycles) {
            return false;
        }
    }
    return true;
}

/* STEP 2: Chờ đồng bộ giữa miền RTC và bus. Các cờ trạng thái RTC_CRL xóa
 * bằng cách ghi 0 nên ghi 1 vào các cờ cần giữ để tránh xóa nhầm sự kiện mới. */
static bool rtc_wait_rtoff(void)
{
    return wait_mask(&RTC->CRL, RTC_CRL_RTOFF, RTC_CRL_RTOFF, RTC_TIMEOUT_CYCLES);
}

static bool rtc_wait_rsf(void)
{
    if (!rtc_wait_rtoff()) {
        return false;
    }
    RTC->CRL = RTC_STATUS_FLAGS & ~RTC_CRL_RSF;
    return wait_mask(&RTC->CRL, RTC_CRL_RSF, RTC_CRL_RSF, RTC_TIMEOUT_CYCLES);
}

static bool rtc_begin_write(void)
{
    if (!rtc_wait_rtoff()) {
        return false;
    }
    RTC->CRL = RTC_STATUS_FLAGS | RTC_CRL_CNF;
    return true;
}

static bool rtc_end_write(void)
{
    RTC->CRL = RTC_STATUS_FLAGS;
    return rtc_wait_rtoff();
}

static bool rtc_set_divisor(uint32_t divisor)
{
    if (!rtc_begin_write()) {
        return false;
    }
    /* Giá trị nạp dưới 64, do đó các bit cao của bộ chia đều bằng 0. */
    RTC->PRLH = 0u;
    RTC->PRLL = divisor - 1u;
    if (!rtc_end_write()) {
        return false;
    }
    s_rtc_divisor = divisor;
    return true;
}

static bool rtc_set_alarm(uint32_t target)
{
    if (!rtc_begin_write()) {
        return false;
    }
    RTC->ALRH = target >> 16u;
    RTC->ALRL = target & 0xffffu;
    return rtc_end_write();
}

static uint32_t rtc_read_counter(void)
{
    const uint32_t hi1 = RTC->CNTH;
    uint32_t lo = RTC->CNTL;
    const uint32_t hi2 = RTC->CNTH;
    if (hi1 != hi2) {
        lo = RTC->CNTL;
    }
    return (hi2 << 16u) | lo;
}

static bool rtc_read_sample(rtc_sample_t *sample)
{
    const uint32_t start = DWT->CYCCNT;
    do {
        /* Đọc bộ chia trước và sau bộ đếm để loại mẫu cắt ngang cạnh nạp lại. */
        const uint32_t div_before = RTC->DIVL;
        const uint32_t count = rtc_read_counter();
        const uint32_t div_after = RTC->DIVL;
        if (div_before == div_after && div_after < s_rtc_divisor) {
            *sample = (rtc_sample_t){count, s_rtc_divisor - 1u - div_after, DWT->CYCCNT};
            return true;
        }
    } while ((uint32_t)(DWT->CYCCNT - start) < RTC_TIMEOUT_CYCLES);
    return false;
}

static void rtc_ack_alarm(void)
{
    RTC->CRL = RTC_STATUS_FLAGS & ~RTC_CRL_ALRF;
    EXTI->PR = RTC_EXTI_LINE;
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
}

void RTCAlarm_IRQHandler(void)
{
    /* Nếu ngắt đến khi đang chạy, chỉ xác nhận báo thức, không chạm bộ lập lịch. */
    if ((RTC->CRL & RTC_CRL_RTOFF) != 0u) {
        rtc_ack_alarm();
    }
}

/* STEP 3: Chọn nguồn RTC, hiệu chuẩn qua HSE nhân 9 và nạp lại hệ số chia.
 * Mặc định dùng LSI. Tùy chọn LSE có thời hạn chờ và tự rơi về LSI. */
static bool rtc_select_source(void)
{
    uint32_t source = RCC_BDCR_RTCSEL_LSI;
#if W3E2_TRY_LSE
    RCC->BDCR |= RCC_BDCR_LSEON;
    if (wait_mask(&RCC->BDCR, RCC_BDCR_LSERDY, RCC_BDCR_LSERDY, 4u * CORE_HZ)) {
        source = RCC_BDCR_RTCSEL_LSE;
    }
#endif
    const uint32_t previous = RCC->BDCR & RCC_BDCR_RTCSEL;
    if (previous != 0u && previous != source) {
        /* W3.2 sở hữu RTC. Chỉ reset miền dự phòng khi đổi nguồn đã khóa. */
        RCC->BDCR |= RCC_BDCR_BDRST;
        RCC->BDCR &= ~RCC_BDCR_BDRST;
    }
    if (source == RCC_BDCR_RTCSEL_LSE) {
        RCC->BDCR |= RCC_BDCR_LSEON;
        if (!wait_mask(&RCC->BDCR, RCC_BDCR_LSERDY, RCC_BDCR_LSERDY, 4u * CORE_HZ)) {
            source = RCC_BDCR_RTCSEL_LSI;
        }
    }
    if (source == RCC_BDCR_RTCSEL_LSI) {
        RCC->BDCR &= ~RCC_BDCR_LSEON;
        RCC->CSR |= RCC_CSR_LSION;
        if (!wait_mask(&RCC->CSR, RCC_CSR_LSIRDY, RCC_CSR_LSIRDY, RTC_TIMEOUT_CYCLES)) {
            return false;
        }
    }

    RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | source | RCC_BDCR_RTCEN;
    s_stats.uses_lse = source == RCC_BDCR_RTCSEL_LSE;
    return rtc_wait_rsf();
}

/* Đo tần số nguồn RTC bằng DWT trong lúc chạy ở 72 MHz. Bắt hai cạnh tăng cách
 * nhau đúng counts bước, không lấy mốc đầu ở giữa chu kỳ RTC. Giữ milli-hertz để
 * giảm lượng tử tần số. divisor là hệ số chia đang nạp trong RTC_PRL. */
static bool rtc_measure_millihz(uint32_t counts, uint32_t divisor, uint32_t timeout_cycles,
                                uint32_t *freq_millihz)
{
    const uint32_t initial = rtc_read_counter();
    const uint32_t wait_start = DWT->CYCCNT;
    uint32_t first;
    do {
        first = rtc_read_counter();
        if ((uint32_t)(DWT->CYCCNT - wait_start) >= RTC_TIMEOUT_CYCLES) {
            return false;
        }
    } while (first == initial);

    const uint32_t start = DWT->CYCCNT;
    uint32_t elapsed_counts;
    do {
        elapsed_counts = (uint32_t)(rtc_read_counter() - first);
        if ((uint32_t)(DWT->CYCCNT - start) >= timeout_cycles) {
            return false;
        }
    } while (elapsed_counts < counts);

    const uint32_t elapsed_cycles = DWT->CYCCNT - start;
    if (elapsed_counts != counts || elapsed_cycles == 0u) {
        return false;
    }
    const uint64_t numerator = CORE_HZ * 1000ULL * counts * divisor;
    const uint64_t freq = (numerator + elapsed_cycles / 2u) / elapsed_cycles;
    if (freq < 29000000u || freq > 61000000u) {
        return false;
    }
    *freq_millihz = (uint32_t)freq;
    return true;
}

static bool rtc_calibrate(void)
{
    if (!rtc_set_divisor(CAL_DIVISOR)) {
        return false;
    }
    uint32_t freq_millihz;
    if (!rtc_measure_millihz(CAL_COUNTS, CAL_DIVISOR, 2u * CORE_HZ, &freq_millihz)) {
        return false;
    }

    s_stats.cal_freq_millihz = freq_millihz;
    const uint32_t divisor = s_stats.uses_lse ? 32u :
        (uint32_t)(freq_millihz / (1000u * configTICK_RATE_HZ));
    return rtc_set_divisor(divisor);
}

/* Đổi tần số hiệu chuẩn giữa lúc chạy. Phần dư sub-tick có đơn vị theo mẫu số là
 * tần số, nên đổi tỷ lệ để không làm lệch thời gian đang tích lũy. */
static void rtc_apply_frequency(uint32_t freq_millihz)
{
    s_subtick_rem = s_subtick_rem * freq_millihz / s_stats.cal_freq_millihz;
    s_stats.cal_freq_millihz = freq_millihz;
    ++s_stats.recalibrations;
}

void tickless_rtc_init(void)
{
    configASSERT(SystemCoreClock == CORE_HZ);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    configASSERT(DWT->CYCCNT != 0u);

    gpio_clock_enable(GPIOA);
    gpio_config_output_pp_2mhz(GPIOA, TRACE_PIN);
    gpio_set(GPIOA, TRACE_PIN);

    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;

    if (!rtc_select_source()) {
        ++s_stats.rtc_faults;
        return;
    }
    /* Bỏ giá trị hiệu chỉnh phần cứng cũ để tỷ lệ đếm do phần mềm kiểm soát. */
    BKP->RTCCR &= ~BKP_RTCCR_CAL;
    if (!rtc_calibrate()) {
        ++s_stats.rtc_faults;
        return;
    }

    EXTI->IMR &= ~RTC_EXTI_LINE;
    EXTI->EMR &= ~RTC_EXTI_LINE;
    EXTI->FTSR &= ~RTC_EXTI_LINE;
    EXTI->RTSR |= RTC_EXTI_LINE;
    RTC->CRH = RTC_CRH_ALRIE;
    rtc_ack_alarm();
    NVIC_SetPriority(RTC_Alarm_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(RTC_Alarm_IRQn);
    s_rtc_ready = true;
}

/* STEP 4: Sau STOP, HSI 8 MHz đã chạy. Cây chia bus và Flash giữ cấu hình
 * từ SystemInit. Khóa ngắt đến khi đã chọn lại PLL 72 MHz. */
static void restore_core_clock(void)
{
    RCC->CR |= RCC_CR_HSEON;
    configASSERT(wait_mask(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY, CLOCK_TIMEOUT_CYCLES));
    RCC->CR |= RCC_CR_PLLON;
    configASSERT(wait_mask(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY, CLOCK_TIMEOUT_CYCLES));
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    configASSERT(wait_mask(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, CLOCK_TIMEOUT_CYCLES));
    SystemCoreClock = CORE_HZ;
    __DSB();
    __ISB();
}

/* STEP 5: Tính thời gian bằng chênh lệch bước đếm và pha của bộ chia.
 * Tích lũy phần lẻ dưới một chu kỳ lõi, rồi nạp khoảng lẻ tới tick kế tiếp
 * vào SysTick. Hai phần lẻ có đơn vị khác nhau và được tính đúng một lần. */
static uint64_t rtc_elapsed_core_cycles(const rtc_sample_t *before, const rtc_sample_t *after)
{
    const uint32_t counts = after->count - before->count;
    const int64_t input = (int64_t)counts * s_rtc_divisor + after->phase - before->phase;
    const uint64_t scaled = (uint64_t)input * configTICK_RATE_HZ * 1000u;
    const uint32_t freq = s_stats.cal_freq_millihz;
    const uint64_t full_ticks = scaled / freq;
    const uint64_t partial = (scaled % freq) * TICK_CYCLES + s_subtick_rem;
    s_subtick_rem = partial % freq;
    return full_ticks * TICK_CYCLES + partial / freq;
}

static void restart_systick(uint32_t ctrl, uint32_t cycles_left)
{
    if (cycles_left < SYSTICK_MIN_CYCLES) {
        cycles_left = SYSTICK_MIN_CYCLES;
    }
    SysTick->LOAD = cycles_left - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = ctrl;
    /* Xác nhận khoảng ngắn đã vào bộ đếm trước khi trả LOAD về chu kỳ 1 ms.
     * Chỉ bật bộ đếm với xung lõi như cấu hình port Cortex-M3 của bài này. */
    while (SysTick->VAL == 0u) {
    }
    SysTick->LOAD = TICK_CYCLES - 1u;
}

/* STEP 6: Luồng nguyên tử khi bộ lập lịch đang tạm ngưng. Xóa báo thức cũ
 * trước khi lập lịch báo thức mới. Không xóa sự kiện vừa phát sinh trước WFI. */
void tickless_suppress_ticks_and_sleep(uint32_t expected_idle_ticks)
{
    if (!s_rtc_ready || expected_idle_ticks < configEXPECTED_IDLE_TIME_BEFORE_SLEEP) {
        return;
    }
    const uint32_t idle_ticks = expected_idle_ticks < MAX_STOP_TICKS ?
        expected_idle_ticks : MAX_STOP_TICKS;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DSB();
    __ISB();

    /* Chờ bit Stop cuối của UART1 trong khi SysTick vẫn đếm. */
    if (!wait_mask(&USART1->SR, USART_SR_TC, USART_SR_TC, CORE_HZ / 500u)) {
        __set_PRIMASK(primask);
        return;
    }
    const uint32_t ctrl = SysTick->CTRL;
    SysTick->CTRL = ctrl & ~SysTick_CTRL_ENABLE_Msk;
    const uint32_t stop_cycles = DWT->CYCCNT;
    const uint32_t tick_left = SysTick->VAL;
    if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0u ||
        eTaskConfirmSleepModeStatus() == eAbortSleep) {
        /* Giữ VAL và tick đang chờ để ngắt SysTick xử lý sau khi mở khóa. */
        SysTick->CTRL = ctrl;
        __set_PRIMASK(primask);
        return;
    }

    const uint32_t tick_phase = tick_left == 0u ? 0u : TICK_CYCLES - tick_left;
    rtc_sample_t before;
    rtc_sample_t after;
    bool stopped = false;
    const bool recal = s_sleeps_since_recal >= RECAL_EVERY_SLEEPS &&
                       idle_ticks >= RECAL_MIN_IDLE_TICKS;
    if (recal) {
        /* Lượt này không vào STOP: đo lại LSI bằng DWT (HSE) rồi bù thời gian đã chạy
         * bằng đường !stopped bên dưới. Nếu đo hỏng thì giữ tần số cũ và ghi lỗi. */
        uint32_t freq_millihz;
        if (rtc_measure_millihz(RECAL_COUNTS, s_rtc_divisor, RECAL_TIMEOUT_CYCLES,
                                &freq_millihz)) {
            rtc_apply_frequency(freq_millihz);
        } else {
            ++s_stats.rtc_faults;
        }
        s_sleeps_since_recal = 0u;
    }
    bool rtc_ok = recal || rtc_read_sample(&before);
    if (rtc_ok && !recal) {
        const uint32_t budget = (idle_ticks - WAKEUP_OVERHEAD_TICKS) * TICK_CYCLES;
        const uint32_t used = tick_phase + (uint32_t)(before.core_cycles - stop_cycles);
        const uint32_t budget_cycles = used < budget ? budget - used : 0u;
        const uint32_t raw_steps = (uint32_t)((uint64_t)budget_cycles * s_stats.cal_freq_millihz /
                                              (CORE_HZ * 1000ULL));
        const uint32_t alarm_steps = (before.phase + raw_steps) / s_rtc_divisor;
        if (alarm_steps != 0u) {
            rtc_ack_alarm();
            rtc_ok = rtc_set_alarm(before.count + alarm_steps);
            if (rtc_ok) {
                PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS | PWR_CR_CWUF;
                EXTI->IMR |= RTC_EXTI_LINE;
                /* Nếu mốc đã qua trong lúc ghi, tiếp tục chạy và bù thời gian
                 * thiết lập. Báo thức mới còn pending sẽ đánh thức WFI ngay. */
                if ((uint32_t)(rtc_read_counter() - before.count) < alarm_steps) {
                    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
                    gpio_clear(GPIOA, TRACE_PIN);
                    __DSB();
                    __WFI();
                    __ISB();
                    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

                    stopped = (RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL;
                    if (stopped) {
                        restore_core_clock();
                        ++s_stats.sleep_entries;
                        ++s_sleeps_since_recal;
                    }
                    gpio_set(GPIOA, TRACE_PIN);
                    rtc_ok = !stopped || rtc_wait_rsf();
                }
            }
        }
    }

    EXTI->IMR &= ~RTC_EXTI_LINE;
    if (rtc_ok) {
        rtc_ack_alarm();
        rtc_ok = !stopped || rtc_read_sample(&after);
    }
    uint64_t total_cycles = tick_phase;
    if (!rtc_ok) {
        ++s_stats.rtc_faults;
        s_rtc_ready = false;
        /* Sau lỗi RTC trong STOP, thời gian ngủ không còn suy ra được. */
    }
    if (!stopped) {
        /* Khi chưa STOP, DWT chạy liên tục ở 72 MHz trong lúc chuẩn bị. */
        total_cycles += (uint32_t)(DWT->CYCCNT - stop_cycles);
    } else if (rtc_ok) {
        total_cycles += (uint32_t)(before.core_cycles - stop_cycles);
        total_cycles += rtc_elapsed_core_cycles(&before, &after);
        total_cycles += (uint32_t)(DWT->CYCCNT - after.core_cycles);
    }

    /* vTaskStepTick không được vượt mốc tác vụ kế tiếp. Phần tick thức muộn được
     * giữ lại và cộng vào lần ngủ sau, thay vì bỏ đi làm tick chạy chậm hơn giờ thật. */
    total_cycles += s_carry_cycles;
    s_carry_cycles = 0u;
    uint64_t completed_ticks = total_cycles / TICK_CYCLES;
    if (completed_ticks > expected_idle_ticks) {
        ++s_stats.late_wakes;
        const uint64_t excess = (completed_ticks - expected_idle_ticks) * TICK_CYCLES;
        s_carry_cycles = excess < MAX_CARRY_CYCLES ? (uint32_t)excess : MAX_CARRY_CYCLES;
        completed_ticks = expected_idle_ticks;
    }
    if (completed_ticks != 0u) {
        vTaskStepTick((TickType_t)completed_ticks);
    }
    restart_systick(ctrl, TICK_CYCLES - (uint32_t)(total_cycles % TICK_CYCLES));
    __set_PRIMASK(primask);
}

/* STEP 7: Xuất số liệu phục vụ cửa sổ đo, độc lập với mốc tick của nhân. */
tickless_rtc_stats_t tickless_rtc_get_stats(void)
{
    tickless_rtc_stats_t stats = s_stats;
    if (s_rtc_ready) {
        stats.rtc_cnt = rtc_read_counter();
    }
    return stats;
}

int32_t tickless_rtc_get_accumulated_drift(void)
{
    if (s_stats.cal_freq_millihz == 0u) {
        return 0;
    }
    return (int32_t)(s_subtick_rem * 1000000u / s_stats.cal_freq_millihz);
}
