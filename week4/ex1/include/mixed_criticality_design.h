/* Design-time interface for the W4.1 mixed-criticality system. This header
 * holds ONLY the data shape of the RTA/ngan sach table you must fill in the
 * design doc before writing any W4.2 code - no task bodies, no drivers, no
 * ISR here. Reused verbatim (same field layout) when you actually create the
 * tasks in W4.2 and when you compare predicted vs measured in W4.3.
 * Budgets (jitter / RAM / CPU / baud) come from params.h
 * (W4E1_CONTROL_LOOP_JITTER_MAX_US, W4E1_RAM_BUDGET_BYTES,
 * W4E1_CPU_BUDGET_PERCENT_MAX, W4E1_TELEMETRY_BAUD_BPS). */
#ifndef W4E1_MIXED_CRITICALITY_DESIGN_H
#define W4E1_MIXED_CRITICALITY_DESIGN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One entry per task role. Fill W4E1_NUM_ROLES rows by hand in the design
 * doc appendix first (on paper / Markdown table), then transcribe here as a
 * static const array so W4.2/W4.3 can refer to the SAME numbers instead of
 * silently drifting from what was approved. */
typedef enum {
  W4E1_ROLE_CONTROL = 0,   /* 1 kHz, TIM-triggered, highest priority */
  W4E1_ROLE_IMU,           /* 500 Hz, I2C1 (W2.1) */
  W4E1_ROLE_TELEMETRY,     /* UART DMA TX (W3.1 style) */
  W4E1_ROLE_SHELL,         /* command shell, sporadic */
  W4E1_ROLE_IDLE_AUX,      /* tickless-idle bookkeeping / lowest priority */
  W4E1_NUM_ROLES
} w4e1_task_role_t;

/* Design-doc RTA row: period/deadline/WCET in microseconds, response time
 * computed BY HAND (classic fixed-priority RTA, not just Liu-Layland), and
 * the priority you are proposing for this role. */
typedef struct {
  w4e1_task_role_t role;
  uint32_t period_us;     /* 0 for sporadic (shell): use worst-case min
                             inter-arrival time instead */
  uint32_t deadline_us;   /* implicit deadline unless you justify otherwise */
  uint32_t wcet_us;       /* measured or honestly derived from W1-W3 data */
  uint32_t response_time_us; /* result of your hand RTA, must be <= deadline_us */
  uint32_t priority;      /* FreeRTOS task priority, higher = more urgent */
} w4e1_rta_entry_t;

/* Static-RAM budget row: one entry per task stack + one per shared static
 * buffer (I2C DMA buffer, UART DMA buffer, stream/message buffer backing
 * store, shell line buffer, ...). Sum of .bytes over every row must stay
 * under W4E1_RAM_BUDGET_BYTES. */
typedef struct {
  const char *name;
  uint32_t bytes;
} w4e1_ram_budget_entry_t;

/* Failure-mode row: for one shared resource, how is a failure DETECTED and
 * what is the CONTROLLED response (feeds mentor/week4/ex3 faults.md design
 * input - do not guess the exact fault list, just the general policy). */
typedef struct {
  const char *resource_name;   /* e.g. "I2C1", "UART1 DMA", "shared RAM" */
  const char *detection_method;
  const char *controlled_response;
} w4e1_failure_mode_entry_t;

#ifdef __cplusplus
}
#endif

#endif /* W4E1_MIXED_CRITICALITY_DESIGN_H */
