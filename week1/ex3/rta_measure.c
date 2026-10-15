#include "FreeRTOS.h"
#include "task.h"
#include "params.h"
#include "rta_measure.h"
#include "tracer.h"
#include "uart.h"
#include <stdint.h>

#define JOB_CAPACITY 256u
typedef struct {
    uint32_t release;
    uint32_t finish;
    uint32_t cpu;
    uint16_t switches;
    uint8_t index;
    uint8_t reserved;
} job_record_t;
static job_record_t records[JOB_CAPACITY];
static TaskHandle_t handles[W1E3_NUM_TASKS];
static volatile uint32_t accumulated[W1E3_NUM_TASKS];
static volatile uint32_t dispatches[W1E3_NUM_TASKS];
static uint32_t dispatch_begin[W1E3_NUM_TASKS];
static volatile uint32_t segment_start;
static volatile int32_t selected = -1;
static volatile uint32_t epoch_cycles;
static volatile uint32_t epoch_ready;
static volatile uint32_t done;
static uint32_t record_count;
static uint32_t capture_start_tick;
static uint32_t hyperperiod;
static uint32_t dumped;

static uint32_t gcd(uint32_t a, uint32_t b)
{
    while (b != 0u) {
        const uint32_t remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

void rta_measure_init(void)
{
    hyperperiod = 1u;
    uint32_t expected_records = 0u;
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        const uint32_t t = W1E3_PERIOD_MS[i];
        configASSERT(t != 0u);
        configASSERT(pdMS_TO_TICKS(t) == t);
        configASSERT(W1E3_DEADLINE_MS[i] == t);
        configASSERT(W1E3_BUSY_WAIT_US[i] < t * 1000u);
        const uint64_t next = (uint64_t)(hyperperiod / gcd(hyperperiod, t)) * t;
        configASSERT(next <= 10000u);
        hyperperiod = (uint32_t)next;
    }
    /* Keep the entire capture below one CYCCNT wrap at 72 MHz. */
    configASSERT(hyperperiod * RTA_HYPERPERIODS < 50000u);
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        expected_records += hyperperiod * RTA_HYPERPERIODS / W1E3_PERIOD_MS[i];
    }
    configASSERT(expected_records <= JOB_CAPACITY);
    capture_start_tick = 20u;
}

void rta_register_task(uint32_t index, void *handle)
{
    configASSERT(index < W1E3_NUM_TASKS);
    handles[index] = (TaskHandle_t)handle;
}

void vApplicationTickHook(void)
{
    if ((epoch_ready == 0u) && (xTaskGetTickCountFromISR() == capture_start_tick)) {
        epoch_cycles = DWT->CYCCNT;
        __DMB();
        epoch_ready = 1u;
    }
}

void rta_switched_in(void)
{
    tracer_on_switched_in();
    const TaskHandle_t current = xTaskGetCurrentTaskHandle();
    selected = -1;
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        if (current == handles[i]) {
            selected = (int32_t)i;
            ++dispatches[i];
            break;
        }
    }
    segment_start = DWT->CYCCNT;
}

void rta_switched_out(void)
{
    const uint32_t now = DWT->CYCCNT;
    if (selected >= 0) {
        accumulated[(uint32_t)selected] += now - segment_start;
    }
    selected = -1;
    tracer_on_switched_out();
}

uint32_t rta_selected_cycles(uint32_t index)
{
    uint32_t result;
    /* BASEPRI blocks a scheduler switch across this consistent snapshot.
     * DWT time while another task runs is excluded. SysTick and portions
     * of exception save/restore remain charged: calibrate on hardware. */
    taskENTER_CRITICAL();
    configASSERT(selected == (int32_t)index);
    result = accumulated[index] + (DWT->CYCCNT - segment_start);
    taskEXIT_CRITICAL();
    return result;
}

void rta_job_begin(uint32_t index)
{
    dispatch_begin[index] = dispatches[index];
}

void rta_job_end(uint32_t index, uint32_t release, uint32_t cpu_cycles)
{
    taskENTER_CRITICAL();
    const uint32_t finish = DWT->CYCCNT;
    configASSERT(record_count < JOB_CAPACITY);
    job_record_t * const record = &records[record_count++];
    record->release = release;
    record->finish = finish;
    record->cpu = cpu_cycles;
    record->switches = (uint16_t)(dispatches[index] - dispatch_begin[index]);
    record->index = (uint8_t)index;
    record->reserved = 0u;
    taskEXIT_CRITICAL();
}

void rta_task_done(void)
{
    taskENTER_CRITICAL();
    ++done;
    taskEXIT_CRITICAL();
}

uint32_t rta_epoch_cycles(void)
{
    configASSERT(epoch_ready != 0u);
    return epoch_cycles;
}

uint32_t rta_hyperperiod_ms(void) { return hyperperiod; }
uint32_t rta_start_tick(void) { return capture_start_tick; }

void vApplicationIdleHook(void)
{
    if ((done != W1E3_NUM_TASKS) || (dumped != 0u)) {
        return;
    }
    dumped = 1u;
    /* All measured jobs have finished; UART cannot perturb their timing. */
    uart1_write_string("# W1E3 JOBS: tag,release_cycles,finish_cycles,selected_cycles,resumes\r\n");
    for (uint32_t n = 0u; n < record_count; ++n) {
        const job_record_t * const record = &records[n];
        uart1_write_string("# JOB,");
        uart1_write_u32(record->index + 1u);
        uart1_write_char(',');
        uart1_write_u32(record->release);
        uart1_write_char(',');
        uart1_write_u32(record->finish);
        uart1_write_char(',');
        uart1_write_u32(record->cpu);
        uart1_write_char(',');
        uart1_write_u32(record->switches);
        uart1_write_string("\r\n");
    }
    /* The W1E2 ring is a short scheduler capture; the JOB extension above
     * continues across all five hyperperiods even after this ring freezes. */
    (void)tracer_dump_uart();
    uart1_write_string("# W1E3 DONE\r\n");
    uart1_flush();
}
