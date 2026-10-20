#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "i2c1_driver.h"
#include "params.h"
#include "uart.h"

/* STEP 1: Bộ nhớ static cho Idle task. */
static StaticTask_t s_idle_tcb;
_Alignas(8) static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb, StackType_t **stack, configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &s_idle_tcb;
    *stack = s_idle_stack;
    *depth = configMINIMAL_STACK_SIZE; // Đơn vị words.
}

/* STEP 2: Dừng để debug khi kernel phát hiện stack overflow. */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;

    __disable_irq();
    for (;;) {
        __NOP();
    }
}

/* STEP 3: Bộ nhớ riêng cho hai task client. */
#define CLIENT_COUNT       2u
#define CLIENT_STACK_WORDS 256u // Mỗi stack = 1024 bytes.
#define CLIENT_PERIOD_MS   100u // Khoảng nghỉ giữa hai lượt thử nghiệm.

static StaticTask_t s_client_tcbs[CLIENT_COUNT];

_Alignas(8) static StackType_t
    s_client_stacks[CLIENT_COUNT][CLIENT_STACK_WORDS];

static TaskHandle_t s_client_handles[CLIENT_COUNT];

/* STEP 4: Register và cấu hình MPU6050. */
#define MPU_ADDR7           0x68u // AD0 LOW; đổi thành 0x69u nếu AD0 HIGH.
#define MPU_REG_PWR_MGMT_1   0x6Bu
#define MPU_REG_WHO_AM_I     0x75u
#define MPU_REG_ACCEL_XOUT_H 0x3Bu
#define MPU_REG_GYRO_XOUT_H  0x43u
#define MPU_SAMPLE_BYTES    6u    // XYZ, mỗi trục hai bytes High/Low.

typedef struct {
    uint8_t start_register;
    const char *name;
} client_config_t;

static client_config_t s_client_config[CLIENT_COUNT] = {
    { MPU_REG_ACCEL_XOUT_H, "ACCEL" },
    { MPU_REG_GYRO_XOUT_H,  "GYRO"  }
};

/* STEP 5: Mutex lớp MPU bảo vệ cặp write/read và log của hai client. */
static StaticSemaphore_t s_mpu_mutex_storage;
static SemaphoreHandle_t s_mpu_mutex;

static void mpu_mutex_init(void)
{
    s_mpu_mutex = xSemaphoreCreateMutexStatic(&s_mpu_mutex_storage);
    configASSERT(s_mpu_mutex != NULL);
}

/* STEP 6: Bên gọi đã giữ mutex MPU; không lấy lại mutex trong hàm này.
 * Hai API driver hiện tại tạo hai transaction, có STOP giữa write/read.
 */
static i2c1_status_t mpu_read_registers_locked(
    uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t length)
{
    i2c1_status_t result = i2c1_master_write(addr7, &reg, 1u);

    if (result == I2C1_OK) {
        result = i2c1_master_read(addr7, data, length);
    }

    return result;
}

/* STEP 7: Kiểm tra WHO_AM_I rồi đánh thức cảm biến; chạy từ task. */
static i2c1_status_t mpu_prepare_locked(void)
{
    uint8_t who = 0u;

    i2c1_status_t result = mpu_read_registers_locked(
        MPU_ADDR7, MPU_REG_WHO_AM_I, &who, 1u);

    if (result != I2C1_OK) {
        return result;
    }

    if (who != 0x68u) { // WHO_AM_I vẫn là 0x68 khi AD0 HIGH.
        return I2C1_ERR_BUS;
    }

    const uint8_t command[] = { MPU_REG_PWR_MGMT_1, 0x01u };

    // 0x01: SLEEP = 0, CLKSEL = 1, chọn PLL gyro trục X.
    return i2c1_master_write(MPU_ADDR7, command, sizeof command);
}

/* STEP 8: Đổi mã trạng thái thành chữ để đọc log UART. */
static const char *status_name(i2c1_status_t status)
{
    switch (status) {
    case I2C1_OK:            return "OK";
    case I2C1_ERR_TIMEOUT:   return "TIMEOUT";
    case I2C1_ERR_NACK:      return "NACK";
    case I2C1_ERR_BUS:       return "BUS";
    case I2C1_ERR_RECOVERED: return "RECOVERED";
    default:                return "UNKNOWN";
    }
}

/* STEP 9: Chỉ xác nhận recovery sau khi một lần đọc mẫu tiếp theo OK.
 * Các biến này chỉ được truy cập khi đã giữ mutex MPU.
 */
static uint32_t s_recovery_streak;
static bool s_recovery_pending;

