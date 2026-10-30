#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "params.h"
#include "sync_app.h"
#include "tracer.h"
#include "uart.h"

/* STEP 1: Chọn bản lỗi (0) hoặc bản sửa (1). */
enum { TASK_SENDER, TASK_RECEIVER, TASK_LOGGER };

#ifndef USE_FIXED
#define USE_FIXED 0
#endif

#ifndef SEND_COUNT
#define SEND_COUNT       1000u
#endif

#ifndef BURST_PERIOD_MS
#define BURST_PERIOD_MS  10u
#endif

#ifndef BARRIER_LATE_MS
#define BARRIER_LATE_MS  0u
#endif

#define BENCH_ITERATIONS 1000u
#define STACK_WORDS      256u
#define START_DELAY_MS   10u
#define TRACE_CAPACITY   32u

#define SEND_DONE        (1u << TASK_SENDER)
#define RECEIVE_DONE     (1u << TASK_RECEIVER)
#define TRANSFER_DONE    (SEND_DONE | RECEIVE_DONE)

_Static_assert(W2E4_NUM_TASKS == 3, "This test uses three task roles");
_Static_assert(W2E4_SYNC_BITS_MASK == 0x07u, "Barrier needs bits 0..2");
_Static_assert(SEND_COUNT > 0u && SEND_COUNT % 2u == 0u,
               "Send count must be positive and even");
_Static_assert(BENCH_ITERATIONS > 0u, "Benchmark needs samples");
_Static_assert(USE_FIXED == 0 || USE_FIXED == 1, "Invalid variant");
_Static_assert(BARRIER_LATE_MS == 0u
               || BARRIER_LATE_MS > W2E4_SYNC_TIMEOUT_MS,
               "Late Logger must miss both other timeouts");

/* STEP 2: Mọi đối tượng đều dùng bộ nhớ static. */
static TaskHandle_t s_tasks[W2E4_NUM_TASKS];
static StaticTask_t s_tcbs[W2E4_NUM_TASKS], s_idle_tcb;

_Alignas(8) static StackType_t
    s_stacks[W2E4_NUM_TASKS][STACK_WORDS];

_Alignas(8) static StackType_t
    s_idle_stack[configMINIMAL_STACK_SIZE];

static EventGroupHandle_t s_group;
static StaticEventGroup_t s_group_storage;
static SemaphoreHandle_t s_baseline;
static StaticSemaphore_t s_baseline_storage;

static volatile uint32_t s_sent, s_received;
static volatile uint32_t s_barrier_in[W2E4_NUM_TASKS];
static volatile uint32_t s_barrier_out[W2E4_NUM_TASKS];
static volatile EventBits_t s_barrier_bits[W2E4_NUM_TASKS];

/* SEND: value là sequence; RECV: value là payload hoặc số đếm. */
typedef struct {
    uint32_t cycles;
    uint32_t value;
    char kind;
} notify_record_t;

static notify_record_t s_trace[TRACE_CAPACITY];
static volatile uint32_t s_head, s_tail, s_trace_dropped;

/* STEP 3: Bộ nhớ Idle Task và hook kiểm tra stack. */
void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &s_idle_tcb;
    *stack = s_idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;

    __disable_irq();
    for (;;) {
        __NOP();
    }
}

/* STEP 4: DWT chung cho Gantt, log sự kiện và benchmark. */
static inline uint32_t cycle_now(void)
{
    __COMPILER_BARRIER();
    const uint32_t cycles = DWT->CYCCNT;
    __COMPILER_BARRIER();
    return cycles;
}

static void print_value(const char *label, uint32_t value)
{
    uart1_write_string(label);
    uart1_write_u32(value);
    uart1_write_string("\r\n");
}

/* Hai task cùng ghi: bảo vệ ngắn, không gửi UART trong critical. */
static void trace_notify(char kind, uint32_t value)
{
    taskENTER_CRITICAL();
    const uint32_t next = (s_head + 1u) % TRACE_CAPACITY;

    if (next == s_tail) {
        ++s_trace_dropped;
    } else {
        s_trace[s_head] = (notify_record_t){
            .cycles = cycle_now(),
            .value = value,
            .kind = kind
        };
        s_head = next;
    }
    taskEXIT_CRITICAL();
}

