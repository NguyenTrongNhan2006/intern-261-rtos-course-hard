/* Interface for the W3.2 RTC-alarm-based tickless idle (STOP mode).
 * Drift bound / test window come from params.h (W3E2_DRIFT_BOUND_PPM,
 * W3E2_DRIFT_TEST_WINDOW_MIN). RTC prescaler setup, the
 * portSUPPRESS_TICKS_AND_SLEEP() implementation itself, STOP-mode entry/
 * exit and the 72 MHz clock restore afterwards are entirely yours to
 * implement (wire this header's functions into port.c's
 * portSUPPRESS_TICKS_AND_SLEEP macro / your own override, per the
 * configUSE_TICKLESS_IDLE=2 application hook). */
#ifndef W3E2_TICKLESS_RTC_H
#define W3E2_TICKLESS_RTC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call once before vTaskStartScheduler(): configures RTC clock source
 * (LSE or LSI - document the choice and its tolerance in REPORT.md),
 * prescaler so 1 RTC second-interrupt tick maps to configTICK_RATE_HZ,
 * and the EXTI line used by the RTC alarm to wake from STOP. */
void tickless_rtc_init(void);

/* Application-supplied tickless idle hook: called by the port layer with
 * the number of ticks the scheduler says it is safe to sleep for
 * (already bounded by portMAX_DELAY handling upstream). Must: check
 * eTaskConfirmSleepModeStatus(), set an RTC alarm for
 * (xExpectedIdleTime - entry/exit overhead) ticks, stop SysTick, enter
 * STOP mode (PWR_CR + WFI), and on wake restore the 72 MHz clock and call
 * vTaskStepTick() with however many whole ticks actually elapsed. */
void tickless_suppress_ticks_and_sleep(uint32_t expected_idle_ticks);

/* Returns the cumulative drift (in RTC sub-ticks, signed) your bookkeeping
 * has accumulated since tickless_rtc_init() - used to cross-check against
 * the wall-clock-vs-PC drift test described in README.md. */
int32_t tickless_rtc_get_accumulated_drift(void);

#ifdef __cplusplus
}
#endif

#endif /* W3E2_TICKLESS_RTC_H */