static void recovery_update_locked(i2c1_status_t result)
{
    if (result == I2C1_ERR_RECOVERED) {
        if (s_recovery_pending) {
            s_recovery_streak = 0u; // Recovery trước chưa được xác nhận.
        }

        s_recovery_pending = true;
    } else if (result == I2C1_OK) {
        if (s_recovery_pending) {
            ++s_recovery_streak;
            s_recovery_pending = false;
        }
    } else {
        s_recovery_streak = 0u; // Một lần lỗi làm đứt chuỗi thành công.
        s_recovery_pending = false;
    }
}

/* STEP 10: In trọn một dòng khi đang giữ mutex, tránh log bị xen kẽ.
 * B0..B5 là bytes thô dạng thập phân; lỗi thì để trống các cột dữ liệu.
 */
static void log_result_locked(
    const char *name, i2c1_status_t result, uint32_t us, const uint8_t *data)
{
    uart1_write_string(name);
    uart1_write_char(',');
    uart1_write_string(status_name(result));
    uart1_write_char(',');
    uart1_write_u32(us);
    uart1_write_char(',');
    uart1_write_u32(s_recovery_streak);
    uart1_write_char(',');
    uart1_write_u32(s_recovery_pending ? 1u : 0u);

    for (uint32_t i = 0u; i < MPU_SAMPLE_BYTES; ++i) {
        uart1_write_char(',');

        if (result == I2C1_OK && data != NULL) {
            uart1_write_u32(data[i]);
        }
    }

    uart1_write_string("\r\n");
}

/* STEP 11: Hai task dùng chung hàm này, khác start_register và name. */
static bool s_mpu_ready; // Chỉ đọc/ghi khi đang giữ mutex MPU.

static void client_task(void *argument)
{
    const client_config_t *config = argument;
    uint8_t data[MPU_SAMPLE_BYTES]; // Buffer riêng trên stack của từng task.

    vTaskDelay(pdMS_TO_TICKS(100u)); // Chờ cảm biến sau khi cấp nguồn.

    for (;;) {
        const TickType_t wait_ticks = pdMS_TO_TICKS(W2E1_I2C_TIMEOUT_MS);

        if (xSemaphoreTake(s_mpu_mutex, wait_ticks) != pdTRUE) {
            // Client khác còn giữ khóa; bỏ lượt này, chưa có lỗi bus.
            vTaskDelay(pdMS_TO_TICKS(CLIENT_PERIOD_MS));
            continue;
        }

        const bool initializing = !s_mpu_ready;
        const uint32_t start = DWT->CYCCNT;
        i2c1_status_t result;

        if (initializing) {
            result = mpu_prepare_locked();

            if (result == I2C1_OK) {
                // Giữ mutex để task còn lại chưa đọc khi cảm biến đang ổn định.
                vTaskDelay(pdMS_TO_TICKS(100u));
                s_mpu_ready = true;
            }
        } else {
            result = mpu_read_registers_locked(
                MPU_ADDR7, config->start_register, data, sizeof data);

            recovery_update_locked(result);
        }

        const uint32_t elapsed_us =
            (uint32_t)(DWT->CYCCNT - start) / (SystemCoreClock / 1000000u);

        log_result_locked(initializing ? "INIT" : config->name,
                          result, elapsed_us, initializing ? NULL : data);

        const BaseType_t released = xSemaphoreGive(s_mpu_mutex);
        configASSERT(released == pdTRUE);

        vTaskDelay(pdMS_TO_TICKS(CLIENT_PERIOD_MS));
    }
}

/* STEP 12: Cấu hình phần cứng, tạo object/task rồi chạy scheduler. */
int main(void)
{
    SCB->VTOR = FLASH_BASE; // Vector table ở 0x08000000 theo linker hiện tại.
    NVIC_SetPriorityGrouping(3u); // Cả 4 bit priority dùng cho preemption.
    __DSB();
    __ISB();

    configASSERT(SystemCoreClock == 72000000u);

    const bool uart_ready = uart1_init(115200u);
    configASSERT(uart_ready);

    i2c1_driver_init(); // Chỉ cấu hình; giao dịch blocking chạy trong task.
    mpu_mutex_init();

    uart1_write_string("# W2E1,100kHz,RECOVERY_TARGET=");
    uart1_write_u32(W2E1_RECOVERY_REPEAT_COUNT);
    uart1_write_string("\r\nCLIENT,STATUS,US,RECOVERY_STREAK,RECOVERY_PENDING,"
                       "B0,B1,B2,B3,B4,B5\r\n");

    for (uint32_t i = 0u; i < CLIENT_COUNT; ++i) {
        s_client_handles[i] = xTaskCreateStatic(
            client_task,
            s_client_config[i].name,
            CLIENT_STACK_WORDS,
            &s_client_config[i],
            (UBaseType_t)(i + 1u),
            s_client_stacks[i],
            &s_client_tcbs[i]);

        configASSERT(s_client_handles[i] != NULL);
    }

    vTaskStartScheduler();

    configASSERT(0); // Scheduler chạy thành công sẽ không trả về.
    for (;;) {
        __NOP();
    }
}