static void flush_notify_trace(void)
{
    for (;;) {
        taskENTER_CRITICAL();

        if (s_tail == s_head) {
            taskEXIT_CRITICAL();
            return;
        }

        const notify_record_t record = s_trace[s_tail];
        s_tail = (s_tail + 1u) % TRACE_CAPACITY;
        taskEXIT_CRITICAL();

        uart1_write_string("# EV,");
        uart1_write_u32(record.cycles);
        uart1_write_char(',');
        uart1_write_char(record.kind);
        uart1_write_char(',');
        uart1_write_u32(record.value);
        uart1_write_string("\r\n");
    }
}

/* STEP 5: Bản lỗi ghi đè giá trị chưa được nhận. */
void sync_app_notify_send_buggy(uint32_t value)
{
    ++s_sent;
    trace_notify('S', value);

    const BaseType_t ok = xTaskNotify(
        s_tasks[TASK_RECEIVER], value, eSetValueWithOverwrite);
    configASSERT(ok == pdPASS);
}

void sync_app_notify_receive_buggy(uint32_t *out_value)
{
    configASSERT(out_value != NULL);

    const BaseType_t ok =
        xTaskNotifyWait(0u, UINT32_MAX, out_value, portMAX_DELAY);
    configASSERT(ok == pdTRUE);

    ++s_received; /* Một lần nhận chỉ chứng minh một sự kiện. */
    trace_notify('R', *out_value);
}

/* STEP 6: Bản sửa cộng số sự kiện, không giữ payload riêng. */
void sync_app_notify_send_fixed(uint32_t value)
{
    ++s_sent;
    trace_notify('S', value); /* Sequence chỉ nằm trong trace. */

    const BaseType_t ok =
        xTaskNotify(s_tasks[TASK_RECEIVER], 0u, eIncrement);
    configASSERT(ok == pdPASS);
}

void sync_app_notify_receive_fixed(uint32_t *out_value)
{
    configASSERT(out_value != NULL);

    const BaseType_t ok =
        xTaskNotifyWait(0u, UINT32_MAX, out_value, portMAX_DELAY);
    configASSERT(ok == pdTRUE);

    s_received += *out_value;
    trace_notify('R', *out_value);
}

void sync_app_get_counts(uint32_t *sent_count, uint32_t *received_count)
{
    configASSERT(sent_count != NULL && received_count != NULL);

    taskENTER_CRITICAL();
    *sent_count = s_sent;
    *received_count = s_received;
    taskEXIT_CRITICAL();
}

/* STEP 7: So sánh một cặp báo hiệu/nhận không blocking. */
static void benchmark(void)
{
    uint32_t overhead = UINT32_MAX;

    for (uint32_t i = 0u; i < 32u; ++i) {
        const uint32_t start = cycle_now();
        const uint32_t elapsed = cycle_now() - start;

        if (elapsed < overhead) {
            overhead = elapsed;
        }
    }

    uint64_t binary_sum = 0u, notify_sum = 0u;
    const TaskHandle_t self = s_tasks[TASK_LOGGER];

    for (uint32_t i = 0u; i < BENCH_ITERATIONS; ++i) {
        uint32_t start = cycle_now();
        const BaseType_t given = xSemaphoreGive(s_baseline);
        const BaseType_t taken = xSemaphoreTake(s_baseline, 0u);
        uint32_t elapsed = cycle_now() - start;

        configASSERT(given == pdTRUE && taken == pdTRUE);
        binary_sum += elapsed > overhead ? elapsed - overhead : 0u;

        uint32_t value = 0u;
        start = cycle_now();

        const BaseType_t sent = xTaskNotify(self, 0u, eIncrement);
        const BaseType_t received =
            xTaskNotifyWait(0u, UINT32_MAX, &value, 0u);

        elapsed = cycle_now() - start;

        configASSERT(sent == pdPASS && received == pdTRUE && value == 1u);
        notify_sum += elapsed > overhead ? elapsed - overhead : 0u;
    }

    print_value("# DWT_OVERHEAD=", overhead);
    print_value("# BENCH_ITERATIONS=", BENCH_ITERATIONS);

    print_value("# BINARY_PAIR_CYCLES=",
                (uint32_t)(binary_sum / BENCH_ITERATIONS));

    print_value("# FIXED_NOTIFY_PAIR_CYCLES=",
                (uint32_t)(notify_sum / BENCH_ITERATIONS));

    const uint32_t tcb_bytes = sizeof(s_tcbs) + sizeof(s_idle_tcb);
    const uint32_t common_bytes = sizeof(s_group_storage) + tcb_bytes;

    print_value("# RAM_EVENT_GROUP=", sizeof(s_group_storage));
    print_value("# RAM_BINARY_SEM=", sizeof(s_baseline_storage));
    print_value("# RAM_TCB_EACH=", sizeof(StaticTask_t));
    print_value("# RAM_ALL_TCBS=", tcb_bytes);
    print_value("# RAM_ALL_STACKS=", sizeof(s_stacks) + sizeof(s_idle_stack));

    print_value("# RAM_SYNC_BINARY=",
                common_bytes + sizeof(s_baseline_storage));

    print_value("# RAM_SYNC_NOTIFY=", common_bytes);

    uart1_write_string(
        "# RAM_SYNC: object cost, not whole firmware RAM\r\n");
}

