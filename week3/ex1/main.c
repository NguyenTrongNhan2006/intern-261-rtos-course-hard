#include "FreeRTOS.h"
#include "task.h"

#include "uart.h"
#include "uart_dma_rx.h"

enum {
    PARSER_STACK_WORDS = 256,
    REPORT_STACK_WORDS = 256,
    TASK_COUNT = 3,
    REPORT_INTERVAL_MS = 1000,
    QUIET_INTERVALS = 2,
    QUIET_REPEAT_INTERVALS = 5
};

/* STEP 1: Cấp phát tĩnh stack và khối điều khiển cho Parser, Report và Idle. */
static StaticTask_t s_parser_control;
static StaticTask_t s_report_control;
static StaticTask_t s_idle_control;
static StackType_t s_parser_stack[PARSER_STACK_WORDS];
static StackType_t s_report_stack[REPORT_STACK_WORDS];
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];
static TaskHandle_t s_parser_handle;
static TaskHandle_t s_report_handle;
static TaskStatus_t s_task_status[TASK_COUNT];

void vApplicationGetIdleTaskMemory(StaticTask_t **control, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *stack_words)
{
    *control = &s_idle_control;
    *stack = s_idle_stack;
    *stack_words = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    configASSERT(0);
}

/* STEP 2: Parser Task chạy ở ưu tiên cao nhất, bật nhận sau khi scheduler đã chạy. */
static void parser_task(void *arg)
{
    (void)arg;
    uart_dma_rx_init();
    for (;;) {
        (void)uart_dma_rx_parse_once();
    }
}

/* STEP 3: Lấy hiệu hai mẫu run-time stats rồi đổi sang đơn vị 0,01%.
 * Thời gian chạy ngắt có thể được tính vào task đang bị ngắt.
 * Report tách riêng vì in log bằng polling TX cũng làm tăng busy. */
typedef struct {
    uint32_t total;
    uint32_t parser;
    uint32_t report;
    uint32_t idle;
} runtime_sample_t;

static runtime_sample_t sample_runtime(void)
{
    runtime_sample_t sample = {0};
    const TaskHandle_t idle = xTaskGetIdleTaskHandle();
    const UBaseType_t count = uxTaskGetSystemState(s_task_status, TASK_COUNT, &sample.total);
    configASSERT(count == TASK_COUNT);

    for (UBaseType_t i = 0u; i < count; ++i) {
        if (s_task_status[i].xHandle == s_parser_handle) {
            sample.parser = s_task_status[i].ulRunTimeCounter;
        } else if (s_task_status[i].xHandle == s_report_handle) {
            sample.report = s_task_status[i].ulRunTimeCounter;
        } else if (s_task_status[i].xHandle == idle) {
            sample.idle = s_task_status[i].ulRunTimeCounter;
        }
    }
    return sample;
}

static uint32_t percent_x100(uint32_t part, uint32_t total)
{
    if (total == 0u) {
        return 0u;
    }
    if (part > total) {
        part = total;
    }
    return (uint32_t)((uint64_t)part * 10000u / total);
}

static void write_field(const char *label, uint32_t value)
{
    uart1_write_string(label);
    uart1_write_u32(value);
}

/* STEP 4: In thống kê bằng task ưu tiên thấp để Parser có thể giành quyền chạy.
 * RX_QUIET chốt số liệu cộng dồn sau hai mẫu không đổi số byte, rồi in lại mỗi
 * QUIET_REPEAT_INTERVALS mẫu để vẫn bắt được log sau khi script đóng cổng COM.
 * Bộ thu vẫn hoạt động. */
static void report_task(void *arg)
{
    (void)arg;
    uart1_write_string("W3E1 READY\r\n");
    TickType_t wake = xTaskGetTickCount();
    runtime_sample_t prev = sample_runtime();
    uint32_t prev_bytes = 0u;
    unsigned quiet = 0u;
    unsigned since_quiet = 0u;
    bool seen_activity = false;
    bool quiet_reported = false;

    for (;;) {
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(REPORT_INTERVAL_MS));
        const runtime_sample_t now = sample_runtime();
        const uint32_t elapsed = now.total - prev.total;
        const uint32_t parser_pct = percent_x100(now.parser - prev.parser, elapsed);
        const uint32_t report_pct = percent_x100(now.report - prev.report, elapsed);
        const uint32_t idle_pct = percent_x100(now.idle - prev.idle, elapsed);
        prev = now;

        uart_dma_rx_stats_t stats;
        uart_dma_rx_get_stats(&stats);
        if (stats.bytes_received != prev_bytes) {
            quiet = 0u;
            seen_activity = true;
            quiet_reported = false;
        } else if (quiet < QUIET_INTERVALS) {
            ++quiet;
        }
        prev_bytes = stats.bytes_received;

        if (stats.dma_errors != 0u || stats.dma_late_events != 0u ||
            stats.usart_overruns != 0u || stats.usart_framing_errors != 0u ||
            stats.usart_noise_errors != 0u || stats.usart_clear_busy != 0u) {
            seen_activity = true;
        }
        if (!seen_activity) {
            continue;
        }
        if (quiet_reported) {
            if (++since_quiet < QUIET_REPEAT_INTERVALS) {
                continue;
            }
        }
        since_quiet = 0u;

        const bool checkpoint = quiet == QUIET_INTERVALS;
        uart1_write_string(checkpoint ? "W3E1 RX_QUIET" : "W3E1 SAMPLE");
        write_field(" bytes=", stats.bytes_received);
        write_field(" ok=", stats.frames_ok);
        write_field(" crc=", stats.frames_crc_error);
        write_field(" lost=", stats.frames_lost);
        write_field(" len_err=", stats.frames_length_error);
        write_field(" old_seq=", stats.frames_old_seq);
        write_field(" last_seq=", stats.last_seq);
        write_field(" restarts=", stats.seq_restarts);
        write_field(" dropped=", stats.stream_dropped_bytes);
        write_field(" peak=", stats.stream_peak_bytes);
        write_field(" dma_late=", stats.dma_late_events);
        write_field(" dma_err=", stats.dma_errors);
        write_field(" ore=", stats.usart_overruns);
        write_field(" fe=", stats.usart_framing_errors);
        write_field(" ne=", stats.usart_noise_errors);
        write_field(" clear_busy=", stats.usart_clear_busy);
        write_field(" parser_pct_x100=", parser_pct);
        write_field(" report_pct_x100=", report_pct);
        write_field(" idle_pct_x100=", idle_pct);
        write_field(" busy_pct_x100=", 10000u - idle_pct);
        uart1_write_string("\r\n");
        quiet_reported = checkpoint;
    }
}

/* STEP 5: Chọn nhóm ưu tiên ngắt, tạo hai task tĩnh và khởi động scheduler. */
int main(void)
{
    NVIC_SetPriorityGrouping(0u);
    s_parser_handle = xTaskCreateStatic(parser_task, "Parser", PARSER_STACK_WORDS,
                                       NULL, configMAX_PRIORITIES - 1u,
                                       s_parser_stack, &s_parser_control);
    s_report_handle = xTaskCreateStatic(report_task, "Report", REPORT_STACK_WORDS,
                                        NULL, tskIDLE_PRIORITY + 1u,
                                        s_report_stack, &s_report_control);
    configASSERT(s_parser_handle != NULL && s_report_handle != NULL);
    vTaskStartScheduler();
    configASSERT(0);
    return 0;
}
