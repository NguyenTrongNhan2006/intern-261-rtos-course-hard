#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "w4e3_internal.h"

#define CORE_HZ 72000000u
#define CYCLES_PER_TICK (CORE_HZ / configTICK_RATE_HZ)
#define RTC_EXTI (1u << 17u)
#define RX_EXTI (1u << 10u)
#define STOP_TRACE_PIN 0u
#define RTC_FLAGS (RTC_CRL_RSF | RTC_CRL_OWF | RTC_CRL_ALRF | RTC_CRL_SECF)
#define RTC_TIMEOUT_CYCLES (CORE_HZ / 100u)
#define FIRST_EDGE_TIMEOUT_CYCLES (CORE_HZ + CORE_HZ / 5u)
#define WAKE_OVERHEAD_TICKS 3u
#define MAX_STOP_TICKS 80u
#define CALIBRATION_COUNTS 2048u

_Static_assert(configEXPECTED_IDLE_TIME_BEFORE_SLEEP > WAKE_OVERHEAD_TICKS,
               "STOP threshold must cover clock restart");

static uint32_t lsi_millihz;
static uint32_t stop_entries, rtc_faults, late_wakes;
static uint64_t rtc_remainder;

static bool wait_bits(volatile const uint32_t *reg, uint32_t mask,
                      uint32_t expected, uint32_t cycles)
{
    const uint32_t start = DWT->CYCCNT;
    while ((*reg & mask) != expected) {
        if (DWT->CYCCNT - start >= cycles) {
            return false;
        }
    }
    return true;
}

static bool rtc_wait_write(void)
{
    return wait_bits(&RTC->CRL, RTC_CRL_RTOFF, RTC_CRL_RTOFF, RTC_TIMEOUT_CYCLES);
}

static bool rtc_sync(void)
{
    if (!rtc_wait_write()) {
        return false;
    }
    RTC->CRL = RTC_FLAGS & ~RTC_CRL_RSF;
    return wait_bits(&RTC->CRL, RTC_CRL_RSF, RTC_CRL_RSF, RTC_TIMEOUT_CYCLES);
}

static uint32_t rtc_count(void)
{
    const uint32_t high = RTC->CNTH;
    uint32_t low = RTC->CNTL;
    const uint32_t checked_high = RTC->CNTH;
    if (high != checked_high) {
        low = RTC->CNTL;
    }
    return (checked_high << 16u) | low;
}

static uint32_t measure_lsi(void)
{
    const uint32_t initial = rtc_count();
    const uint32_t wait_start = DWT->CYCCNT;
    uint32_t first;
    do {
        first = rtc_count();
        if (DWT->CYCCNT - wait_start >= FIRST_EDGE_TIMEOUT_CYCLES) {
            return 0u;
        }
    } while (first == initial);

    const uint32_t start = DWT->CYCCNT;
    uint32_t delta;
    do {
        delta = rtc_count() - first;
        if (DWT->CYCCNT - start >= CORE_HZ / 8u) {
            return 0u;
        }
    } while (delta < CALIBRATION_COUNTS);
    /* SystemInit drives DWT from the 8 MHz HSE via PLL x9. */
    const uint32_t frequency = (CORE_HZ * 1000ULL * delta) / (DWT->CYCCNT - start);
    return frequency >= 29000000u && frequency <= 61000000u ? frequency : 0u;
}

static bool init_lsi(void)
{
    RCC->CSR |= RCC_CSR_LSION;
    if (!wait_bits(&RCC->CSR, RCC_CSR_LSIRDY, RCC_CSR_LSIRDY, RTC_TIMEOUT_CYCLES)) {
        return false;
    }
    const uint32_t source = RCC->BDCR & RCC_BDCR_RTCSEL;
    if (source != 0u && source != RCC_BDCR_RTCSEL_LSI) {
        RCC->BDCR |= RCC_BDCR_BDRST;
        RCC->BDCR &= ~RCC_BDCR_BDRST;
    }
    RCC->BDCR |= RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSI;
    if (!rtc_sync() || !rtc_wait_write()) {
        return false;
    }
    BKP->RTCCR &= ~BKP_RTCCR_CAL;
    RTC->CRL = RTC_FLAGS | RTC_CRL_CNF;
    /* A prescaler of one makes the 32-bit counter count raw LSI cycles. */
    RTC->PRLH = 0u;
    RTC->PRLL = 0u;
    RTC->CNTH = 0u;
    RTC->CNTL = 0u;
    RTC->CRL = RTC_FLAGS;
    if (!rtc_wait_write()) {
        return false;
    }
    lsi_millihz = measure_lsi();
    return lsi_millihz != 0u;
}

