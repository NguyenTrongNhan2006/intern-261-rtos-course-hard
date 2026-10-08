/* Interface for the W4.3 verification + fault-injection harness. Trigger
 * bodies (how a task is actually hung, how UART is flooded, how I2C is cut,
 * how CPU is overloaded) and the watchdog/.noinit reset-reason bookkeeping
 * are entirely yours to implement, reusing W3.3 (.noinit post-mortem) and
 * W4.2's system (include/mixed_criticality_system.h in week4/ex2/).
 * Assigned scenario names and tolerances come from params.h
 * (W4E3_ASSIGNED_FAULT_NAMES, W4E3_WCRT_TOLERANCE_PERCENT,
 * W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS). */
#ifndef W4E3_FAULT_INJECTION_H
#define W4E3_FAULT_INJECTION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  W4E3_FAULT_TASK_HANG = 0,
  W4E3_FAULT_UART_FLOOD,
  W4E3_FAULT_SENSOR_UNPLUG,
  W4E3_FAULT_CPU_OVERLOAD,
  W4E3_FAULT_COUNT
} w4e3_fault_kind_t;

/* Event-group bits used to synchronize observation of degraded-mode entry
 * across tasks (xEventGroupWaitBits per the syllabus API requirement). */
#define W4E3_BIT_DEGRADED_MODE_ENTERED (1u << 0)
#define W4E3_BIT_WATCHDOG_RECOVERED    (1u << 1)

typedef struct {
  uint32_t wcrt_predicted_us; /* from W4.1 RTA, per task role */
  uint32_t wcrt_measured_us;
  uint32_t cpu_load_percent;
  uint32_t stack_high_water_mark_words;
} w4e3_verification_sample_t;

typedef struct {
  w4e3_fault_kind_t kind;
  bool control_deadline_missed;
  bool was_declared_controlled_degradation; /* per W4.1 failure-mode table */
  uint32_t watchdog_reset_reason_code;      /* 0 if no reset occurred */
} w4e3_fault_result_t;

/* Call once before vTaskStartScheduler(): configures IWDG with timeout
 * W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS and reads any pending reset-reason
 * record from .noinit (W3.3 style) into *out_prev_reason. */
void fault_injection_init(uint32_t *out_prev_reason);

/* Triggers one assigned fault scenario (must be one of
 * W4E3_ASSIGNED_FAULT_NAMES). Blocks the caller until either
 * W4E3_BIT_DEGRADED_MODE_ENTERED or the watchdog fires, whichever first. */
void fault_injection_trigger(w4e3_fault_kind_t kind);

/* Snapshot of the current WCRT/CPU/stack sample for one task role, for the
 * REPORT.md predicted-vs-measured table. */
void fault_injection_get_verification_sample(uint32_t role_index,
                                              w4e3_verification_sample_t *out);

/* Result of the most recently triggered fault scenario. */
void fault_injection_get_last_result(w4e3_fault_result_t *out);

#ifdef __cplusplus
}
#endif

#endif /* W4E3_FAULT_INJECTION_H */
