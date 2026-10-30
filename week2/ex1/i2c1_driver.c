#include "i2c1_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f1xx.h"
#include "params.h"

#ifndef W2E1_TRACE_EV
#define W2E1_TRACE_EV 0 // 1: PB13 = ISR EV, PB14 = ISR ER cho logic analyzer.
#endif

#define I2C1_IRQ_MASK \
    (I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN)
#define I2C1_ERROR_MASK \
    (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)
#define I2C1_LINES (GPIO_IDR_IDR6 | GPIO_IDR_IDR7)

/* I2C1: PB6/PB7, một Master, 100 kHz. Init một lần trước scheduler.
 * Read/write/recovery gọi từ task khi scheduler chạy; dành notification index 0.
 * STEP là thứ tự đọc file; giao dịch bắt đầu từ master_write/master_read.
 * Đối chiếu RM0008 mục 26.3.3 và ES096 mục 2.8 cho STM32F103C8.
 */
_Static_assert(configSUPPORT_STATIC_ALLOCATION == 1 &&
               configUSE_MUTEXES == 1 && configUSE_TASK_NOTIFICATIONS == 1,
               "I2C1 requires static mutexes and task notifications");
_Static_assert(configMAX_SYSCALL_INTERRUPT_PRIORITY != 0,
               "I2C1 requires a nonzero syscall interrupt priority");
_Static_assert(W2E1_I2C_SPEED_HZ == 100000UL,
               "I2C1 requires Standard-mode 100 kHz");

/* STEP 1: FSM và context lưu tiến độ, buffer, kết quả, task owner.
 * Mutex chia quyền dùng bus giữa các task; volatile không thay đồng bộ.
 */
typedef enum {
    I2C1_STATE_IDLE = 0,
    I2C1_STATE_START,
    I2C1_STATE_ADDRESS,
    I2C1_STATE_TX,
    I2C1_STATE_RX,
    I2C1_STATE_STOP
} i2c1_state_t;

typedef struct {
    i2c1_state_t state;
    i2c1_status_t result;
    uint32_t error_flags; // 
    TaskHandle_t owner;
    uint8_t address_byte;
    const uint8_t *tx_data;
    uint8_t *rx_data;
    uint16_t length;
    uint16_t position;
} i2c1_context_t;

static volatile i2c1_context_t s_i2c1;
static StaticSemaphore_t s_mutex_storage;
static SemaphoreHandle_t s_bus_mutex;
static uint32_t s_recovery_pulse_count;
// Số xung SCL của lần recovery gần nhất; ghi khi giữ mutex bus.

/* STEP 2: Chờ FreeRTOS dùng tick; chờ thanh ghi dùng cycle của DWT.
 * Chỉ cho gọi API blocking sau init, ngoài ISR và ngoài vùng khóa ngắt.
 */
static TickType_t i2c1_timeout_ticks(void)
{
    const TickType_t ticks = pdMS_TO_TICKS(W2E1_I2C_TIMEOUT_MS);
    return ticks != 0u ? ticks : (TickType_t)1u;
}

static uint32_t i2c1_timeout_cycles(void)
{
    return (SystemCoreClock / 1000UL) * W2E1_I2C_TIMEOUT_MS;
}

/* Phép trừ unsigned tính được thời gian ngắn qua một lần tràn DWT. */
static bool i2c1_expired(uint32_t start, uint32_t timeout)
{
    return (uint32_t)(DWT->CYCCNT - start) >= timeout; // DWT tràn 32-bit sau ~60 s ở 72 MHz.
}

static bool i2c1_can_block(void)
{
    return s_bus_mutex != NULL && __get_IPSR() == 0u &&
           __get_PRIMASK() == 0u && __get_BASEPRI() == 0u;
}

/* STEP 3: Khóa nguồn ngắt CR2 và NVIC trước khi sửa context.
 * Mở NVIC chỉ cho phép nhận IRQ; nguồn CR2 được bật khi bắt đầu giao dịch.
 */
