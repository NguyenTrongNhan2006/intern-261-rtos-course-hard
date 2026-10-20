#include "i2c1_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "stm32f1xx.h"
#include "params.h"

/* STEP 3: Cấu hình timing Standard-mode 100 kHz với PCLK1 = 36 MHz.
 * Bật clock và reset I2C1; CR2.FREQ = 36; CCR = 180; TRISE = 37.
 * OAR1[14] phải bằng 1; PE vẫn bằng 0 trong lúc cấu hình timing.
 */
 
_Static_assert(W2E1_I2C_SPEED_HZ == 100000UL,
               "This configuration requires Standard-mode 100 kHz");

static void i2c1_hw_configure(void)
{
    const uint32_t pclk1_hz = 36000000UL;

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;

    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    I2C1->CR2 = pclk1_hz / 1000000UL;
    I2C1->CCR = pclk1_hz / (2UL * W2E1_I2C_SPEED_HZ);
    I2C1->TRISE = (pclk1_hz / 1000000UL) + 1UL;
    I2C1->OAR1 = 1UL << 14;
}

/* STEP 2: Cấu hình chân I2C1 mặc định: PB6 = SCL, PB7 = SDA.
 * Bật clock AFIO/GPIOB; nạp output latch lên 1 trước khi đổi mode.
 * CNF = 11: Alternate Function open-drain; MODE = 10: output speed 2 MHz. (RM0008)
 * Mức HIGH trên bus cần pull-up bên ngoài.
 */
static void i2c1_gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN;
    (void)RCC->APB2ENR;

    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;

    GPIOB->CRL =
        (GPIOB->CRL &
         ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
           GPIO_CRL_MODE7 | GPIO_CRL_CNF7)) |
        GPIO_CRL_MODE6_1 | GPIO_CRL_CNF6 |
        GPIO_CRL_MODE7_1 | GPIO_CRL_CNF7;
}

/* STEP 1: FSM, context và đối tượng đồng bộ.
 * state lưu pha giao dịch; result lưu kết quả; owner là task nhận notification.
 * tx_data/rx_data là buffer; length/position quản lý số byte.
 * static giữ dữ liệu suốt chương trình; volatile không thay thế mutex.
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
    uint32_t error_flags; /* Lưu các cờ lỗi SR1 của giao dịch hiện tại. */
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

/* STEP 4: Khởi tạo driver một lần trước vTaskStartScheduler().
 * Đưa context về IDLE; tạo mutex static ở trạng thái available.
 * Cấu hình GPIO/timing, xóa pending tại NVIC rồi bật CR1.PE.
 * Hiện IRQ I2C1 vẫn disable; chưa phát START nên chưa có giao dịch.
 */
void i2c1_driver_init(void)
{
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);

    s_i2c1 = (i2c1_context_t){
        .state = I2C1_STATE_IDLE,
        .result = I2C1_OK
    };

    s_bus_mutex = xSemaphoreCreateMutexStatic(&s_mutex_storage);
    configASSERT(s_bus_mutex != NULL);

    i2c1_gpio_init();
    i2c1_hw_configure();

    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);

    I2C1->CR1 = I2C_CR1_PE;
}

/* STEP 5: Delay microsecond bằng bộ đếm DWT->CYCCNT.
 * Core clock 72 MHz tương ứng 72 cycle/us; 5 us tương ứng 360 cycle.
 * Không reset CYCCNT; phép trừ unsigned xử lý được một lần wrap.
 * Đây là busy-wait; ngắt hoặc preemption có thể kéo dài khoảng chờ.
 */
static void i2c1_delay_us(uint32_t us)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    const uint32_t cycles =
        (SystemCoreClock / 1000000UL) * us;
    const uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

/* STEP 6: Tiếp quản SCL/SDA bằng GPIO để chuẩn bị bus recovery.
 * Thực hiện trong task-context khi đang giữ mutex bảo vệ bus.
 * Disable IRQ và reset I2C1; đặt output latch lên 1 để nhả hai chân.
 * CNF = 01, MODE = 10: GPIO open-drain 2 MHz do CPU điều khiển.
 * Chưa tạo xung SCL, chưa phát STOP; timing I2C1 cần cấu hình lại sau đó.
 */
