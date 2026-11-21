#ifndef W4E2_INTERNAL_H
#define W4E2_INTERNAL_H

#include "FreeRTOS.h"
#include "mixed_criticality_system.h"

enum { W4E2_IO_NOTIFY_INDEX = 1u, W4E2_UART_TX_BYTES = 128u };

void w4e2_control_clock_start(void);
void w4e2_control_clock_stop(void);
uint32_t w4e2_control_release_cycles(void);
uint32_t w4e2_control_missed_releases(void);
bool w4e2_system_allows_stop(void);

void w4e2_imu_init(void);
bool w4e2_imu_wake(void);
bool w4e2_imu_configure(void);
bool w4e2_imu_read(w4e2_imu_sample_t *sample);
bool w4e2_imu_busy(void);
uint32_t w4e2_imu_recoveries(void);
uint32_t w4e2_imu_recovery_pulses(void);

void w4e2_uart_init(void);
bool w4e2_uart_send(const void *data, size_t len, TickType_t wait);
size_t w4e2_uart_receive(uint8_t *byte, TickType_t wait);
bool w4e2_uart_cooling(void);
bool w4e2_uart_fault_active(void);
void w4e2_uart_bad_frame(void);
uint32_t w4e2_uart_rx_faults(void);
uint32_t w4e2_uart_tx_errors(void);
bool w4e2_uart_prepare_stop(void);
void w4e2_uart_resume(bool rx_edge);

void w4e2_power_init(void);
uint32_t w4e2_stop_entries(void);
uint32_t w4e2_stop_faults(void);

#endif
