/* Interface for the W2.1 interrupt-driven I2C1 driver + bus recovery.
 * Speed / timeout / recovery repeat count come from params.h
 * (W2E1_I2C_SPEED_HZ, W2E1_I2C_TIMEOUT_MS, W2E1_RECOVERY_REPEAT_COUNT).
 * Register-level ISR sequencing (EV5..EV8), NVIC priority setup and the
 * SCL-toggle bus-recovery procedure are entirely yours to implement. */
#ifndef W2E1_I2C1_DRIVER_H
#define W2E1_I2C1_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  I2C1_OK = 0,
  I2C1_ERR_TIMEOUT,
  I2C1_ERR_NACK,
  I2C1_ERR_BUS,      /* arbitration lost / bus error flags (BERR/ARLO) */
  I2C1_ERR_RECOVERED /* transaction failed but bus recovery succeeded */
} i2c1_status_t;

/* Call once before vTaskStartScheduler(): configures GPIO AF open-drain,
 * CR2/CCR/TRISE for W2E1_I2C_SPEED_HZ, enables event/error/buffer
 * interrupts and NVIC (priority below
 * configMAX_SYSCALL_INTERRUPT_PRIORITY), and initializes the internal
 * static mutex guarding the bus. */
void i2c1_driver_init(void);

/* Blocking (task-context) master write: START, address+W, write len bytes
 * from data, STOP. Internally waits on ulTaskNotifyTake() for the ISR to
 * signal completion/error, bounded by W2E1_I2C_TIMEOUT_MS. On timeout or a
 * bus-stuck condition, triggers i2c1_bus_recover() before returning. Must
 * take the internal mutex for the whole transaction (2 client tasks share
 * the bus). */
i2c1_status_t i2c1_master_write(uint8_t addr7, const uint8_t *data, uint16_t len);

/* Blocking (task-context) master read: START, address+R, read len bytes
 * into data (NACK on the last byte), STOP. Same notification/timeout/
 * recovery/mutex rules as i2c1_master_write(). */
i2c1_status_t i2c1_master_read(uint8_t addr7, uint8_t *data, uint16_t len);

/* Manual bus recovery per the F103 I2C errata: switch SCL/SDA to plain
 * GPIO, pulse SCL up to 9 times while SDA stays low, issue a STOP
 * condition, then restore I2C1 alternate-function mode. Returns true if
 * SDA was observed high (bus free) afterwards. Called internally on
 * timeout/bus-stuck, but exposed here so the recovery test harness can
 * invoke it directly and count successes. */
bool i2c1_bus_recover(void);

/* Call from I2C1_EV_IRQHandler() / I2C1_ER_IRQHandler(): drive the EV5..EV8
 * state machine (or record the error flags) and xTaskNotifyFromISR() the
 * task blocked in i2c1_master_write/read. */
void i2c1_on_event_irq(void);
void i2c1_on_error_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* W2E1_I2C1_DRIVER_H */