static void i2c1_gpio_takeover(void)
{
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);

    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;

    GPIOB->CRL =
        (GPIOB->CRL &
         ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
           GPIO_CRL_MODE7 | GPIO_CRL_CNF7)) |
        GPIO_CRL_MODE6_1 | GPIO_CRL_CNF6_0 |
        GPIO_CRL_MODE7_1 | GPIO_CRL_CNF7_0;
}

/* STEP 7: Chờ mức HIGH thực tế trên SCL/PB6 qua GPIOB->IDR.
 * DWT->CYCCNT phải đang chạy trước khi ghi nhận start_cycles.
 * start_cycles và timeout_cycles dùng chung cho toàn bộ recovery.
 * Trả true khi SCL HIGH trong hạn; false khi hết thời gian.
 */
 /* start_cycles: giá trị CYCCNT được lưu một lần khi bắt đầu tính giờ.
 * CYCCNT tiếp tục tăng; start_cycles giữ nguyên.
 * CYCCNT - start_cycles = số cycle đã trôi qua từ mốc bắt đầu.
 */
static bool i2c1_wait_scl_high(uint32_t start_cycles,
                               uint32_t timeout_cycles)
{
    while ((uint32_t)(DWT->CYCCNT - start_cycles) <
           timeout_cycles) {
        if ((GPIOB->IDR & GPIO_IDR_IDR6) != 0u) {
            return true;
        }
    }

    return false;
}

/* STEP 8: Tạo tối đa 9 xung SCL để slave nhả SDA.
 * Gọi sau GPIO takeover, khi DWT đang chạy và task đang giữ mutex.
 * Giữ SDA ở trạng thái nhả; SCL LOW 5 us rồi HIGH 5 us.
 * Mỗi lần nhả SCL phải kiểm tra IDR và dùng chung ngân sách timeout.
 * true: SDA đã HIGH; vẫn cần tạo START/STOP và khôi phục I2C1.
 */
static bool i2c1_recovery_pulses(uint32_t start_cycles,
                                uint32_t timeout_cycles)
{
    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;

    for (uint32_t pulse = 0u; pulse < 9u; ++pulse) {
        if (!i2c1_wait_scl_high(start_cycles, timeout_cycles)) {
            return false;
        }

        if ((GPIOB->IDR & GPIO_IDR_IDR7) != 0u) {
            return true;
        }

        GPIOB->BRR = GPIO_BRR_BR6;
        i2c1_delay_us(5u);
        GPIOB->BSRR = GPIO_BSRR_BS6;

        if (!i2c1_wait_scl_high(start_cycles, timeout_cycles)) {
            return false;
        }

        i2c1_delay_us(5u);
    }

    return ((uint32_t)(DWT->CYCCNT - start_cycles) <
            timeout_cycles) &&
           ((GPIOB->IDR & GPIO_IDR_IDR7) != 0u);
}

/* STEP 9: Tạo START/STOP bằng GPIO trong quá trình recovery.
 * START: SDA HIGH -> LOW khi SCL HIGH.
 * Kéo SCL LOW rồi nhả HIGH; sau đó nhả SDA để tạo STOP.
 * Khi chờ SCL thất bại, nhả cả hai chân trước khi trả false.
 * true khi SCL/SDA đều HIGH và vẫn trong ngân sách timeout.
 */
