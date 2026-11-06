#include "FreeRTOS.h"
#include "task.h"
#include "params.h"
#include "tickless_rtc.h"
#include "uart.h"

/* STEP 1: Cấp phát toàn bộ stack và khối điều khiển tác vụ ngay lúc liên kết. */
#define PERIOD_MS           1000u
/* Mặc định chạy bận 20 ms mỗi giây. Khi đo dòng, biên dịch với -DW3E2_ACTIVE_MS=0u
 * (chỉ đo STOP) hoặc -DW3E2_ACTIVE_MS=900u (gần như chỉ đo Active). */
#ifndef W3E2_ACTIVE_MS
#define W3E2_ACTIVE_MS      20u
#endif
#define ACTIVE_MS           W3E2_ACTIVE_MS
#define LOG_PERIOD_MS       60000u
#define TEST_WINDOW_MS      (W3E2_DRIFT_TEST_WINDOW_MIN * 60000u)
#define APP_STACK_WORDS     256u

static StaticTask_t s_idle_tcb;
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t s_app_tcb;
static StackType_t s_app_stack[APP_STACK_WORDS];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *stack_words)
{
    *tcb = &s_idle_tcb;
    *stack = s_idle_stack;
    *stack_words = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    configASSERT(false);
}

/* STEP 2: Chốt mốc tick trước khi phát bản tin. RTC và số lần STOP giúp
 * đối chiếu nguồn xung và phát hiện firmware đang chạy bằng SysTick dự phòng. */
static void log_sample(const char *event, uint32_t tick_ms)
{
    const tickless_rtc_stats_t stats = tickless_rtc_get_stats();

    uart1_write_string("W3E2 event=");
    uart1_write_string(event);
    uart1_write_string(" tick_ms=");
    uart1_write_u32(tick_ms);
    uart1_write_string(" rtc_cnt=");
    uart1_write_u32(stats.rtc_cnt);
    uart1_write_string(" sleep_entries=");
    uart1_write_u32(stats.sleep_entries);
    uart1_write_string(" source=");
    uart1_write_string(stats.uses_lse ? "LSE" : "LSI");
    uart1_write_string(" cal_freq_hz=");
    uart1_write_u32((stats.cal_freq_millihz + 500u) / 1000u);
    uart1_write_string(" cal_freq_millihz=");
    uart1_write_u32(stats.cal_freq_millihz);
    uart1_write_string(" drift_bound_ppm=");
    uart1_write_u32(W3E2_DRIFT_BOUND_PPM);
    uart1_write_string(" rtc_faults=");
    uart1_write_u32(stats.rtc_faults);
    uart1_write_string(" late_wakes=");
    uart1_write_u32(stats.late_wakes);
    uart1_write_string(" recals=");
    uart1_write_u32(stats.recalibrations);
    uart1_write_string("\r\n");
    uart1_flush();
}

/* STEP 3: Mỗi giây chạy bận 20 ms rồi block đến mốc phát hành tuyệt đối.
 * In START, từng phút và DONE đúng một lần, sau đó tiếp tục duy trì tải đo dòng. */
static void measurement_task(void *context)
{
    (void)context;
    const TickType_t start = xTaskGetTickCount();
    const uint32_t active_cycles = (SystemCoreClock / 1000u) * ACTIVE_MS;
    TickType_t release = start;
    uint32_t next_log_ms = LOG_PERIOD_MS;
    bool window_done = false;

    log_sample("START", 0u);
    for (;;) {
        const uint32_t active_start = DWT->CYCCNT;
        const uint32_t elapsed_ms = (uint32_t)(xTaskGetTickCount() - start);

        if (!window_done && elapsed_ms >= TEST_WINDOW_MS) {
            log_sample("DONE", elapsed_ms);
            window_done = true;
        } else if (!window_done && elapsed_ms >= next_log_ms) {
            log_sample("SAMPLE", elapsed_ms);
            next_log_ms += LOG_PERIOD_MS;
        }

        /* Thời gian phát log cũng nằm trong ngân sách Active nếu gửi xong trong 20 ms. */
        while ((uint32_t)(DWT->CYCCNT - active_start) < active_cycles) {
            __NOP();
        }
        (void)xTaskDelayUntil(&release, pdMS_TO_TICKS(PERIOD_MS));
    }
}

/* STEP 4: UART1 chạy 115200 baud từ cây xung 72 MHz, sau đó hiệu chuẩn RTC
 * trước khi giao quyền chạy cho bộ lập lịch và tác vụ đo tĩnh. */
int main(void)
{
    /* Gọi hàm có tác dụng phụ ngoài configASSERT để vẫn chạy nếu macro bị tắt. */
    const bool uart_ready = uart1_init(115200u);
    configASSERT(uart_ready);
    tickless_rtc_init();
    const TaskHandle_t app = xTaskCreateStatic(measurement_task, "Measure", APP_STACK_WORDS,
                                               NULL, tskIDLE_PRIORITY + 1u, s_app_stack,
                                               &s_app_tcb);
    configASSERT(app != NULL);
    vTaskStartScheduler();
    configASSERT(false);
    return 0;
}
