#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "w4e2_internal.h"

#define MPU_ADDRESS 0x68u
#define MPU_PWR_MGMT_1 0x6bu
#define MPU_SAMPLE_DIV 0x19u
#define MPU_ACCEL_XOUT_H 0x3bu
#define I2C_TIMEOUT_MS 28u
#define I2C_LINES (GPIO_IDR_IDR6 | GPIO_IDR_IDR7)
#define I2C_INTERRUPTS (I2C_CR2_ITERREN | I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN)
#define I2C_ERRORS (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)

enum transfer_phase { WRITE_ADDRESS, WRITE_DATA, READ_ADDRESS, READ_DATA };

static volatile struct {
    TaskHandle_t owner;
    enum transfer_phase phase;
    const uint8_t *tx;
    uint8_t *rx;
    uint8_t tx_len, rx_len, pos;
    bool ok, busy;
} transfer;
static StaticSemaphore_t bus_storage;
static SemaphoreHandle_t bus;
static volatile uint32_t recovery_count, last_recovery_pulses;

static void delay_us(uint32_t us)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t clocks = us * (SystemCoreClock / 1000000u);
    while (DWT->CYCCNT - start < clocks) {
        __NOP();
    }
}

static void configure_pins(bool alternate)
{
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    const uint32_t mode = alternate ?
        (GPIO_CRL_MODE6 | GPIO_CRL_MODE7) : (GPIO_CRL_MODE6_1 | GPIO_CRL_MODE7_1);
    const uint32_t cnf = alternate ?
        (GPIO_CRL_CNF6 | GPIO_CRL_CNF7) : (GPIO_CRL_CNF6_0 | GPIO_CRL_CNF7_0);
    GPIOB->CRL = (GPIOB->CRL & ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
                                GPIO_CRL_MODE7 | GPIO_CRL_CNF7)) |
                 mode | cnf;
}

static void configure_i2c(void)
{
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
    I2C1->CR2 = 36u;
    I2C1->CCR = I2C_CCR_FS | 30u;
    I2C1->TRISE = 11u;
    I2C1->OAR1 = 1u << 14u;
    I2C1->CR1 = I2C_CR1_PE;
}