static bool i2c1_recovery_start_stop(uint32_t start_cycles,
                                    uint32_t timeout_cycles)
{
    const uint32_t lines = GPIO_IDR_IDR6 | GPIO_IDR_IDR7;

    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    if (!i2c1_wait_scl_high(start_cycles, timeout_cycles) ||
        (GPIOB->IDR & GPIO_IDR_IDR7) == 0u) {
        return false;
    }

    i2c1_delay_us(5u);
    GPIOB->BRR = GPIO_BRR_BR7;
    i2c1_delay_us(5u);

    GPIOB->BRR = GPIO_BRR_BR6;
    i2c1_delay_us(5u);
    GPIOB->BSRR = GPIO_BSRR_BS6;

    if (!i2c1_wait_scl_high(start_cycles, timeout_cycles)) {
        GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
        return false;
    }

    i2c1_delay_us(5u);
    GPIOB->BSRR = GPIO_BSRR_BS7;
    i2c1_delay_us(5u);

    return ((uint32_t)(DWT->CYCCNT - start_cycles) <
            timeout_cycles) &&
           ((GPIOB->IDR & lines) == lines);
}

/* STEP 10: Ghép recovery; bên gọi phải đang giữ mutex của bus.
 * Takeover GPIO -> tối đa 9 xung -> START/STOP, dùng chung timeout.
 * Luôn nhả chân, trả về AF open-drain và cấu hình lại I2C1.
 * Xóa context cũ, bật PE; chỉ báo recovered khi BUSY đã bằng 0.
 * IRQ vẫn disable; ERR_RECOVERED không xác nhận dữ liệu cũ hợp lệ.
 */
static bool i2c1_recover_locked(void)
{
    i2c1_gpio_takeover();
    i2c1_delay_us(5u);

    const uint32_t start = DWT->CYCCNT;
    const uint32_t timeout =
        (SystemCoreClock / 1000UL) * W2E1_I2C_TIMEOUT_MS;

    bool recovered =
        i2c1_recovery_pulses(start, timeout) &&
        i2c1_recovery_start_stop(start, timeout);

    GPIOB->BSRR = GPIO_BSRR_BS6 | GPIO_BSRR_BS7;
    i2c1_gpio_init();
    i2c1_hw_configure();

    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);

    s_i2c1 = (i2c1_context_t){
        .state = I2C1_STATE_IDLE,
        .result = I2C1_ERR_BUS
    };

    I2C1->CR1 = I2C_CR1_PE;
    recovered = recovered && ((I2C1->SR2 & I2C_SR2_BUSY) == 0u);
    s_i2c1.result = recovered ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;

    return recovered;
}

/* STEP 11: Hàm recovery public, gọi từ task-context sau khi driver init.
 * Chờ lấy mutex có timeout; lấy được mới thay đổi GPIO/I2C1.
 * Gọi recover_locked rồi trả mutex, kể cả khi recovery thất bại.
 * Bên gọi hàm này chưa được giữ mutex; bên đã giữ dùng recover_locked.
 */
bool i2c1_bus_recover(void)
{
    configASSERT(s_bus_mutex != NULL);

    const TickType_t wait_ticks =
        pdMS_TO_TICKS(W2E1_I2C_TIMEOUT_MS);

    if (xSemaphoreTake(s_bus_mutex, wait_ticks) != pdTRUE) {
        return false;
    }

    const bool recovered = i2c1_recover_locked();
    const BaseType_t released = xSemaphoreGive(s_bus_mutex);

    configASSERT(released == pdTRUE);
    return recovered && (released == pdTRUE);
}

/* STEP 12: ISR chốt kết quả rồi đánh thức task sở hữu giao dịch.
 * Tắt nguồn ngắt I2C1; lưu result, chuyển sang pha kết thúc.
 * Xóa owner để tránh báo hoàn tất lặp lại cho cùng giao dịch.
 * eIncrement tăng notification value; task nhận bằng ulTaskNotifyTake().
 * Task tiếp tục kiểm tra/dọn dẹp bus và trả mutex; ISR không trả mutex.
 */