void w4e3_power_init(void)
{
    gpio_clock_enable(GPIOA);
    gpio_config_output_pp_2mhz(GPIOA, STOP_TRACE_PIN);
    gpio_set(GPIOA, STOP_TRACE_PIN);

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    (void)RCC->APB2ENR;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;
    if (!init_lsi()) {
        ++rtc_faults;
        return;
    }

    EXTI->IMR &= ~(RTC_EXTI | RX_EXTI);
    EXTI->EMR &= ~(RTC_EXTI | RX_EXTI);
    EXTI->FTSR = (EXTI->FTSR & ~RTC_EXTI) | RX_EXTI;
    EXTI->RTSR = (EXTI->RTSR & ~RX_EXTI) | RTC_EXTI;
    AFIO->EXTICR[2] &= ~AFIO_EXTICR3_EXTI10;
    RTC->CRH = RTC_CRH_ALRIE;
    if (rtc_wait_write()) {
        RTC->CRL = RTC_FLAGS & ~RTC_CRL_ALRF;
    }
    EXTI->PR = RTC_EXTI | RX_EXTI;
    NVIC_SetPriority(RTC_Alarm_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    NVIC_SetPriority(EXTI15_10_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    NVIC_EnableIRQ(RTC_Alarm_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void RTCAlarm_IRQHandler(void)
{
    if ((RTC->CRL & RTC_CRL_RTOFF) != 0u) {
        RTC->CRL = RTC_FLAGS & ~RTC_CRL_ALRF;
    }
    EXTI->PR = RTC_EXTI;
}

void EXTI15_10_IRQHandler(void)
{
    EXTI->PR = RX_EXTI;
}

static bool rtc_alarm(uint32_t target)
{
    if (!rtc_wait_write()) {
        return false;
    }
    RTC->CRL = (RTC_FLAGS & ~RTC_CRL_ALRF) | RTC_CRL_CNF;
    RTC->ALRH = target >> 16u;
    RTC->ALRL = target & 0xffffu;
    RTC->CRL = RTC_FLAGS;
    return rtc_wait_write();
}

static void restore_clock(void)
{
    const uint32_t timeout = 800000u;
    RCC->CR |= RCC_CR_HSEON;
    configASSERT(wait_bits(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY, timeout));
    RCC->CR |= RCC_CR_PLLON;
    configASSERT(wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY, timeout));
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    configASSERT(wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, timeout));
    __DSB();
    __ISB();
}

static void shallow_sleep(void)
{
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    __DSB();
    __WFI();
    __ISB();
}

void vApplicationIdleHook(void)
{
    /* The kernel reaches the tickless hook only at or above five idle ticks. */
    shallow_sleep();
}

static void restart_systick(uint32_t control, uint32_t cycles)
{
    if (cycles < 32u) {
        cycles = 32u;
    }
    SysTick->LOAD = cycles - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = control;
    while (SysTick->VAL == 0u) {
        __NOP();
    }
    SysTick->LOAD = CYCLES_PER_TICK - 1u;
}

static void stop_with_alarm(TickType_t expected)
{
    const TickType_t idle = expected < MAX_STOP_TICKS ? expected : MAX_STOP_TICKS;
    const uint32_t systick_control = SysTick->CTRL;
    SysTick->CTRL = systick_control & ~SysTick_CTRL_ENABLE_Msk;
    const uint32_t frozen_at = DWT->CYCCNT;
    const uint32_t remaining = SysTick->VAL;
    if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0u) {
        SysTick->CTRL = systick_control;
        return;
    }

    const uint32_t phase = remaining != 0u ? CYCLES_PER_TICK - remaining : 0u;
    const uint32_t before = rtc_count();
    const uint32_t before_cycles = DWT->CYCCNT;
    const uint64_t budget = (uint64_t)(idle - WAKE_OVERHEAD_TICKS) * CYCLES_PER_TICK;
    const uint32_t used = phase + (before_cycles - frozen_at);
    const uint32_t alarm_delta = used < budget ?
        ((budget - used) * lsi_millihz) / (CORE_HZ * 1000ULL) : 0u;
    bool stopped = false;
    if (alarm_delta != 0u) {
        EXTI->PR = RTC_EXTI;
        NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
        if (!rtc_alarm(before + alarm_delta)) {
            lsi_millihz = 0u;
            ++rtc_faults;
        } else if (rtc_count() - before < alarm_delta) {
            EXTI->IMR |= RTC_EXTI;
            PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS | PWR_CR_CWUF;
            SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
            gpio_clear(GPIOA, STOP_TRACE_PIN);
            __DSB();
            __WFI();
            __ISB();
            SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
            stopped = (RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL;
            if (!stopped) {
                gpio_set(GPIOA, STOP_TRACE_PIN);
            }
        }
    }

    uint64_t elapsed = phase;
    if (stopped) {
        ++stop_entries;
        restore_clock();
        gpio_set(GPIOA, STOP_TRACE_PIN);
        if (rtc_sync()) {
            const uint32_t counts = rtc_count() - before;
            const uint32_t after_cycles = DWT->CYCCNT;
            const uint64_t scaled = counts * (CORE_HZ * 1000ULL) + rtc_remainder;
            elapsed += (before_cycles - frozen_at) + scaled / lsi_millihz;
            rtc_remainder = scaled % lsi_millihz;
            elapsed += DWT->CYCCNT - after_cycles;
        } else {
            if ((EXTI->PR & RTC_EXTI) != 0u) {
                elapsed = budget;
            }
            lsi_millihz = 0u;
            ++rtc_faults;
        }
    } else {
        elapsed += DWT->CYCCNT - frozen_at;
    }

    EXTI->IMR &= ~RTC_EXTI;
    (void)rtc_wait_write();
    RTCAlarm_IRQHandler();
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    TickType_t ticks = (TickType_t)(elapsed / CYCLES_PER_TICK);
    if (ticks > expected) {
        ++late_wakes;
        ticks = expected;
    }
    if (ticks != 0u) {
        vTaskStepTick(ticks);
    }
    restart_systick(systick_control, CYCLES_PER_TICK - (uint32_t)(elapsed % CYCLES_PER_TICK));
}

void w4e3_sleep(uint32_t expected_idle_ticks)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DSB();
    __ISB();
    if (eTaskConfirmSleepModeStatus() != eAbortSleep) {
        bool stopped_rx = false;
        if (lsi_millihz != 0u && expected_idle_ticks >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP &&
            w4e3_system_allows_stop() && w4e3_watchdog_allows_stop() && (TIM3->CR1 & TIM_CR1_CEN) == 0u && !w4e3_imu_busy()) {
            /* PA10 wakes an explicitly quiesced console. The wake byte is discarded. */
            EXTI->PR = RX_EXTI;
            NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
            EXTI->IMR |= RX_EXTI;
            stopped_rx = w4e3_uart_prepare_stop();
        }
        if (stopped_rx) {
            stop_with_alarm(expected_idle_ticks);
            w4e3_uart_resume((EXTI->PR & RX_EXTI) != 0u);
        } else {
            shallow_sleep();
        }
        EXTI->IMR &= ~RX_EXTI;
        EXTI15_10_IRQHandler();
        NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    }
    __set_PRIMASK(primask);
}

uint32_t w4e3_stop_entries(void)
{
    return stop_entries;
}

uint32_t w4e3_stop_faults(void)
{
    return rtc_faults + late_wakes;
}