/* STEP 8: Kiểm tra kết quả barrier của cả ba task. */
static bool barrier_ok(void)
{
    for (uint32_t i = 0u; i < W2E4_NUM_TASKS; ++i) {
        if ((s_barrier_bits[i] & W2E4_SYNC_BITS_MASK)
            != W2E4_SYNC_BITS_MASK) {
            return false;
        }
    }
    return true;
}

static void logger_run(void)
{
    uint32_t last_in = s_barrier_in[0];
    uint32_t last_out = s_barrier_out[0];

    for (uint32_t i = 1u; i < W2E4_NUM_TASKS; ++i) {
        if (s_barrier_in[i] > last_in) {
            last_in = s_barrier_in[i];
        }
        if (s_barrier_out[i] > last_out) {
            last_out = s_barrier_out[i];
        }
    }

    uart1_write_string("# BARRIER=PASS\r\n");
    print_value("# BARRIER_RELEASE_CYCLES=", last_out - last_in);

    /* Tái dùng hai bit sau khi barrier đã hoàn tất và xóa mask. */
    for (;;) {
        flush_notify_trace();

        const EventBits_t done = xEventGroupWaitBits(
            s_group, TRANSFER_DONE, pdFALSE, pdTRUE,
            pdMS_TO_TICKS(BURST_PERIOD_MS));

        if ((done & TRANSFER_DONE) == TRANSFER_DONE) {
            break;
        }
    }

    flush_notify_trace();
    tracer_stop();

    uint32_t sent, received;
    sync_app_get_counts(&sent, &received);

    print_value("# SENT=", sent);
    print_value("# RECEIVED_EVENTS=", received);
    print_value("# TRACE_DROPPED=", s_trace_dropped);

    if (s_trace_dropped != 0u) {
        uart1_write_string("# STATUS=TRACE_INVALID\r\n");
    } else if (sent == SEND_COUNT && received == sent) {
        uart1_write_string("# STATUS=BALANCED\r\n");
    } else if (sent == SEND_COUNT && received < sent) {
        print_value("# LOST_EVENTS=", sent - received);
        uart1_write_string("# STATUS=LOSS_CONFIRMED\r\n");
    } else {
        uart1_write_string("# STATUS=COUNT_ERROR\r\n");
    }

    benchmark(); /* Slot của logger, không cộng vào phép thử sự kiện. */

    uart1_write_string("# GANTT: cycles,tag,event\r\n");
    const uint32_t records = tracer_dump_uart();
    print_value("# GANTT_RECORDS=", records);

    uart1_write_string("# END\r\n");
    uart1_flush();
}

/* STEP 9: Sender gửi hai lần trước khi receiver được chạy. */
static void sender_run(void)
{
    TickType_t release = xTaskGetTickCount();

    for (uint32_t value = 1u; value <= SEND_COUNT; value += 2u) {
        if (USE_FIXED) {
            sync_app_notify_send_fixed(value);
            sync_app_notify_send_fixed(value + 1u);
        } else {
            sync_app_notify_send_buggy(value);
            sync_app_notify_send_buggy(value + 1u);
        }

        if (value + 1u < SEND_COUNT) {
            xTaskDelayUntil(&release, pdMS_TO_TICKS(BURST_PERIOD_MS));
        }
    }

    xEventGroupSetBits(s_group, SEND_DONE);
}

static void receiver_run(void)
{
    for (;;) {
        uint32_t value;

        if (USE_FIXED) {
            sync_app_notify_receive_fixed(&value);

            if (s_received >= SEND_COUNT) {
                break;
            }
        } else {
            sync_app_notify_receive_buggy(&value);

            if (value == SEND_COUNT) {
                break;
            }
        }
    }

    xEventGroupSetBits(s_group, RECEIVE_DONE);
}

