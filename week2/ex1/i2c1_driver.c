#include "i2c1_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "stm32f1xx.h"
#include "params.h"

#define I2C1_IRQ_MASK \
    (I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN)
#define I2C1_ERROR_MASK \
    (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)

/* Gọi read/write và recovery từ task, không gọi từ ISR.
 * Mỗi task client dành notification index 0 riêng cho driver này.
 * I2C1 dùng PB6/PB7; STM32 là Master duy nhất trên bus.
 * Gọi init một lần trước khi khởi động scheduler.
 */
_Static_assert(configSUPPORT_STATIC_ALLOCATION == 1,
               "I2C1 requires static allocation");
_Static_assert(configUSE_MUTEXES == 1 &&
               configUSE_TASK_NOTIFICATIONS == 1,
               "I2C1 requires mutexes and task notifications");
_Static_assert(configMAX_SYSCALL_INTERRUPT_PRIORITY != 0,
               "I2C1 requires a nonzero syscall interrupt priority");

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

/* STEP 4a: Quản lý ngắt và đổi timeout từ ms sang tick.
 * EV/ER dùng cùng priority để không ngắt lẫn nhau.
 * NVIC có thể bật, nhưng CR2 chỉ cho phát ngắt khi có giao dịch.
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

static TickType_t i2c1_timeout_ticks(void)
{
    const TickType_t ticks = pdMS_TO_TICKS(W2E1_I2C_TIMEOUT_MS);
    return ticks != 0u ? ticks : (TickType_t)1u;
}

/* STEP 4: Khởi tạo driver một lần trước vTaskStartScheduler().
 * Đưa context về IDLE; tạo mutex static ở trạng thái available.
 * Cấu hình GPIO/timing, xóa pending tại NVIC rồi bật CR1.PE.
 * Cấu hình priority hợp lệ cho FreeRTOS và bật NVIC.
 * Nguồn ngắt CR2 chỉ bật khi task đã chuẩn bị xong giao dịch.
 */
