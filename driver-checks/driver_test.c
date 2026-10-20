#include <string.h>
#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "../week2/ex1/i2c1_driver.c"

MockI2C mock_i2c;
MockRCC mock_rcc;
MockGPIO mock_gpio;
MockAFIO mock_afio;
MockCore mock_core;
MockDWT mock_clock;
uint32_t SystemCoreClock = 72000000u;
uint32_t mock_primask, mock_ipsr;
unsigned mock_critical, mock_nvic[2], mock_priorities[2];
static uint32_t notification;
static unsigned notification_calls, pulses, release_after;
static int hold_sda, hold_scl, take_fails, finish_stop;
static uint32_t latches;
static unsigned checks;
enum { MODE_OK, MODE_TIMEOUT, MODE_NACK, MODE_ARLO, MODE_BOUNDARY, MODE_STUCK, MODE_BERR };
static int mode;

#define CHECK(x) do { ++checks; configASSERT(x); } while (0)

MockDWT *mock_dwt(void)
{
    const uint32_t previous = mock_gpio.IDR;
    latches |= mock_gpio.BSRR & 0xffffu;
    latches &= ~(mock_gpio.BSRR >> 16u);
    latches &= ~mock_gpio.BRR;
    mock_gpio.BSRR = mock_gpio.BRR = 0u;
    const int gpio_mode = (mock_gpio.CRL & GPIO_CRL_CNF6) == GPIO_CRL_CNF6_0;
    uint32_t lines = gpio_mode ? latches : (GPIO_IDR_IDR6 | GPIO_IDR_IDR7);
    if (hold_scl) { lines &= ~GPIO_IDR_IDR6; }
    if (gpio_mode && (previous & GPIO_IDR_IDR6) == 0u &&
        (lines & GPIO_IDR_IDR6) != 0u && hold_sda) {
        ++pulses;
        if (release_after != 0u && pulses >= release_after) { hold_sda = 0; }
    }
    if (hold_sda) { lines &= ~GPIO_IDR_IDR7; }
    mock_gpio.IDR = lines;
    if (gpio_mode && (lines & 0xc0u) == 0xc0u) {
        mock_i2c.SR2 &= ~(I2C_SR2_BUSY | I2C_SR2_MSL);
    }
    if (finish_stop && (mock_i2c.CR1 & I2C_CR1_STOP) != 0u) {
        mock_i2c.CR1 &= ~I2C_CR1_STOP;
        mock_i2c.SR2 &= ~(I2C_SR2_BUSY | I2C_SR2_MSL);
    }
    mock_clock.CYCCNT += 7200u;
    return &mock_clock;
}

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage)
{
    storage->available = 1;
    return storage;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks)
{
    CHECK(ticks == 28u);
    if (take_fails || !semaphore->available) { return pdFALSE; }
    semaphore->available = 0;
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    CHECK(semaphore->available == 0);
    semaphore->available = 1;
    return pdTRUE;
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)(uintptr_t)1u; }
BaseType_t xTaskNotifyFromISR(TaskHandle_t owner, uint32_t value, int action, BaseType_t *woken)
{
    CHECK(mock_primask == 0u);
    CHECK(owner == xTaskGetCurrentTaskHandle());
    CHECK(value == 0u && action == eIncrement);
    ++notification;
    ++notification_calls;
    *woken = pdTRUE;
    return pdTRUE;
}

