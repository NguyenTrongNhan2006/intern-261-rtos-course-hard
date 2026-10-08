/* Interface for the W1.3 RTA task set. Periods/deadlines/busy-wait times
 * come from params.h (W1E3_PERIOD_MS / W1E3_DEADLINE_MS / W1E3_BUSY_WAIT_US,
 * W1E3_NUM_TASKS entries, index 0..3). Bodies, priority assignment and the
 * WCRT measurement plumbing (reusing your W1.2 tracer) are yours to write. */
#ifndef W1E3_RTA_APP_H
#define W1E3_RTA_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* One periodic task body: busy-wait for W1E3_BUSY_WAIT_US[idx] (converted
 * to DWT cycles), then xTaskDelayUntil() to its own period
 * W1E3_PERIOD_MS[idx]. idx is passed as pvParameters. */
void vTaskPeriodic(void *pvParameters);

/* Creates the 4 tasks above with priorities assigned by Rate Monotonic
 * (shortest period = highest priority), and arms whatever WCRT measurement
 * you build on top of the W1.2 tracer. Call once before
 * vTaskStartScheduler(). */
void rta_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* W1E3_RTA_APP_H */
