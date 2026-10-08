/* Data shape for the one-page W4.4 defense summary sheet. This is filled by
 * hand from your own W4.1/W4.2/W4.3 results (design doc, build, verification
 * logs) - not code that runs on the MCU, just the structure the summary
 * sheet you bring to the defense must cover. Session shape comes from
 * params.h (W4E4_DEFENSE_DURATION_MIN, W4E4_UNKNOWN_FAULT_COUNT,
 * W4E4_MUTATION_TARGET_HINT). */
#ifndef W4E4_DEFENSE_SUMMARY_H
#define W4E4_DEFENSE_SUMMARY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One row per task role, final numbers from W4.1 (predicted) vs W4.3
 * (measured) - the first thing you should be able to recite without
 * looking anything up. */
typedef struct {
  const char *role_name;
  uint32_t wcrt_predicted_us;
  uint32_t wcrt_measured_us;
  uint32_t priority;
} w4e4_summary_row_t;

/* One row per fault scenario you verified in W4.3 - what happened, and
 * whether it was declared controlled degradation in the W4.1 failure-mode
 * table. */
typedef struct {
  const char *fault_name;
  uint8_t control_deadline_missed;    /* 0/1 */
  uint8_t was_controlled_degradation; /* 0/1, per W4.1 design */
} w4e4_fault_summary_row_t;

/* Top-level one-page summary: fill these by hand before the defense. */
typedef struct {
  uint32_t ram_budget_bytes;      /* W4E1_RAM_BUDGET_BYTES */
  uint32_t ram_used_bytes;        /* final measured */
  uint32_t cpu_budget_percent;    /* W4E1_CPU_BUDGET_PERCENT_MAX */
  uint32_t cpu_measured_percent;  /* final measured */
  uint32_t jitter_budget_us;      /* W4E1_CONTROL_LOOP_JITTER_MAX_US */
  uint32_t jitter_measured_us;    /* final measured, worst case */
} w4e4_defense_summary_t;

#ifdef __cplusplus
}
#endif

#endif /* W4E4_DEFENSE_SUMMARY_H */