void i2c1_driver_init(void)
{
    configASSERT(s_bus_mutex == NULL);
    configASSERT(SystemCoreClock == 72000000UL);
    configASSERT((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV2);
    configASSERT(NVIC_GetPriorityGrouping() <= (7u - __NVIC_PRIO_BITS));

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
    configASSERT((AFIO->MAPR & AFIO_MAPR_I2C1_REMAP) == 0u);

    const uint32_t irq_priority =
        configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8u - __NVIC_PRIO_BITS);
    NVIC_SetPriority(I2C1_EV_IRQn, irq_priority);
    NVIC_SetPriority(I2C1_ER_IRQn, irq_priority);

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    I2C1->CR1 = I2C_CR1_PE;
    i2c1_irq_enable();
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

/* STEP 5a: Chờ giao dịch kết thúc thật trên dây trước khi trả mutex.
 * Kiểm tra START/STOP đã xóa, BUSY = 0 và SCL/SDA đều HIGH.
 * Dùng DWT giới hạn thời gian chờ; ngắt ngoại vi khác vẫn hoạt động.
 */
static bool i2c1_wait_bus_free(void)
{
    const uint32_t lines = GPIO_IDR_IDR6 | GPIO_IDR_IDR7;
    const uint32_t start = DWT->CYCCNT;
    const uint32_t timeout =
        (SystemCoreClock / 1000UL) * W2E1_I2C_TIMEOUT_MS;

    do {
        if ((I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0u &&
            (I2C1->SR2 & I2C_SR2_BUSY) == 0u &&
            (GPIOB->IDR & lines) == lines) {
            i2c1_delay_us(5u);
            return (GPIOB->IDR & lines) == lines &&
                   (I2C1->SR2 & I2C_SR2_BUSY) == 0u;
        }
    } while ((uint32_t)(DWT->CYCCNT - start) < timeout);

    return false;
}

/* STEP 6: Chuyển SCL/SDA sang GPIO để chuẩn bị phục hồi bus.
 * Bên gọi đang giữ mutex; tắt ngắt và PE rồi reset I2C1.
 * Nhả hai chân trước khi đổi sang GPIO open-drain 2 MHz.
 * Sau recovery phải cấu hình lại timing và trả chân về AF.
 */
static void i2c1_gpio_takeover(void)
{
    i2c1_irq_disable();
    I2C1->CR1 &= ~I2C_CR1_PE;

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
    if ((GPIOB->IDR & GPIO_IDR_IDR7) != 0u) {
        return false;
    }

    GPIOB->BRR = GPIO_BRR_BR6;
    i2c1_delay_us(5u);
    if ((GPIOB->IDR & GPIO_IDR_IDR6) != 0u) {
        return false;
    }
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
 * Khôi phục NVIC; nguồn ngắt CR2 tắt khi không có giao dịch.
 * ERR_RECOVERED không xác nhận dữ liệu cũ hợp lệ.
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
    i2c1_irq_enable();

    return recovered;
}

/* STEP 11: Hàm recovery public, gọi từ task-context sau khi driver init.
 * Chờ lấy mutex có timeout; lấy được mới thay đổi GPIO/I2C1.
 * Gọi recover_locked rồi trả mutex, kể cả khi recovery thất bại.
 * Bên gọi hàm này chưa được giữ mutex; bên đã giữ dùng recover_locked.
 */
bool i2c1_bus_recover(void)
{
    if (s_bus_mutex == NULL || __get_IPSR() != 0u ||
        __get_PRIMASK() != 0u || __get_BASEPRI() != 0u) {
        return false;
    }

    const TickType_t wait_ticks = i2c1_timeout_ticks();

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
 * Các nhánh dưới xử lý EV6, TXE/BTF và kết thúc nhận bằng RXNE.
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

    /* STEP 16: Master receive một byte.
     * ADDR: POS = 0, ACK = 0; xóa ADDR rồi yêu cầu STOP.
     * Bảo vệ chuỗi cấu hình bằng PRIMASK, sau đó khôi phục.
     * RXNE: đọc byte vào buffer rồi báo hoàn tất cho task.
     */
    if (s_i2c1.state == I2C1_STATE_ADDRESS &&
        (sr1 & I2C_SR1_ADDR) != 0u &&
        (s_i2c1.address_byte & 1u) != 0u &&
        s_i2c1.length == 1u) {
        const uint32_t saved_primask = __get_PRIMASK();
        __disable_irq();

        s_i2c1.state = I2C1_STATE_RX;
        I2C1->CR1 &= ~(I2C_CR1_POS | I2C_CR1_ACK);
        (void)I2C1->SR2;
        I2C1->CR1 |= I2C_CR1_STOP;

        __set_PRIMASK(saved_primask);
        return;
    }

    if (s_i2c1.state == I2C1_STATE_RX &&
        s_i2c1.length == 1u &&
        (sr1 & I2C_SR1_RXNE) != 0u) {
        s_i2c1.rx_data[0] = (uint8_t)I2C1->DR;
        s_i2c1.position = 1u;
        i2c1_complete_from_isr(I2C1_OK);
        return;
    }
    /* STEP 17: EV6 cho Master receive hai byte.
     * POS = 1: NACK áp dụng cho byte thứ hai.
     * ACK = 1 trước khi xóa ADDR; sau đó chuyển ACK = 0.
     * Tắt ITBUFEN để chờ BTF, không xử lý riêng từng RXNE.
     */
    if (s_i2c1.state == I2C1_STATE_ADDRESS &&
        (sr1 & I2C_SR1_ADDR) != 0u &&
        (s_i2c1.address_byte & 1u) != 0u &&
        s_i2c1.length == 2u) {
        const uint32_t saved_primask = __get_PRIMASK();
        __disable_irq();

        s_i2c1.state = I2C1_STATE_RX;
        I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
        I2C1->CR1 |= I2C_CR1_POS | I2C_CR1_ACK;
        (void)I2C1->SR2;
        I2C1->CR1 &= ~I2C_CR1_ACK;

        __set_PRIMASK(saved_primask);
        return;
    }

    /* STEP 18: BTF khi nhận đủ hai byte.
     * Yêu cầu STOP rồi đọc DR hai lần trong đoạn khóa ngắt.
     * Khôi phục PRIMASK trước khi báo notification cho task.
     */
    if (s_i2c1.state == I2C1_STATE_RX &&
        s_i2c1.length == 2u &&
        (sr1 & I2C_SR1_BTF) != 0u) {
        const uint32_t saved_primask = __get_PRIMASK();
        __disable_irq();

        I2C1->CR1 |= I2C_CR1_STOP;
        s_i2c1.rx_data[0] = (uint8_t)I2C1->DR;
        s_i2c1.rx_data[1] = (uint8_t)I2C1->DR;
        s_i2c1.position = 2u;

        __set_PRIMASK(saved_primask);
        i2c1_complete_from_isr(I2C1_OK);
        return;
    }

    /* STEP 19: EV6 cho Master receive trên hai byte.
     * POS = 0, ACK = 1 để tiếp tục nhận dữ liệu.
     * Tắt ITBUFEN, dùng BTF để xử lý các byte ở bước sau.
     */
    if (s_i2c1.state == I2C1_STATE_ADDRESS &&
        (sr1 & I2C_SR1_ADDR) != 0u &&
        (s_i2c1.address_byte & 1u) != 0u &&
        s_i2c1.length > 2u) {
        s_i2c1.state = I2C1_STATE_RX;
        I2C1->CR1 =
            (I2C1->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK;
        I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
        (void)I2C1->SR2;
        return;
    }
    /* STEP 20: BTF khi còn trên ba byte chưa đọc.
     * Đọc một byte, tăng position; giữ ACK = 1.
     * Ba byte cuối sẽ được xử lý riêng.
     */
    if (s_i2c1.state == I2C1_STATE_RX &&
        s_i2c1.length > 2u &&
        (sr1 & I2C_SR1_BTF) != 0u &&
        (uint16_t)(s_i2c1.length - s_i2c1.position) > 3u) {
        s_i2c1.rx_data[s_i2c1.position] =
            (uint8_t)I2C1->DR;
        ++s_i2c1.position;
        return;
    }
    /* STEP 21: BTF khi còn ba byte cuối.
     * ACK = 0; đọc N-2, yêu cầu STOP rồi đọc N-1.
     * Bật ITBUFEN để nhận ngắt RXNE cho byte cuối.
     */
    if (s_i2c1.state == I2C1_STATE_RX &&
        s_i2c1.length > 2u &&
        (sr1 & I2C_SR1_BTF) != 0u &&
        (uint16_t)(s_i2c1.length - s_i2c1.position) == 3u) {
        const uint32_t saved_primask = __get_PRIMASK();
        __disable_irq();

        I2C1->CR1 &= ~I2C_CR1_ACK;
        s_i2c1.rx_data[s_i2c1.position] = (uint8_t)I2C1->DR;
        ++s_i2c1.position;

        I2C1->CR1 |= I2C_CR1_STOP;
        s_i2c1.rx_data[s_i2c1.position] = (uint8_t)I2C1->DR;
        ++s_i2c1.position;

        I2C1->CR2 |= I2C_CR2_ITBUFEN;
        __set_PRIMASK(saved_primask);
        return;
    }

    /* STEP 22: Đọc byte cuối của giao dịch nhận trên hai byte.
     * STEP 21 đã yêu cầu STOP; đọc DR rồi báo hoàn tất cho task.
     */
    if (s_i2c1.state == I2C1_STATE_RX &&
        s_i2c1.length > 2u &&
        (uint16_t)(s_i2c1.length - s_i2c1.position) == 1u &&
        (sr1 & I2C_SR1_RXNE) != 0u) {
        s_i2c1.rx_data[s_i2c1.position] = (uint8_t)I2C1->DR;
        ++s_i2c1.position;
        i2c1_complete_from_isr(I2C1_OK);
        return;
    }
}

/* STEP 23: Luồng giao dịch dùng chung cho read và write.
 * Lấy mutex trước khi chạm vào bus; giữ đến hết STOP hoặc recovery.
 * Không dùng notification index 0 của task client cho công việc khác.
 * Mỗi lời gọi là một transaction; cặp ghi địa chỉ register rồi đọc
 * phải được các task client bảo vệ liền mạch ở lớp ứng dụng.
 */
static i2c1_status_t i2c1_transfer(uint8_t addr7,
                                const uint8_t *tx_data,
                                uint8_t *rx_data,
                                uint16_t length)
{
    if (s_bus_mutex == NULL || addr7 > 0x7fu || length == 0u ||
        ((tx_data == NULL) == (rx_data == NULL)) ||
        __get_IPSR() != 0u || __get_PRIMASK() != 0u ||
        __get_BASEPRI() != 0u) {
        return I2C1_ERR_BUS;
    }

    const TickType_t wait_ticks = i2c1_timeout_ticks();
    if (xSemaphoreTake(s_bus_mutex, wait_ticks) != pdTRUE) {
        return I2C1_ERR_TIMEOUT;
    }

    i2c1_status_t result = I2C1_ERR_BUS;
    i2c1_irq_disable();
    (void)ulTaskNotifyTake(pdTRUE, 0u);

    if (!i2c1_wait_bus_free()) {
        result = i2c1_recover_locked()
                 ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;
        goto release_bus;
    }

    /* STEP 24: Chuẩn bị đủ context trước khi bật nguồn ngắt và START.
     * Xóa notification cũ; đặt owner là task đang gọi.
     * Chỉ đặt lại ACK/POS sau khi STOP của giao dịch trước đã xong.
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

    /* STEP 25: Task ngủ chờ notification, không polling từng byte.
     * Nếu ISR vừa hoàn tất ở biên timeout, dùng kết quả ISR đã lưu.
     * Khóa ngắt I2C khi chốt kết quả để tránh tranh chấp với ISR.
     */
    (void)ulTaskNotifyTake(pdTRUE, wait_ticks);

    taskENTER_CRITICAL();
    i2c1_irq_disable();
    result = s_i2c1.state == I2C1_STATE_STOP
             ? s_i2c1.result : I2C1_ERR_TIMEOUT;
    const uint32_t errors =
        s_i2c1.error_flags | (I2C1->SR1 & I2C1_ERROR_MASK);
    if (errors != 0u) {
        result = errors == I2C_SR1_AF ? I2C1_ERR_NACK : I2C1_ERR_BUS;
    }

    /* Xóa owner và buffer để ISR đến muộn không truy cập dữ liệu cũ. */
    s_i2c1.owner = NULL;
    s_i2c1.tx_data = NULL;
    s_i2c1.rx_data = NULL;
    taskEXIT_CRITICAL();
    (void)ulTaskNotifyTake(pdTRUE, 0u);

    /* STEP 26: Chốt STOP hoặc phục hồi bus trước khi trả mutex.
     * ARLO: không ép STOP hay toggle SCL khi đã mất quyền Master.
     * Recovery có timeout riêng; thành công không làm dữ liệu cũ hợp lệ.
     */
    if ((errors & I2C_SR1_ARLO) != 0u) {
        (void)i2c1_wait_bus_free();
        i2c1_hw_configure();
        I2C1->CR1 = I2C_CR1_PE;
        result = I2C1_ERR_BUS;
    } else if (result == I2C1_ERR_TIMEOUT || result == I2C1_ERR_BUS) {
        result = i2c1_recover_locked()
                 ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;
    } else if (!i2c1_wait_bus_free()) {
        result = i2c1_recover_locked()
                 ? I2C1_ERR_RECOVERED : I2C1_ERR_BUS;
    } else {
        I2C1->CR1 = I2C_CR1_PE;
    }

release_bus:
    s_i2c1 = (i2c1_context_t){
        .state = I2C1_STATE_IDLE,
        .result = result
    };
    i2c1_irq_enable();

    const BaseType_t released = xSemaphoreGive(s_bus_mutex);
    configASSERT(released == pdTRUE);
    return released == pdTRUE ? result : I2C1_ERR_BUS;
}

/* STEP 27: Giữ đúng interface read/write đã khai báo trong repo.
 * Write dùng tx_data; read dùng rx_data; cả hai đi qua cùng mutex.
 */
i2c1_status_t i2c1_master_write(uint8_t addr7,
                              const uint8_t *data,
                              uint16_t len)
{
    return i2c1_transfer(addr7, data, NULL, len);
}

i2c1_status_t i2c1_master_read(uint8_t addr7,
                             uint8_t *data,
                             uint16_t len)
{
    return i2c1_transfer(addr7, NULL, data, len);
}

/* STEP 28: Hàm vào ISR chuyển tiếp đến phần xử lý của driver.
 * Vector table phải trỏ đúng hai hàm này.
 * Chỉ định nghĩa tên hàm chưa thay được Default_Handler trong startup.c.
 */
void I2C1_EV_IRQHandler(void)
{
    i2c1_on_event_irq();
}

void I2C1_ER_IRQHandler(void)
{
    i2c1_on_error_irq();
}