/* STEP 10: Mỗi participant đặt một bit và chờ đủ mask của đề. */
void vTaskBarrierParticipant(void *pvParameters)
{
    const uint32_t idx = (uint32_t)(uintptr_t)pvParameters;
    configASSERT(idx < W2E4_NUM_TASKS);

    TickType_t release = (TickType_t)configINITIAL_TICK_COUNT;
    const uint32_t late = (idx == TASK_LOGGER) ? BARRIER_LATE_MS : 0u;
    xTaskDelayUntil(&release, pdMS_TO_TICKS(idx + 1u + late));

    s_barrier_in[idx] = cycle_now();

    const EventBits_t bits = xEventGroupSync(
        s_group,
        (EventBits_t)(1UL << idx),
        W2E4_SYNC_BITS_MASK,
        pdMS_TO_TICKS(W2E4_SYNC_TIMEOUT_MS));

    s_barrier_out[idx] = cycle_now();
    s_barrier_bits[idx] = bits;

    release = (TickType_t)configINITIAL_TICK_COUNT;
    xTaskDelayUntil(&release, pdMS_TO_TICKS(START_DELAY_MS));

    if (!barrier_ok()) {
        if (idx == TASK_LOGGER) {
            tracer_stop();
            uart1_write_string("# BARRIER=FAIL\r\n");
            for (uint32_t i = 0u; i < W2E4_NUM_TASKS; ++i) {
                print_value("# BARRIER_BITS=", s_barrier_bits[i]);
            }
            (void)tracer_dump_uart();
            uart1_write_string("# END\r\n");
            uart1_flush();
        }
    } else if (idx == TASK_SENDER) {
        sender_run();
    } else if (idx == TASK_RECEIVER) {
        receiver_run();
    } else {
        logger_run();
    }

    for (;;) {
        vTaskSuspend(NULL);
    }
}

/* STEP 11: Tạo ba participant và gắn tag Gantt 1, 2, 3. */
static BaseType_t tag_sender(void *arg)
{
    (void)arg;
    return 1;
}

static BaseType_t tag_receiver(void *arg)
{
    (void)arg;
    return 2;
}

static BaseType_t tag_logger(void *arg)
{
    (void)arg;
    return 3;
}

void sync_app_init_barrier(void)
{
    configASSERT(pdMS_TO_TICKS(BURST_PERIOD_MS) > 0u);

    s_group = xEventGroupCreateStatic(&s_group_storage);
    s_baseline = xSemaphoreCreateBinaryStatic(&s_baseline_storage);
    configASSERT(s_group != NULL && s_baseline != NULL);

    static const char *const names[W2E4_NUM_TASKS] = {
        "Sender", "Receiver", "Logger"
    };

    static const TaskHookFunction_t tags[W2E4_NUM_TASKS] = {
        tag_sender, tag_receiver, tag_logger
    };

    for (uint32_t i = 0u; i < W2E4_NUM_TASKS; ++i) {
        s_tasks[i] = xTaskCreateStatic(
            vTaskBarrierParticipant,
            names[i],
            STACK_WORDS,
            (void *)(uintptr_t)i,
            (UBaseType_t)(3u - i),
            s_stacks[i],
            &s_tcbs[i]);

        configASSERT(s_tasks[i] != NULL);
        vTaskSetApplicationTaskTag(s_tasks[i], tags[i]);
    }
}

/* STEP 12: Khởi tạo UART, DWT và scheduler. */
int main(void)
{
    NVIC_SetPriorityGrouping(0u);

    const bool uart_ok = uart1_init(115200u);
    configASSERT(uart_ok);

    tracer_init();
    sync_app_init_barrier();

    print_value("# USE_FIXED=", USE_FIXED);
    print_value("# CPU_HZ=", SystemCoreClock);
    print_value("# SEND_COUNT=", SEND_COUNT);
    print_value("# BURST_PERIOD_MS=", BURST_PERIOD_MS);
    print_value("# BARRIER_LATE_MS=", BARRIER_LATE_MS);

    uart1_write_string(
        "# EV: cycles,S=sent_sequence,R=payload_or_count\r\n"
        "# TAGS: 0=Idle,1=Sender,2=Receiver,3=Logger\r\n");
    uart1_flush();

    DWT->CYCCNT = 0u;
    vTaskStartScheduler();

    configASSERT(0);
    for (;;) {
        __NOP();
    }
}