static void i2c1_irq_disable(void)
{
    I2C1->CR2 &= ~I2C1_IRQ_MASK;
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);
}

static void i2c1_irq_enable(void)
{
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
}

/* STEP 4: PB6 = SCL, PB7 = SDA; AF open-drain khi chạy, GPIO khi recovery.
 * BSRR nhả chân trước khi đổi mode; HIGH cần pull-up ngoài.
 * MODE = 2 MHz là tốc độ GPIO, không phải tần số SCL.
 */
static void i2c1_gpio_configure(bool alternate)
{
    const uint32_t cnf = alternate // Alternate Function Open-Drain
        ? (GPIO_CRL_CNF6 | GPIO_CRL_CNF7)
        : (GPIO_CRL_CNF6_0 | GPIO_CRL_CNF7_0);

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN;
    (void)RCC->APB2ENR;
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    GPIOB->CRL =
        (GPIOB->CRL & ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
                        GPIO_CRL_MODE7 | GPIO_CRL_CNF7)) |
        GPIO_CRL_MODE6_1 | GPIO_CRL_MODE7_1 | cnf;
}

/* STEP 5: RCC cấp clock/reset I2C1; cấu hình timing khi PE = 0.
 * PCLK1 = 36 MHz: CR2.FREQ = 36, CCR = 180, TRISE = 37; OAR1[14] = 1.
 */
static void i2c1_hw_reset(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;  // I2C1RST = 1: giữ I2C1 ở reset.
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST; // I2C1RST = 0: nhả reset để cấu hình.
}

static void i2c1_hw_configure(void)
{
    const uint32_t pclk1_hz = 36000000UL;

    i2c1_hw_reset();
    I2C1->CR2 = pclk1_hz / 1000000UL;                // FREQ = 36 ứng với PCLK1 = 36 MHz.
    I2C1->CCR = pclk1_hz / (2UL * W2E1_I2C_SPEED_HZ); // CCR = 180 để tạo SCL 100 kHz.
    I2C1->TRISE = pclk1_hz / 1000000UL + 1UL;         // TRISE = 37: rise time tối đa 1 us.
    I2C1->OAR1 = 1UL << 14;                          // OAR1[14] bắt buộc giữ bằng 1 theo RM.
}

/* STEP 6: Init mutex static, GPIO, I2C, NVIC và DWT rồi bật PE.
 * EV/ER cùng priority được phép gọi FromISR; nguồn ngắt CR2 còn tắt.
 */
void i2c1_driver_init(void)
{
    configASSERT(s_bus_mutex == NULL);
    configASSERT(SystemCoreClock == 72000000UL);
    configASSERT((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV2);
    configASSERT(NVIC_GetPriorityGrouping() <= (7u - __NVIC_PRIO_BITS));

    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);
    s_i2c1 = (i2c1_context_t){ .state = I2C1_STATE_IDLE, .result = I2C1_OK };
    s_bus_mutex = xSemaphoreCreateMutexStatic(&s_mutex_storage);
    configASSERT(s_bus_mutex != NULL);

    i2c1_gpio_configure(true);
#if W2E1_TRACE_EV
    GPIOB->BRR = GPIO_BRR_BR13 | GPIO_BRR_BR14;
    GPIOB->CRH = (GPIOB->CRH & ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13 |
                                 GPIO_CRH_MODE14 | GPIO_CRH_CNF14)) |
                 GPIO_CRH_MODE13_1 | GPIO_CRH_MODE14_1; // Push-pull 2 MHz.
#endif
    i2c1_hw_configure();
    configASSERT((AFIO->MAPR & AFIO_MAPR_I2C1_REMAP) == 0u);

    /* NVIC nhận priority chưa dịch bit; EV/ER dùng cùng mức. */
    const uint32_t priority =
        configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8u - __NVIC_PRIO_BITS);
    NVIC_SetPriority(I2C1_EV_IRQn, priority);
    NVIC_SetPriority(I2C1_ER_IRQn, priority);

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    I2C1->CR1 = I2C_CR1_PE;
    i2c1_irq_enable();
}