void w4e2_imu_init(void)
{
    bus = xSemaphoreCreateMutexStatic(&bus_storage);
    configASSERT(bus != NULL);
    configASSERT((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV2);
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    configure_pins(true);
    configure_i2c();
    NVIC_SetPriority(I2C1_EV_IRQn, 6u);
    NVIC_SetPriority(I2C1_ER_IRQn, 6u);
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
}

bool w4e2_imu_busy(void)
{
    return transfer.busy || (I2C1->SR2 & I2C_SR2_BUSY) != 0u;
}

uint32_t w4e2_imu_recoveries(void)
{
    return recovery_count;
}

uint32_t w4e2_imu_recovery_pulses(void)
{
    return last_recovery_pulses;
}

static bool bus_free(void)
{
    return (I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0u &&
           (I2C1->SR2 & I2C_SR2_BUSY) == 0u &&
           (GPIOB->IDR & I2C_LINES) == I2C_LINES;
}

static bool wait_scl_high(void)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t limit = SystemCoreClock / 10000u;
    while ((GPIOB->IDR & GPIO_IDR_IDR6) == 0u) {
        if (DWT->CYCCNT - start >= limit) {
            return false;
        }
    }
    return true;
}

static void recover_bus(void)
{
    /* W2.1 GPIO recovery, called with the bus mutex held and interrupts open. */
    ++recovery_count;
    last_recovery_pulses = 0u;
    I2C1->CR2 &= ~I2C_INTERRUPTS;
    I2C1->CR1 = 0u;
    configure_pins(false);
    delay_us(5u);
    for (uint32_t pulse = 0u; pulse < 9u; ++pulse) {
        if (!wait_scl_high() || (GPIOB->IDR & GPIO_IDR_IDR7) != 0u) {
            break;
        }
        GPIOB->BRR = GPIO_BRR_BR6;
        delay_us(5u);
        GPIOB->BSRR = GPIO_BSRR_BS6;
        ++last_recovery_pulses;
        if (!wait_scl_high()) {
            break;
        }
        delay_us(5u);
    }

    /* Toggle both lines as in W2.1 for the STM32F1 analog-filter erratum. */
    if ((GPIOB->IDR & I2C_LINES) == I2C_LINES) {
        GPIOB->BRR = GPIO_BRR_BR7;
        delay_us(5u);
        GPIOB->BRR = GPIO_BRR_BR6;
        delay_us(5u);
        GPIOB->BSRR = GPIO_BSRR_BS6;
        if (wait_scl_high()) {
            delay_us(5u);
        }
        GPIOB->BSRR = GPIO_BSRR_BS7;
        delay_us(5u);
    }
    configure_pins(true);
    configure_i2c();
}

static void complete_from_isr(bool ok)
{
    I2C1->CR2 &= ~I2C_INTERRUPTS;
    const TaskHandle_t owner = transfer.owner;
    if (owner != NULL) {
        transfer.ok = ok;
        transfer.owner = NULL;
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveIndexedFromISR(owner, W4E2_IO_NOTIFY_INDEX, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

void I2C1_ER_IRQHandler(void)
{
    const uint32_t errors = I2C1->SR1 & I2C_ERRORS;
    if (errors != 0u) {
        I2C1->SR1 = I2C_ERRORS & ~errors;
        if (errors == I2C_SR1_AF && (I2C1->SR2 & I2C_SR2_MSL) != 0u) {
            I2C1->CR1 |= I2C_CR1_STOP;
        }
        complete_from_isr(false);
    }
}

static void read_byte(void)
{
    transfer.rx[transfer.pos] = (uint8_t)I2C1->DR;
    ++transfer.pos;
}

static void receive_from_isr(uint32_t sr1)
{
    const uint8_t remaining = transfer.rx_len - transfer.pos;
    if (remaining == 1u && (sr1 & I2C_SR1_RXNE) != 0u) {
        read_byte();
        complete_from_isr(true);
    } else if ((sr1 & I2C_SR1_BTF) != 0u && remaining > 3u) {
        read_byte();
    } else if ((sr1 & I2C_SR1_BTF) != 0u && remaining == 3u) {
        /* W2.1 / RM0008: NACK and STOP must precede the final byte.
         * Keep the DR-to-STOP sequence contiguous for the F1 erratum. */
        const uint32_t primask = __get_PRIMASK();
        __disable_irq();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        read_byte();
        I2C1->CR1 |= I2C_CR1_STOP;
        read_byte();
        I2C1->CR2 |= I2C_CR2_ITBUFEN;
        __set_PRIMASK(primask);
    }
}

void I2C1_EV_IRQHandler(void)
{
    const uint32_t sr1 = I2C1->SR1;
    if ((sr1 & I2C_ERRORS) != 0u) {
        I2C1_ER_IRQHandler();
        return;
    }
    if (transfer.owner == NULL) {
        I2C1->CR2 &= ~I2C_INTERRUPTS;
        return;
    }

    switch (transfer.phase) {
    case WRITE_ADDRESS:
    case READ_ADDRESS:
        if ((sr1 & I2C_SR1_SB) != 0u) {
            I2C1->DR = (MPU_ADDRESS << 1u) | (transfer.phase == READ_ADDRESS);
        } else if ((sr1 & I2C_SR1_ADDR) != 0u) {
            const bool reading = transfer.phase == READ_ADDRESS;
            transfer.phase = reading ? READ_DATA : WRITE_DATA;
            if (!reading) {
                I2C1->CR2 |= I2C_CR2_ITBUFEN;
            }
            (void)I2C1->SR2;
        }
        break;
    case WRITE_DATA:
        if (transfer.pos < transfer.tx_len && (sr1 & I2C_SR1_TXE) != 0u) {
            I2C1->DR = transfer.tx[transfer.pos++];
            if (transfer.pos == transfer.tx_len) {
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            }
        } else if (transfer.pos == transfer.tx_len && (sr1 & I2C_SR1_BTF) != 0u) {
            if (transfer.rx_len != 0u) {
                transfer.pos = 0u;
                transfer.phase = READ_ADDRESS;
                I2C1->CR1 |= I2C_CR1_START;
            } else {
                I2C1->CR1 |= I2C_CR1_STOP;
                complete_from_isr(true);
            }
        }
        break;
    case READ_DATA:
        receive_from_isr(sr1);
        break;
    }
}

static bool exchange(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint8_t rx_len)
{
    configASSERT(rx_len == 0u || rx_len >= 3u);
    const TickType_t timeout = pdMS_TO_TICKS(I2C_TIMEOUT_MS);
    if (xSemaphoreTake(bus, timeout) != pdTRUE) {
        return false;
    }

    bool ok = false;
    transfer.busy = true;
    if (bus_free()) {
        (void)ulTaskNotifyTakeIndexed(W4E2_IO_NOTIFY_INDEX, pdTRUE, 0u);
        taskENTER_CRITICAL();
        transfer.phase = WRITE_ADDRESS;
        transfer.owner = xTaskGetCurrentTaskHandle();
        transfer.tx = tx;
        transfer.rx = rx;
        transfer.tx_len = tx_len;
        transfer.rx_len = rx_len;
        transfer.pos = 0u;
        transfer.ok = false;
        I2C1->SR1 = 0u;
        I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
        I2C1->CR2 |= I2C_CR2_ITERREN | I2C_CR2_ITEVTEN;
        I2C1->CR1 |= I2C_CR1_START;
        taskEXIT_CRITICAL();

        const uint32_t done = ulTaskNotifyTakeIndexed(W4E2_IO_NOTIFY_INDEX, pdTRUE, timeout);
        taskENTER_CRITICAL();
        I2C1->CR2 &= ~I2C_INTERRUPTS;
        transfer.owner = NULL;
        ok = done != 0u && transfer.ok;
        taskEXIT_CRITICAL();
        if (ok) {
            const uint32_t start = DWT->CYCCNT;
            const uint32_t limit = SystemCoreClock / 20000u;
            while (!bus_free() && DWT->CYCCNT - start < limit) {
                __NOP();
            }
            ok = bus_free();
        }
    }
    if (!ok) {
        recover_bus();
    }
    transfer.busy = false;
    (void)xSemaphoreGive(bus);
    return ok;
}

bool w4e2_imu_wake(void)
{
    static const uint8_t command[] = { MPU_PWR_MGMT_1, 0x01u };
    return exchange(command, sizeof command, NULL, 0u);
}

bool w4e2_imu_configure(void)
{
    /* DLPF on: 1 kHz internal rate / (1 + 1), +/-2 g, +/-250 deg/s. */
    static const uint8_t command[] = { MPU_SAMPLE_DIV, 1u, 3u, 0u, 0u };
    return exchange(command, sizeof command, NULL, 0u);
}

static int16_t read_be16(const uint8_t *raw)
{
    return (int16_t)(((uint16_t)raw[0] << 8u) | raw[1]);
}

bool w4e2_imu_read(w4e2_imu_sample_t *sample)
{
    static const uint8_t reg = MPU_ACCEL_XOUT_H;
    uint8_t raw[14];
    const TickType_t acquisition_tick = xTaskGetTickCount();
    if (!exchange(&reg, 1u, raw, sizeof raw)) {
        return false;
    }
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        sample->accel_g[axis] = (float)read_be16(raw + 2u * axis) * (1.0f / 16384.0f);
        sample->gyro_dps[axis] = (float)read_be16(raw + 8u + 2u * axis) * (1.0f / 131.0f);
    }
    sample->sample_tick = acquisition_tick;
    return true;
}
