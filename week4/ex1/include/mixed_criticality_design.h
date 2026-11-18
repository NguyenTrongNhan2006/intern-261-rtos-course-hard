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
#include "params.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
#define W4E1_UNUSED __attribute__((unused))
#else
#define W4E1_UNUSED
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

/* Summary design constants (Single Source of Truth for W4.1 -> W4.4). */
#define W4E1_DESIGN_RAM_USED_BYTES       11904UL
#define W4E1_DESIGN_RAM_MARGIN_BYTES     (W4E1_RAM_BUDGET_BYTES - W4E1_DESIGN_RAM_USED_BYTES)
#define W4E1_DESIGN_CPU_LO_PERCENT       17UL  /* 16.89 % (5 tasks at C_LO), rounded up */
#define W4E1_DESIGN_CPU_WC_PERCENT       26UL  /* 25.59 % (5 tasks at C_HI), rounded up */
#define W4E1_DESIGN_CPU_TOTAL_WC_PERCENT 28UL  /* 27.70 % incl. 2.11 % ISR background */
#define W4E1_DESIGN_CONTROL_JITTER_US    97UL  /* = R_HI(control) <= 113 us, margin 16 us */
#define W4E1_DESIGN_JITTER_ALARM_US      100UL /* CPU_OVERLOAD early-warning, < 113 us hard limit */
#define W4E1_NUM_RAM_BUDGET_ENTRIES      18U
#define W4E1_NUM_FAILURE_MODE_ENTRIES    4U

/* Official hand-computed Worst-Case RTA table (5 roles, descending priority:
 * control=8, imu=7, telemetry=5, shell=3, idle_aux=2).
 * LO-mode nominal R_i: {70, 210, 390, 810, 1130} us.
 * HI-stress worst-case R_i (with I2C1 recovery & ISR overhead):
 * {97, 357, 537, 957, 1304} us. Every R_i <= D_i and R_control <= 113 us.
 * C_HI(control) = C_LO 70 + stale/safe-hold branch 11 + one peripheral ISR
 * preemption 16 = 97 us. */
static const w4e1_rta_entry_t W4E1_UNUSED W4E1_RTA_TABLE[W4E1_NUM_ROLES] = {
  /* role,                period_us, deadline_us, wcet_us, response_time_us, priority */
  { W4E1_ROLE_CONTROL,       1000UL,      1000UL,    97UL,             97UL,      8UL },
  { W4E1_ROLE_IMU,           2000UL,      2000UL,   260UL,            357UL,      7UL },
  { W4E1_ROLE_TELEMETRY,    10000UL,     10000UL,   180UL,            537UL,      5UL },
  { W4E1_ROLE_SHELL,        50000UL,    200000UL,   420UL,            957UL,      3UL },
  { W4E1_ROLE_IDLE_AUX,    100000UL,    100000UL,   250UL,           1304UL,      2UL }
};

/* Official 100% Static-RAM budget table: sum = 11,904 bytes <= 15,684 bytes
 * (W4E1_RAM_BUDGET_BYTES), leaving 3,780 bytes headroom. */
static const w4e1_ram_budget_entry_t W4E1_UNUSED
W4E1_RAM_BUDGET_TABLE[W4E1_NUM_RAM_BUDGET_ENTRIES] = {
  { "control_task_stack (192 words)",          768UL },
  { "imu_task_stack (192 words)",              768UL },
  { "telemetry_task_stack (192 words)",        768UL },
  { "shell_task_stack (256 words)",           1024UL },
  { "idle_aux_task_stack (192 words)",         768UL },
  { "freertos_idle_and_timer_stacks (2x128w)", 1024UL },
  { "static_task_tcbs (7x96B StaticTask_t)",   672UL },
  { "uart1_rx_dma_ring_buf (DMA1_Ch5)",        256UL },
  { "uart1_rx_stream_buf_storage (W3.1)",      512UL },
  { "uart1_tx_dma_pingpong_buf (DMA1_Ch4)",    256UL },
  { "i2c1_trx_and_recovery_ctx (W2.1)",         64UL },
  { "imu_spsc_snapshot_ring (16x32B W2.2)",    512UL },
  { "shell_cmd_and_reply_buf (2x128B)",        256UL },
  { "static_ipc_control_blocks (Sem/SB/EG)",   288UL },
  { "runtime_stats_and_dwt_trace_buf",        2304UL },
  { "noinit_postmortem_and_wdg_record (W3.3)", 128UL },
  { "kernel_and_system_globals (.data/.bss)",  512UL },
  { "msp_isr_stack_reserve (_Min_Stack_Size)", 1024UL }
};

/* Official Failure-Mode & Controlled-Degradation matrix covering the 4 fault
 * classes (SENSOR_UNPLUG, UART_FLOOD, CPU_OVERLOAD, TASK_HANG). */
static const w4e1_failure_mode_entry_t W4E1_UNUSED
W4E1_FAILURE_MODE_TABLE[W4E1_NUM_FAILURE_MODE_ENTRIES] = {
  {
    "I2C1 / MPU6050 Bus (SENSOR_UNPLUG)",
    "I2C1_ER_IRQn flags (AF/BERR/ARLO) or ulTaskNotifyTake timeout > 28 ms; control checks sample_tick staleness (> 2 ms)",
    "Set W4E3_BIT_DEGRADED_MODE_ENTERED; imu runs bounded 9-clock SCL recovery in task context; control uses safe-hold attitude without blocking"
  },
  {
    "USART1 RX + DMA1_Ch5 + StreamBuffer (UART_FLOOD)",
    "xStreamBufferSendFromISR returns < len (dropped > 0), USART1 ORE/FE/NE flags, or shell parse exceeds 200 ms timeout",
    "Set W4E3_BIT_DEGRADED_MODE_ENTERED; mask USART1 IDLEIE for 50 ms cooldown, flush corrupt frame, suspend shell & throttle telemetry"
  },
  {
    "CPU Time & TIM3 1kHz Jitter Budget (CPU_OVERLOAD)",
    "DWT cycle monitor sees control jitter > 100 us (alarm before the 113 us limit), WCRT > +12% RTA, or TIM2 100kHz runtime stats total CPU > 76%",
    "Enter HI-Criticality mode: shed LO-criticality tasks (suspend telemetry & shell, lock out STOP mode in idle_aux) to protect control & imu"
  },
  {
    "Task Liveness & Shared Static RAM (TASK_HANG)",
    "EventGroup heartbeat mask (active roles only; shed roles masked out in HI mode) incomplete within 100 ms window, stack high-water mark < 23 words, or HardFault/IWDG (> 296 ms)",
    "Log hung role ID, PC/LR/CFSR and reset reason into .noinit post-mortem record (canary 0x6C217697), trigger IWDG/NVIC reset within 296 ms"
  }
};

#ifdef __cplusplus
}
#endif

#endif /* W4E1_MIXED_CRITICALITY_DESIGN_H */