/* STEP 7: Delay bằng CYCCNT - start; không reset bộ đếm DWT dùng chung.
 * Ở 72 MHz, 5 us = 360 cycles; ISR có thể làm delay dài hơn.
 */
static void i2c1_delay_us(uint32_t us)
{
    const uint32_t cycles = (SystemCoreClock / 1000000UL) * us;
    const uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

/* STEP 8: Bus rảnh khi START/STOP = 0, BUSY = 0 và SCL/SDA HIGH.
 * Chờ thêm 5 us trước giao dịch mới hoặc trả mutex; có timeout DWT.
 */
static bool i2c1_wait_bus_free(void)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t timeout = i2c1_timeout_cycles();

    do {
        if ((I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0u &&
            (I2C1->SR2 & I2C_SR2_BUSY) == 0u &&
            (GPIOB->IDR & I2C1_LINES) == I2C1_LINES) {
            i2c1_delay_us(5u);
            return (GPIOB->IDR & I2C1_LINES) == I2C1_LINES &&
                   (I2C1->SR2 & I2C_SR2_BUSY) == 0u;
        }
    } while (!i2c1_expired(start, timeout));

    return false;
}

/* STEP 9: Nhả SCL chưa chắc đã HIGH; đọc IDR và chờ có timeout. */
static bool i2c1_wait_scl_high(uint32_t start, uint32_t timeout)
{
    while (!i2c1_expired(start, timeout)) {
        if ((GPIOB->IDR & GPIO_IDR_IDR6) != 0u) {
            return true;
        }
    }
    return false;
}

/* STEP 10: Tối đa 9 xung SCL để slave nhả SDA; dừng sớm nếu SDA HIGH.
 * Mỗi pha LOW/HIGH ít nhất 5 us; các xung dùng chung timeout recovery.
 */
static bool i2c1_recovery_pulses(uint32_t start, uint32_t timeout)
{
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    s_recovery_pulse_count = 0u;
    for (uint32_t pulse = 0u; pulse < 9u; ++pulse) {
        if (!i2c1_wait_scl_high(start, timeout)) {
            return false;
        }
        if ((GPIOB->IDR & GPIO_IDR_IDR7) != 0u) {
            return true;
        }
        GPIOB->BRR = GPIO_BRR_BR6;
        ++s_recovery_pulse_count;
        i2c1_delay_us(5u);
        GPIOB->BSRR = GPIO_BSRR_BS6;
        if (!i2c1_wait_scl_high(start, timeout)) {
            return false;
        }
        i2c1_delay_us(5u);
    }
    return !i2c1_expired(start, timeout) &&
           (GPIOB->IDR & GPIO_IDR_IDR7) != 0u;
}

/* STEP 11: Tạo START/STOP bằng GPIO và kiểm tra từng mức qua IDR.
 * SDA xuống/lên khi SCL HIGH; chuyển mức cả hai chân giúp gỡ analog filter.
 */
static bool i2c1_recovery_start_stop(uint32_t start, uint32_t timeout)
{
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    if (!i2c1_wait_scl_high(start, timeout) ||
        (GPIOB->IDR & GPIO_IDR_IDR7) == 0u) {
        return false;
    }
    i2c1_delay_us(5u);
    GPIOB->BRR = GPIO_BRR_BR7;
    i2c1_delay_us(5u);
    if ((GPIOB->IDR & GPIO_IDR_IDR7) != 0u) {
        return false;
    }
    GPIOB->BRR = GPIO_BRR_BR6;
    i2c1_delay_us(5u);
    if ((GPIOB->IDR & GPIO_IDR_IDR6) != 0u) {
        return false;
    }
    GPIOB->BSRR = GPIO_BSRR_BS6;
    if (!i2c1_wait_scl_high(start, timeout)) {
        return false;
    }
    i2c1_delay_us(5u);
    GPIOB->BSRR = GPIO_BSRR_BS7;
    i2c1_delay_us(5u);
    return !i2c1_expired(start, timeout) &&
           (GPIOB->IDR & I2C1_LINES) == I2C1_LINES;
}

/* STEP 12: Giữ mutex, tắt I2C, phục hồi bằng GPIO rồi cấu hình lại AF/I2C.
 * Luôn nhả chân khi thoát; ERR_RECOVERED không có nghĩa giao dịch thành công.
 */
static bool i2c1_recover_locked(void)
{
    i2c1_irq_disable();
    I2C1->CR1 &= ~I2C_CR1_PE;
    i2c1_hw_reset();
    i2c1_gpio_configure(false);
    i2c1_delay_us(5u);

    const uint32_t start = DWT->CYCCNT;
    const uint32_t timeout = i2c1_timeout_cycles();
    bool recovered = i2c1_recovery_pulses(start, timeout) &&
                     i2c1_recovery_start_stop(start, timeout);

    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    i2c1_gpio_configure(true);
    i2c1_hw_configure();
    s_i2c1 = (i2c1_context_t){ .state = I2C1_STATE_IDLE };
    I2C1->CR1 = I2C_CR1_PE;
    recovered = recovered && (I2C1->SR2 & I2C_SR2_BUSY) == 0u;
    s_i2c1.result = recovered ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;
    i2c1_irq_enable();
    return recovered;
}

/* STEP 13: Recovery public tự lấy/trả mutex; nội bộ đã giữ mutex gọi locked. */
bool i2c1_bus_recover(void)
{
    if (!i2c1_can_block() ||
        xSemaphoreTake(s_bus_mutex, i2c1_timeout_ticks()) != pdTRUE) {
        return false;
    }
    const bool recovered = i2c1_recover_locked();
    const BaseType_t released = xSemaphoreGive(s_bus_mutex);
    configASSERT(released == pdTRUE);
    return recovered && released == pdTRUE;
}

uint32_t i2c1_last_recovery_pulses(void)
{
    return s_recovery_pulse_count;
}

/* STEP 14: Tắt nguồn ngắt, lưu kết quả rồi notify đúng task owner một lần.
 * State STOP nghĩa là chờ task dọn giao dịch, chưa khẳng định bus rảnh.
 */
static void i2c1_complete_from_isr(i2c1_status_t result)
{
    BaseType_t task_woken = pdFALSE;
    const TaskHandle_t owner = s_i2c1.owner;

    I2C1->CR2 &= ~I2C1_IRQ_MASK;
    if (owner == NULL) {
        return;
    }
    s_i2c1.result = result;
    s_i2c1.state = I2C1_STATE_STOP;
    s_i2c1.owner = NULL;
    (void)xTaskNotifyFromISR(owner, 0u, eIncrement, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

/* STEP 15: Lưu lỗi SR1 rồi xóa bằng ghi 0; AF đơn lẻ trả NACK.
 * Chỉ yêu cầu STOP khi còn Master; BERR/ARLO/OVR chuyển cho task xử lý.
 */
void i2c1_on_error_irq(void)
{
    const uint32_t errors = I2C1->SR1 & I2C1_ERROR_MASK;
    if (errors == 0u) {
        return;
    }
    s_i2c1.error_flags |= errors;
    I2C1->SR1 = I2C1_ERROR_MASK & ~errors;
    if (errors == I2C_SR1_AF && (I2C1->SR2 & I2C_SR2_MSL) != 0u &&
        (I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0u) {
        I2C1->CR1 |= I2C_CR1_STOP;
    }
    i2c1_complete_from_isr(errors == I2C_SR1_AF ? I2C1_ERR_NACK : I2C1_ERR_BUS);
}

/* STEP 16: Đọc một byte DR vào buffer rồi tăng số byte đã nhận. */
static void i2c1_store_rx_byte(void)
{
    const uint16_t position = s_i2c1.position;
    s_i2c1.rx_data[position] = (uint8_t)I2C1->DR;
    s_i2c1.position = position + 1u;
}

/* STEP 17: ISR ưu tiên lỗi, bỏ IRQ khi không có owner rồi chạy FSM.
 * EV5: SB = 1; đọc SR1 rồi ghi địa chỉ + R/W vào DR để xóa SB.
 */
void i2c1_on_event_irq(void)
{
    const uint32_t sr1 = I2C1->SR1;
    if ((sr1 & I2C1_ERROR_MASK) != 0u) {
        i2c1_on_error_irq();
        return;
    }
    if (s_i2c1.owner == NULL) {
        I2C1->CR2 &= ~I2C1_IRQ_MASK;
        return;
    }

    switch (s_i2c1.state) {
    case I2C1_STATE_START:
        if ((sr1 & I2C_SR1_SB) != 0u) {
            s_i2c1.state = I2C1_STATE_ADDRESS;
            I2C1->DR = s_i2c1.address_byte;
        }
        break;

    /* STEP 18: EV6 - địa chỉ đã được ACK; đọc SR2 sau SR1 để xóa ADDR.
     * RX cần đặt ACK/POS đúng độ dài; PRIMASK giữ liền mạch đoạn nhạy timing.
     */
    case I2C1_STATE_ADDRESS: {
        if ((sr1 & I2C_SR1_ADDR) == 0u) {
            break;
        }
        if ((s_i2c1.address_byte & 1u) == 0u) {
            s_i2c1.state = I2C1_STATE_TX;
            (void)I2C1->SR2;
            break;
        }

        const uint16_t length = s_i2c1.length;
        const uint32_t saved_primask = __get_PRIMASK();
        __disable_irq();
        s_i2c1.state = I2C1_STATE_RX;
        if (length == 1u) {
            /* 1 byte: ACK = 0, xóa ADDR, STOP; byte tới sẽ được NACK. */
            I2C1->CR1 &= ~(I2C_CR1_POS | I2C_CR1_ACK);
            (void)I2C1->SR2;
            I2C1->CR1 |= I2C_CR1_STOP;
        } else if (length == 2u) {
            /* 2 byte: POS = ACK = 1, xóa ADDR rồi ACK = 0; chờ BTF. */
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            I2C1->CR1 |= I2C_CR1_POS | I2C_CR1_ACK;
            (void)I2C1->SR2;
            I2C1->CR1 &= ~I2C_CR1_ACK;
        } else {
            /* N > 2: POS = 0, ACK = 1; dùng BTF để giữ được ba byte cuối. */
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            I2C1->CR1 &= ~I2C_CR1_POS;
            I2C1->CR1 |= I2C_CR1_ACK;
            (void)I2C1->SR2;
        }
        __set_PRIMASK(saved_primask);
        break;
    }

    /* STEP 19: TXE (EV8) cho nạp DR; đủ byte thì tắt ITBUFEN.
     * Chờ BTF (EV8_2) xác nhận byte cuối đã truyền rồi STOP và notify.
     */
    case I2C1_STATE_TX:
        if (s_i2c1.position == s_i2c1.length) {
            if ((sr1 & I2C_SR1_BTF) != 0u) {
                I2C1->CR1 |= I2C_CR1_STOP;
                i2c1_complete_from_isr(I2C1_OK);
            }
        } else if ((sr1 & I2C_SR1_TXE) != 0u) {
            I2C1->DR = s_i2c1.tx_data[s_i2c1.position];
            ++s_i2c1.position;
            if (s_i2c1.position == s_i2c1.length) {
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            }
        }
        break;

    /* STEP 20: RXNE (EV7) lấy byte cuối; còn > 3 byte thì đọc theo BTF.
     * remaining là số byte chưa chép vào buffer, kể cả byte đang trên bus.
     */
    case I2C1_STATE_RX: {
        const uint16_t remaining = s_i2c1.length - s_i2c1.position;
        if (remaining == 1u && (sr1 & I2C_SR1_RXNE) != 0u) {
            i2c1_store_rx_byte();
            i2c1_complete_from_isr(I2C1_OK);
        } else if ((sr1 & I2C_SR1_BTF) != 0u) {
            if (remaining > 3u) {
                i2c1_store_rx_byte();
            } else if (remaining == 2u || remaining == 3u) {
                /* STEP 21: Kết thúc RX theo Method 2 (RM0008).
                 * PRIMASK bảo vệ thứ tự đọc DR/STOP theo ES096 mục 2.8.2.
                 */
                const uint32_t saved_primask = __get_PRIMASK();
                __disable_irq();
                if (remaining == 2u) {
                    /* 2 byte: BTF giữ SCL LOW; STOP rồi đọc DR hai lần. */
                    I2C1->CR1 |= I2C_CR1_STOP;
                    i2c1_store_rx_byte();
                    i2c1_store_rx_byte();
                } else {
                    /* 3 byte: ACK = 0, đọc N-2, STOP, đọc N-1; đợi RXNE. */
                    I2C1->CR1 &= ~I2C_CR1_ACK;
                    i2c1_store_rx_byte();
                    I2C1->CR1 |= I2C_CR1_STOP;
                    i2c1_store_rx_byte();
                    I2C1->CR2 |= I2C_CR2_ITBUFEN;
                }
                __set_PRIMASK(saved_primask);
                if (remaining == 2u) {
                    i2c1_complete_from_isr(I2C1_OK);
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

/* STEP 22: Kiểm tra địa chỉ 7-bit, độ dài và đúng một buffer TX hoặc RX.
 * Lấy mutex có timeout; giữ quyền dùng bus đến hết cleanup/recovery.
 */
static i2c1_status_t i2c1_transfer(uint8_t addr7, const uint8_t *tx_data,
                                uint8_t *rx_data, uint16_t length)
{
    if (!i2c1_can_block() || addr7 > 0x7fu || length == 0u ||
        ((tx_data == NULL) == (rx_data == NULL))) {
        return I2C1_ERR_BUS;
    }

    const TickType_t wait_ticks = i2c1_timeout_ticks();
    if (xSemaphoreTake(s_bus_mutex, wait_ticks) != pdTRUE) {
        return I2C1_ERR_TIMEOUT;
    }

    i2c1_status_t result;
    /* STEP 23: Khóa IRQ, xóa notification cũ, kiểm tra bus trước START.
     * Bus kẹt thì recovery và trả lỗi để client chủ động thử giao dịch lại.
     */
    i2c1_irq_disable();
    (void)ulTaskNotifyTake(pdTRUE, 0u);
    if (!i2c1_wait_bus_free()) {
        result = i2c1_recover_locked() ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;
        goto release_bus;
    }

    /* STEP 24: Bus đã rảnh: điền context, mở nguồn ngắt CR2 rồi START.
     * Critical section ngăn ISR thấy context chưa hoàn chỉnh.
     */
    taskENTER_CRITICAL();
    if ((I2C1->SR1 & I2C_SR1_RXNE) != 0u) {
        (void)I2C1->DR;
    }
    I2C1->SR1 = 0u;
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
    s_i2c1 = (i2c1_context_t){
        .state = I2C1_STATE_START,
        .result = I2C1_ERR_TIMEOUT,
        .owner = xTaskGetCurrentTaskHandle(),
        .address_byte = (uint8_t)(((uint32_t)addr7 << 1u) |
                                (rx_data != NULL ? 1u : 0u)),
        .tx_data = tx_data,
        .rx_data = rx_data,
        .length = length
    };
    i2c1_irq_enable();
    I2C1->CR2 |= I2C1_IRQ_MASK;
    I2C1->CR1 |= I2C_CR1_START;
    taskEXIT_CRITICAL();

    /* STEP 25: Chờ notification ngoài critical section để task được Blocked.
     * Khi thức dậy, khóa IRQ rồi chốt kết quả và tách buffer khỏi ISR.
     */
    (void)ulTaskNotifyTake(pdTRUE, wait_ticks);
    taskENTER_CRITICAL();
    i2c1_irq_disable();
    /* ISR đã hoàn tất ở biên timeout thì giữ kết quả của ISR. */
    result = s_i2c1.state == I2C1_STATE_STOP
             ? s_i2c1.result : I2C1_ERR_TIMEOUT;
    const uint32_t errors =
        s_i2c1.error_flags | (I2C1->SR1 & I2C1_ERROR_MASK);
    if (errors != 0u) {
        result = errors == I2C_SR1_AF ? I2C1_ERR_NACK : I2C1_ERR_BUS;
    }
    /* Ngắt quyền truy cập buffer trước khi hàm có thể trả về. */
    s_i2c1.owner = NULL;
    s_i2c1.tx_data = NULL;
    s_i2c1.rx_data = NULL;
    taskEXIT_CRITICAL();
    (void)ulTaskNotifyTake(pdTRUE, 0u);

    /* STEP 26: Xử lý STOP/lỗi/recovery xong mới trả mutex.
     * Chờ mutex, notification, bus rảnh và recovery có timeout riêng.
     */
    if ((errors & I2C_SR1_ARLO) != 0u) {
        /* Mất quyền Master: không ép STOP hoặc tạo xung GPIO. */
        (void)i2c1_wait_bus_free();
        /* Hệ một Master: ARLO do nhiễu SDA; chờ bus xong rồi recovery để đồng bộ lại slave. */
        result = i2c1_recover_locked() ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;
    } else if (result == I2C1_ERR_TIMEOUT || result == I2C1_ERR_BUS ||
               !i2c1_wait_bus_free()) {
        result = i2c1_recover_locked() ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;
    } else {
        I2C1->CR1 = I2C_CR1_PE;
    }

release_bus:
    s_i2c1 = (i2c1_context_t){ .state = I2C1_STATE_IDLE, .result = result };
    i2c1_irq_enable();
    const BaseType_t released = xSemaphoreGive(s_bus_mutex);
    configASSERT(released == pdTRUE);
    return released == pdTRUE ? result : I2C1_ERR_BUS;
}

/* STEP 27: API write/read dùng chung transfer; addr7 chưa dịch trái.
 * Cặp write chọn register rồi read phải được client bảo vệ liền mạch.
 */
i2c1_status_t i2c1_master_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    return i2c1_transfer(addr7, data, NULL, len);
}

i2c1_status_t i2c1_master_read(uint8_t addr7, uint8_t *data, uint16_t len)
{
    return i2c1_transfer(addr7, NULL, data, len);
}

/* STEP 28: Vector table nối IRQ EV/ER tới hai handler để chạy FSM/báo lỗi. */
void I2C1_EV_IRQHandler(void)
{
#if W2E1_TRACE_EV
    GPIOB->BSRR = GPIO_BSRR_BS13;
#endif
    i2c1_on_event_irq();
#if W2E1_TRACE_EV
    GPIOB->BRR = GPIO_BRR_BR13;
#endif
}

void I2C1_ER_IRQHandler(void)
{
#if W2E1_TRACE_EV
    GPIOB->BSRR = GPIO_BSRR_BS14;
#endif
    i2c1_on_error_irq();
#if W2E1_TRACE_EV
    GPIOB->BRR = GPIO_BRR_BR14;
#endif
}
