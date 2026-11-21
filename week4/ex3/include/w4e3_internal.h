#ifndef W4E3_INTERNAL_H
#define W4E3_INTERNAL_H

#include "FreeRTOS.h"
#include "fault_injection.h"
#include "mixed_criticality_system.h"
#include "task.h"
#include "../../ex1/include/mixed_criticality_design.h"

enum {
    W4E3_IO_NOTIFY_INDEX = 1u,
    W4E3_STATS_NOTIFY_INDEX = 2u,
    W4E3_RUNTIME_TICK_US = 10u,
    W4E3_UART_TX_BYTES = 128u,
    W4E3_ROLE_LOAD = W4E1_NUM_ROLES,
    W4E3_ROLE_UNKNOWN
};

typedef struct {
    char *next;
    size_t room;
    bool full;
} reply_writer_t;

void w4e3_append_text(reply_writer_t *writer, const char *text);
void w4e3_append_number(reply_writer_t *writer, uint32_t value);
void w4e3_format_wcrt(reply_writer_t *writer, bool lo);
void w4e3_format_runtime(reply_writer_t *writer);
void w4e3_wait_for_stats_window(void);
void w4e3_format_fault(reply_writer_t *writer);
const char *w4e3_role_name(uint32_t role);

void w4e3_verification_init(const TaskHandle_t *roles, TaskHandle_t load);
void w4e3_measure(uint32_t role, uint32_t cycles);
void w4e3_measure_imu_e2e(uint32_t cycles);
uint32_t w4e3_verification_collect(void);
uint32_t w4e3_stack_hwm(uint32_t role);
uint32_t w4e3_runtime_percent(uint32_t role);

void w4e3_control_clock_start(void);
void w4e3_control_clock_stop(void);
void w4e3_control_release_from_isr(uint32_t release, uint32_t release_runtime,
                                   uint32_t skipped, const uint32_t *frame, uint32_t exc_return);
uint32_t w4e3_control_misses(void);
uint32_t w4e3_current_role(void);
bool w4e3_system_allows_stop(void);
bool w4e3_system_running(void);
void w4e3_set_hi_mode(void);
void w4e3_leave_hi_mode(void);
void w4e3_start_load(bool high);
void w4e3_wake_aux(void);

bool w4e3_fault_request(w4e3_fault_kind_t kind, uint32_t role, bool high);
bool w4e3_fault_clear(void);
void w4e3_fault_observe(w4e3_fault_kind_t kind);
void w4e3_fault_observe_uart(uint32_t generation, uint32_t misses_at_start);
bool w4e3_fault_wait_step(TickType_t wait);
void w4e3_fault_check_hang(uint32_t role);
bool w4e3_fault_lo_load(void);
bool w4e3_fault_allows_stop(void);
bool w4e3_watchdog_allows_stop(void);
void w4e3_watchdog_check(uint32_t missing_mask);
void w4e3_fault_timer(const uint32_t *frame, uint32_t exc_return,
                       BaseType_t *woken);
void w4e3_fault_stack_overflow(uint32_t role) __attribute__((noreturn));

void w4e3_imu_init(void);
bool w4e3_imu_wake(void);
bool w4e3_imu_configure(void);
bool w4e3_imu_read(w4e2_imu_sample_t *sample);
void w4e3_imu_set_cut(bool cut);
bool w4e3_imu_is_cut(void);
uint32_t w4e3_imu_bus_wait_cycles(void);
bool w4e3_imu_busy(void);
uint32_t w4e3_imu_recoveries(void);
uint32_t w4e3_imu_recovery_pulses(void);

void w4e3_uart_init(void);
bool w4e3_uart_send(const void *data, size_t len, TickType_t wait);
size_t w4e3_uart_receive(uint8_t *byte, TickType_t wait);
bool w4e3_uart_cooling(void);
bool w4e3_uart_fault_active(void);
void w4e3_uart_bad_frame(void);
void w4e3_uart_clear_throttle(void);
uint32_t w4e3_uart_rx_faults(void);
uint32_t w4e3_uart_misses_at_cooldown(void);
uint32_t w4e3_uart_tx_errors(void);
bool w4e3_uart_prepare_stop(void);
void w4e3_uart_resume(bool rx_edge);

void w4e3_power_init(void);
uint32_t w4e3_stop_entries(void);
uint32_t w4e3_stop_faults(void);

#endif
