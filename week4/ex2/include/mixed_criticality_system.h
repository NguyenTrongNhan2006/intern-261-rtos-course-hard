/* Interface for the W4.2 mixed-criticality system implementation. Task
 * bodies, TIM/I2C/UART register setup, and the tickless STOP-mode hook are
 * entirely yours to implement following the design approved in W4.1
 * (include/mixed_criticality_design.h in week4/ex1/) and your own drivers
 * from W2.1 (I2C), W3.1 (UART DMA), W3.2 (tickless). Priorities and timeouts
 * come from params.h (W4E2_TASK_PRIORITY, W4E2_SHELL_CMD_TIMEOUT_MS,
 * W4E2_STACK_MARGIN_TOLERANCE_WORDS). */
#ifndef W4E2_MIXED_CRITICALITY_SYSTEM_H
#define W4E2_MIXED_CRITICALITY_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Latest snapshot shared from imu_task to control_task. Fill in whatever
 * fields your MPU6050 driver (W2.1) actually produces; this is the minimal
 * shape the control loop needs. */
typedef struct {
  float accel_g[3];
  float gyro_dps[3];
  uint32_t sample_tick; /* xTaskGetTickCount() at acquisition, for staleness check */
} w4e2_imu_sample_t;

/* Runtime-tunable system state exposed to the shell (W4E2_SHELL role). */
typedef struct {
  uint32_t telemetry_period_ms; /* shell can change this at runtime */
  uint32_t control_overrun_count;
  uint32_t imu_stale_count; /* control loop ran with no fresh IMU sample */
} w4e2_system_status_t;

/* Call once before vTaskStartScheduler(): creates all W4E2_NUM_TASKS tasks
 * at the priorities in params.h, wires TIM compare interrupt for the 1 kHz
 * control trigger, and initializes shared static queues/notifications. */
void mixed_criticality_system_init(void);

/* Call from the TIM compare ISR driving the 1 kHz control loop: notifies
 * the control task. Must be safe to call from ISR context only. */
void mixed_criticality_system_on_control_tick(void);

/* Snapshot of runtime status for telemetry/shell/REPORT.md evidence. */
void mixed_criticality_system_get_status(w4e2_system_status_t *out);

/* Shell command entry point: parses one line (already framed, no newline),
 * executes it within W4E2_SHELL_CMD_TIMEOUT_MS, writes a reply into
 * reply_buf (caller-owned, reply_buf_len bytes). Returns true if the
 * command was recognized and executed. */
bool mixed_criticality_system_shell_exec(const char *cmd_line, char *reply_buf,
                                          uint32_t reply_buf_len);

#ifdef __cplusplus
}
#endif

#endif /* W4E2_MIXED_CRITICALITY_SYSTEM_H */
