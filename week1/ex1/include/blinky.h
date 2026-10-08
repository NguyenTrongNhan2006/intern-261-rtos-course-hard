/* Interface for the W1.1 blinky demo: two tasks toggling the per-student
 * tag GPIO pin (W1E1_TAG_GPIO_PORT/PIN in params.h) at their own periods
 * (W1E1_TASK_A/B_PERIOD_MS). Bodies and the clock/GPIO bring-up they depend
 * on are entirely yours to write - this header only fixes the shape main()
 * wires into the scheduler. */
#ifndef W1E1_BLINKY_H
#define W1E1_BLINKY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Task bodies - toggle the tag pin every period; vTaskDelay vs
 * xTaskDelayUntil is your choice, justify it quantitatively in REPORT.md. */
void vTaskBlinkA(void *pvParameters);
void vTaskBlinkB(void *pvParameters);

/* Called once from main(), after clock + GPIO bring-up and before
 * vTaskStartScheduler(): creates the two tasks above. */
void blinky_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* W1E1_BLINKY_H */