static void event(uint32_t flags, uint8_t data)
{
    mock_i2c.SR1 = flags;
    mock_i2c.DR = data;
    I2C1_EV_IRQHandler();
}
static void drive_transaction(void)
{
    CHECK(mock_critical == 0u && mock_primask == 0u);
    CHECK(mock_nvic[0] && mock_nvic[1]);
    CHECK((mock_i2c.CR2 & I2C1_IRQ_MASK) == I2C1_IRQ_MASK);
    CHECK((mock_i2c.CR1 & I2C_CR1_START) != 0u);
    const uint8_t address = s_i2c1.address_byte;
    mock_i2c.CR1 &= ~I2C_CR1_START;
    mock_i2c.SR2 = I2C_SR2_BUSY | I2C_SR2_MSL;
    event(I2C_SR1_SB, 0u);
    CHECK(mock_i2c.DR == address);
    if (mode == MODE_NACK || mode == MODE_ARLO || mode == MODE_BERR) {
        mock_i2c.SR1 = mode == MODE_NACK ? I2C_SR1_AF :
                        mode == MODE_ARLO ? I2C_SR1_ARLO : I2C_SR1_BERR;
        if (mode == MODE_ARLO) { mock_i2c.SR2 &= ~I2C_SR2_MSL; }
        I2C1_ER_IRQHandler();
        if (mode == MODE_ARLO) {
            CHECK((mock_i2c.CR1 & I2C_CR1_STOP) == 0u);
            mock_i2c.SR2 = 0u;
        }
        mock_i2c.SR1 = 0u; /* Model W0C flags; normal memory cannot emulate W0C. */
        return;
    }
    event(I2C_SR1_ADDR, 0u);
    if ((address & 1u) == 0u) {
        while (s_i2c1.position < s_i2c1.length) {
            const uint16_t index = s_i2c1.position;
            const uint8_t expected = s_i2c1.tx_data[index];
            event(I2C_SR1_TXE, 0u);
            CHECK(mock_i2c.DR == expected && s_i2c1.position == index + 1u);
        }
        CHECK((mock_i2c.CR2 & I2C_CR2_ITBUFEN) == 0u);
        event(I2C_SR1_BTF, 0u);
    } else if (s_i2c1.length == 1u) {
        CHECK((mock_i2c.CR1 & (I2C_CR1_ACK | I2C_CR1_POS)) == 0u);
        event(I2C_SR1_RXNE, 0xa1u);
    } else if (s_i2c1.length == 2u) {
        CHECK((mock_i2c.CR1 & I2C_CR1_POS) != 0u);
        CHECK((mock_i2c.CR1 & I2C_CR1_ACK) == 0u);
        CHECK((mock_i2c.CR2 & I2C_CR2_ITBUFEN) == 0u);
        event(I2C_SR1_BTF, 0xa2u);
    } else {
        CHECK((mock_i2c.CR1 & I2C_CR1_ACK) != 0u);
        CHECK((mock_i2c.CR1 & I2C_CR1_POS) == 0u);
        while ((uint16_t)(s_i2c1.length - s_i2c1.position) > 3u) {
            const uint16_t index = s_i2c1.position;
            event(I2C_SR1_BTF, (uint8_t)(0x20u + index));
            CHECK(s_i2c1.position == index + 1u);
            CHECK((mock_i2c.CR1 & I2C_CR1_ACK) != 0u);
        }
        event(I2C_SR1_BTF, 0xc1u);
        CHECK(s_i2c1.length - s_i2c1.position == 1);
        CHECK((mock_i2c.CR1 & I2C_CR1_ACK) == 0u);
        CHECK((mock_i2c.CR1 & I2C_CR1_STOP) != 0u);
        CHECK((mock_i2c.CR2 & I2C_CR2_ITBUFEN) != 0u);
        CHECK(notification_calls == 0u);
        event(I2C_SR1_RXNE, 0xd1u);
    }
    CHECK(s_i2c1.position == s_i2c1.length);
    CHECK(notification_calls == 1u && s_i2c1.owner == NULL);
    CHECK((mock_i2c.CR2 & I2C1_IRQ_MASK) == 0u);
    mock_i2c.SR1 = 0u;
}

uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks)
{
    CHECK(clear == pdTRUE);
    if (ticks != 0u) {
        CHECK(ticks == 28u);
        if (mode == MODE_TIMEOUT || mode == MODE_STUCK) { return 0u; }
        drive_transaction();
        if (mode == MODE_BOUNDARY) { return 0u; }
    }
    const uint32_t result = notification;
    notification = 0u;
    return result;
}