static void i2c1_complete_from_isr(i2c1_status_t result)
{
    BaseType_t task_woken = pdFALSE;
    const TaskHandle_t owner = s_i2c1.owner;

    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN |
                   I2C_CR2_ITBUFEN |
                   I2C_CR2_ITERREN);

    if (owner == NULL) {
        return;
    }

    s_i2c1.result = result;
    s_i2c1.state = I2C1_STATE_STOP;
    s_i2c1.owner = NULL;

    (void)xTaskNotifyFromISR(owner, 0u, eIncrement, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

/* STEP 13: Error ISR đọc, lưu và xóa các cờ lỗi I2C.
 * AF đơn lẻ -> ERR_NACK; BERR/ARLO/OVR hoặc lỗi phối hợp -> ERR_BUS.
 * Cờ lỗi SR1 xóa bằng ghi 0; giữ error_flags để task xử lý nguyên nhân.
 * Chỉ yêu cầu STOP khi AF đơn lẻ, còn Master và không có START/STOP pending.
 * Báo lỗi bằng notification; recovery được thực hiện trong task-context.
 */
void i2c1_on_error_irq(void)
{
    const uint32_t error_mask =
        I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR;
    const uint32_t errors = I2C1->SR1 & error_mask;

    if (errors == 0u) {
        return;
    }

    s_i2c1.error_flags |= errors;
    I2C1->SR1 = error_mask & ~errors;

    if (errors == I2C_SR1_AF &&
        (I2C1->SR2 & I2C_SR2_MSL) != 0u &&
        (I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0u) {
        I2C1->CR1 |= I2C_CR1_STOP;
    }

    i2c1_complete_from_isr(errors == I2C_SR1_AF
                          ? I2C1_ERR_NACK : I2C1_ERR_BUS);
}

/* STEP 14: Event ISR bắt đầu xử lý EV5.
 * Ưu tiên xử lý lỗi; owner NULL thì tắt nguồn ngắt I2C1.
 * EV5: đọc SR1 thấy SB = 1, sau đó ghi byte địa chỉ vào DR.
 * Chuyển FSM từ START sang ADDRESS trước khi gửi địa chỉ.
 * EV6 và các nhánh truyền/nhận sẽ được bổ sung sau; IRQ vẫn chưa bật.
 */
void i2c1_on_event_irq(void)
{
    const uint32_t sr1 = I2C1->SR1;

    if ((sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO |
                I2C_SR1_AF | I2C_SR1_OVR)) != 0u) {
        i2c1_on_error_irq();
        return;
    }

    if (s_i2c1.owner == NULL) {
        I2C1->CR2 &= ~(I2C_CR2_ITEVTEN |
                       I2C_CR2_ITBUFEN |
                       I2C_CR2_ITERREN);
        return;
    }

    if (s_i2c1.state == I2C1_STATE_START &&
        (sr1 & I2C_SR1_SB) != 0u) {
        s_i2c1.state = I2C1_STATE_ADDRESS;
        I2C1->DR = s_i2c1.address_byte;
        return;
    }
}


     /* STEP 15: Master transmit từ EV6 đến EV8_2.
     * EV6: địa chỉ write được ACK; đọc SR2 sau SR1 để xóa ADDR.
     * TxE: nạp byte kế tiếp vào DR rồi tăng position.
     * Nạp đủ byte: tắt ITBUFEN và chờ BTF xác nhận byte cuối truyền xong.
     * BTF cuối: yêu cầu STOP rồi báo kết quả cho task bằng notification.
     */

    if (s_i2c1.state == I2C1_STATE_ADDRESS &&
        (sr1 & I2C_SR1_ADDR) != 0u &&
        (s_i2c1.address_byte & 1u) == 0u) {
        s_i2c1.state = I2C1_STATE_TX;
        (void)I2C1->SR2;
        return;
    }

    if (s_i2c1.state == I2C1_STATE_TX) {
        if (s_i2c1.position == s_i2c1.length) {
            if ((sr1 & I2C_SR1_BTF) != 0u) {
                I2C1->CR1 |= I2C_CR1_STOP;
                i2c1_complete_from_isr(I2C1_OK);
            }
            return;
        }

        if ((sr1 & I2C_SR1_TXE) != 0u) {
            I2C1->DR = s_i2c1.tx_data[s_i2c1.position];
            ++s_i2c1.position;

            if (s_i2c1.position == s_i2c1.length) {
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            }
        }
        return;
    }

  