static void reset_driver(void)
{
    memset(&mock_i2c, 0, sizeof mock_i2c);
    memset(&mock_rcc, 0, sizeof mock_rcc);
    memset(&mock_gpio, 0, sizeof mock_gpio);
    memset(&mock_afio, 0, sizeof mock_afio);
    memset(&mock_clock, 0, sizeof mock_clock);
    mock_primask = mock_ipsr = mock_critical = 0u;
    mock_rcc.CFGR = RCC_CFGR_PPRE1_DIV2;
    latches = mock_gpio.IDR = 0xc0u;
    notification = notification_calls = pulses = release_after = 0u;
    hold_sda = hold_scl = take_fails = 0;
    finish_stop = 1;
    mode = MODE_OK;
    s_bus_mutex = NULL;
    i2c1_driver_init();
    CHECK(mock_i2c.CCR == 180u && mock_i2c.TRISE == 37u);
    CHECK(mock_priorities[0] == 5u && mock_priorities[1] == 5u);
}
static void check_clean(void)
{
    CHECK(s_i2c1.state == I2C1_STATE_IDLE && s_i2c1.owner == NULL);
    CHECK(s_i2c1.tx_data == NULL && s_i2c1.rx_data == NULL);
    CHECK(s_bus_mutex->available == 1 && notification == 0u);
    CHECK(mock_primask == 0u && mock_critical == 0u);
    CHECK((mock_i2c.CR2 & I2C1_IRQ_MASK) == 0u);
    CHECK(mock_nvic[0] && mock_nvic[1]);
}
int main(void)
{
    const uint16_t lengths[] = {1u, 2u, 3u, 4u, 6u, 16u, 256u};
    uint8_t buffer[260], tx[] = {0x6bu, 0u, 0x75u, 0x3bu};
    for (unsigned i = 0u; i < sizeof lengths / sizeof lengths[0]; ++i) {
        reset_driver();
        memset(buffer, 0xee, sizeof buffer);
        CHECK(i2c1_master_read(0x68u, &buffer[1], lengths[i]) == I2C1_OK);
        CHECK(buffer[0] == 0xee && buffer[lengths[i] + 1u] == 0xee);
        CHECK(buffer[lengths[i]] == (lengths[i] == 1u ? 0xa1u : lengths[i] == 2u ? 0xa2u : 0xd1u));
        check_clean();
    }
    for (uint16_t length = 1u; length <= sizeof tx; ++length) {
        reset_driver();
        CHECK(i2c1_master_write(0x69u, tx, length) == I2C1_OK);
        check_clean();
    }
    reset_driver();
    mode = MODE_BOUNDARY;
    CHECK(i2c1_master_read(0x68u, buffer, 6u) == I2C1_OK);
    check_clean();

    reset_driver();
    mode = MODE_NACK;
    CHECK(i2c1_master_write(0x68u, tx, 1u) == I2C1_ERR_NACK);
    check_clean();
    mode = MODE_OK;
    notification_calls = 0u;
    CHECK(i2c1_master_read(0x68u, buffer, 6u) == I2C1_OK);
    check_clean();

    reset_driver();
    mode = MODE_BERR;
    CHECK(i2c1_master_write(0x68u, tx, 1u) == I2C1_ERR_RECOVERED);
    check_clean();

    reset_driver();
    mode = MODE_ARLO;
    CHECK(i2c1_master_write(0x68u, tx, 1u) == I2C1_ERR_BUS);
    CHECK(pulses == 0u);
    check_clean();

    reset_driver();
    mode = MODE_TIMEOUT;
    CHECK(i2c1_master_write(0x68u, tx, 1u) == I2C1_ERR_RECOVERED);
    check_clean();
    event(I2C_SR1_RXNE, 0u); /* Late interrupt must not access a detached buffer. */
    CHECK(notification_calls == 0u);

    reset_driver();
    take_fails = 1;
    CHECK(i2c1_master_write(0x68u, tx, 1u) == I2C1_ERR_TIMEOUT);
    CHECK(s_bus_mutex->available == 1);

    reset_driver();
    CHECK(i2c1_master_read(0x80u, buffer, 1u) == I2C1_ERR_BUS);
    CHECK(i2c1_master_read(0x68u, NULL, 1u) == I2C1_ERR_BUS);
    CHECK(i2c1_master_write(0x68u, tx, 0u) == I2C1_ERR_BUS);
    mock_ipsr = 1u;
    CHECK(i2c1_master_write(0x68u, tx, 1u) == I2C1_ERR_BUS);
    CHECK(!i2c1_bus_recover());
    mock_ipsr = 0u;

    reset_driver();
    hold_sda = 1;
    release_after = 9u;
    CHECK(i2c1_bus_recover());
    CHECK(pulses == 9u);
    check_clean();

    reset_driver();
    hold_sda = 1;
    CHECK(!i2c1_bus_recover());
    CHECK(pulses == 9u);
    check_clean();

    reset_driver();
    hold_scl = 1;
    CHECK(!i2c1_bus_recover());
    CHECK(pulses == 0u);
    check_clean();

    reset_driver();
    for (unsigned repeat = 0u; repeat < 11u; ++repeat) {
        hold_sda = 1;
        release_after = repeat % 9u + 1u;
        pulses = 0u;
        CHECK(i2c1_bus_recover());
        CHECK(pulses == release_after);
        notification_calls = 0u;
        CHECK(i2c1_master_read(0x68u, buffer, 6u) == I2C1_OK);
        check_clean();
    }

    reset_driver();
    mock_clock.CYCCNT = UINT32_MAX - 1000u;
    i2c1_delay_us(5u);
    CHECK(mock_clock.CYCCNT < 100000u);

    printf("PASS: %u assertions; RX lengths 1/2/3/4/6/16/256, TX, NACK, ARLO, timeout boundary, late ISR, mutex, recovery and DWT wrap.\n", checks);
    return 0;
}
