#include "FreeRTOS.h"
#include "task.h"
#include "params.h"
#include "rta_measure.h"
#include "tracer.h"
#include "uart.h"
#include <stdint.h>

#define JOB_CAPACITY 210u
#define CAL_REPEATS 8u
typedef struct {
    uint32_t release, finish, wall;
    uint32_t tick_cycles, pend_cycles, tick_count, pend_count;
    uint16_t resumes, state;
} job_record_t;
typedef struct { uint32_t target, iterations, minimum, maximum; } calibration_t;
typedef struct { uint32_t total, count, minimum, maximum, start; } irq_stats_t;
static job_record_t records[JOB_CAPACITY];
static calibration_t calibration[W1E3_NUM_TASKS];
static volatile irq_stats_t irq_stats[2];
static volatile int32_t active_irq = -1;
static TaskHandle_t handles[W1E3_NUM_TASKS];
static volatile uint32_t dispatches[W1E3_NUM_TASKS];
static uint32_t dispatch_begin[W1E3_NUM_TASKS];
static uint32_t offsets[W1E3_NUM_TASKS], job_counts[W1E3_NUM_TASKS];
static uint32_t hyperperiod, total_records, probe_min, probe_max;
static volatile uint32_t done;
static uint32_t dumped;
extern void rta_work_iterations(uint32_t iterations);

static uint32_t gcd(uint32_t a, uint32_t b)
{
    while (b != 0u) { const uint32_t c = a % b; a = b; b = c; }
    return a;
}

/* Same call and timestamp envelope for calibration and scheduled work.
 * No loop condition reads wall time: an interrupt cannot truncate work. */
__attribute__((noinline, noclone))
static uint32_t measure_iterations(uint32_t iterations)
{
    __DSB();
    __ISB();
    const uint32_t start = DWT->CYCCNT;
    rta_work_iterations(iterations);
    return DWT->CYCCNT - start;
}

static void calibrate(uint32_t i)
{
    calibration_t * const c = &calibration[i];
    c->target = W1E3_BUSY_WAIT_US[i] * (SystemCoreClock / 1000000u);
    uint32_t low = 1u, high = c->target;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (measure_iterations(middle) < c->target) { low = middle + 1u; }
        else { high = middle; }
    }
    c->iterations = low;
    /* Recheck the minimum; changing fetch/prefetch phase must not silently
     * produce a calibration sample below the requested budget. */
    for (uint32_t attempt = 0u; attempt < 32u; ++attempt) {
        c->minimum = UINT32_MAX;
        c->maximum = 0u;
        for (uint32_t sample = 0u; sample < CAL_REPEATS; ++sample) {
            const uint32_t elapsed = measure_iterations(c->iterations);
            if (elapsed < c->minimum) { c->minimum = elapsed; }
            if (elapsed > c->maximum) { c->maximum = elapsed; }
        }
        if (c->minimum >= c->target) { break; }
        ++c->iterations;
    }
    configASSERT(c->minimum >= c->target);
    configASSERT(c->maximum - c->target <= SystemCoreClock / 1000000u);
}

void rta_irq_enter(uint32_t kind)
{
    const uint32_t now = DWT->CYCCNT;
    configASSERT(kind < 2u);
    /* SysTick and PendSV share lowest priority in this experiment. */
    configASSERT(active_irq == -1);
    irq_stats[kind].start = now;
    active_irq = (int32_t)kind;
}
void rta_irq_exit(uint32_t kind)
{
    const uint32_t now = DWT->CYCCNT;
    configASSERT(kind < 2u);
    configASSERT(active_irq == (int32_t)kind);
    const uint32_t elapsed = now - irq_stats[kind].start;
    irq_stats[kind].total += elapsed;
    ++irq_stats[kind].count;
    if (elapsed < irq_stats[kind].minimum) { irq_stats[kind].minimum = elapsed; }
    if (elapsed > irq_stats[kind].maximum) { irq_stats[kind].maximum = elapsed; }
    active_irq = -1;
}

void rta_measure_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    configASSERT((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) == 0u);
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
    const uint32_t saved = __get_PRIMASK();
    __disable_irq();
    hyperperiod = 1u;
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        const uint32_t t = W1E3_PERIOD_MS[i];
        configASSERT(t != 0u && pdMS_TO_TICKS(t) == t);
        configASSERT(W1E3_DEADLINE_MS[i] == t);
        configASSERT(W1E3_BUSY_WAIT_US[i] > 0u && W1E3_BUSY_WAIT_US[i] < t * 1000u);
        const uint64_t h = (uint64_t)(hyperperiod / gcd(hyperperiod, t)) * t;
        configASSERT(h <= 10000u);
        hyperperiod = (uint32_t)h;
        calibrate(i);
    }
    configASSERT(hyperperiod * RTA_HYPERPERIODS < 50000u);
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        offsets[i] = total_records;
        job_counts[i] = hyperperiod * RTA_HYPERPERIODS / W1E3_PERIOD_MS[i];
        total_records += job_counts[i];
    }
    configASSERT(total_records <= JOB_CAPACITY);
    probe_min = UINT32_MAX;
    for (uint32_t n = 0u; n < 32u; ++n) {
        rta_irq_enter(0u);
        rta_irq_exit(0u);
        const uint32_t delta = irq_stats[0].total;
        if (delta < probe_min) { probe_min = delta; }
        if (delta > probe_max) { probe_max = delta; }
        irq_stats[0].total = 0u;
    }
    for (uint32_t k = 0u; k < 2u; ++k) {
        irq_stats[k].total = irq_stats[k].count = 0u;
        irq_stats[k].minimum = UINT32_MAX;
        irq_stats[k].maximum = 0u;
    }
    __set_PRIMASK(saved);
}

void rta_register_task(uint32_t i, void *handle)
{
    configASSERT(i < W1E3_NUM_TASKS);
    handles[i] = (TaskHandle_t)handle;
}
void rta_switched_in(void)
{
    tracer_on_switched_in();
    const TaskHandle_t current = xTaskGetCurrentTaskHandle();
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        if (handles[i] == current) { ++dispatches[i]; break; }
    }
}
void rta_switched_out(void) { tracer_on_switched_out(); }

void vApplicationTickHook(void)
{
    const uint32_t tick = xTaskGetTickCountFromISR();
    if (tick < rta_start_tick()) { return; }
    const uint32_t relative = tick - rta_start_tick();
    if (relative >= hyperperiod * RTA_HYPERPERIODS) { return; }
    configASSERT(active_irq == 0);
    const uint32_t now = DWT->CYCCNT;
    const uint32_t ticks_now = irq_stats[0].total + (now - irq_stats[0].start);
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        const uint32_t period = W1E3_PERIOD_MS[i];
        if (relative % period == 0u) {
            const uint32_t sequence = relative / period;
            configASSERT(sequence < job_counts[i]);
            job_record_t * const r = &records[offsets[i] + sequence];
            configASSERT(r->state == 0u);
            r->release = now;
            /* The current SysTick is in progress: retain only its part
             * after this release in the eventual response delta. */
            r->tick_cycles = ticks_now;
            r->pend_cycles = irq_stats[1].total;
            r->tick_count = irq_stats[0].count;
            r->pend_count = irq_stats[1].count;
            r->state = 1u;
        }
    }
}
void rta_job_begin(uint32_t i, uint32_t sequence)
{
    configASSERT(sequence < job_counts[i]);
    configASSERT(records[offsets[i] + sequence].state == 1u);
    dispatch_begin[i] = dispatches[i];
}
uint32_t rta_run_work(uint32_t i)
{
    return measure_iterations(calibration[i].iterations);
}
void rta_job_end(uint32_t i, uint32_t sequence, uint32_t wall)
{
    const uint32_t saved = __get_PRIMASK();
    __disable_irq();
    const uint32_t finish = DWT->CYCCNT;
    job_record_t * const r = &records[offsets[i] + sequence];
    configASSERT(r->state == 1u);
    r->finish = finish;
    r->wall = wall;
    r->tick_cycles = irq_stats[0].total - r->tick_cycles;
    r->pend_cycles = irq_stats[1].total - r->pend_cycles;
    r->tick_count = irq_stats[0].count - r->tick_count;
    r->pend_count = irq_stats[1].count - r->pend_count;
    r->resumes = (uint16_t)(dispatches[i] - dispatch_begin[i]);
    r->state = 2u;
    __set_PRIMASK(saved);
}
void rta_task_done(void)
{
    taskENTER_CRITICAL();
    ++done;
    taskEXIT_CRITICAL();
}
uint32_t rta_hyperperiod_ms(void) { return hyperperiod; }
uint32_t rta_start_tick(void) { return 20u; }

static void comma(uint32_t n)
{
    uart1_write_char(','); uart1_write_u32(n);
}
void vApplicationIdleHook(void)
{
    if (done != W1E3_NUM_TASKS || dumped != 0u) { return; }
    dumped = 1u;
    /* Freeze IRQ statistics after the experiment, before UART output. */
    irq_stats_t snapshot[2];
    const uint32_t saved = __get_PRIMASK();
    __disable_irq();
    for (uint32_t k = 0u; k < 2u; ++k) {
        snapshot[k].total = irq_stats[k].total;
        snapshot[k].count = irq_stats[k].count;
        snapshot[k].minimum = irq_stats[k].minimum;
        snapshot[k].maximum = irq_stats[k].maximum;
    }
    __set_PRIMASK(saved);
    uart1_write_string("# VERSION,2\r\n");
    uart1_write_string("# IRQPROBE"); comma(probe_min); comma(probe_max);
    uart1_write_string("\r\n# HEAP"); comma(xPortGetFreeHeapSize()); comma(xPortGetMinimumEverFreeHeapSize());
    uart1_write_string("\r\n");
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        uart1_write_string("# CAL"); comma(i + 1u);
        comma(calibration[i].target); comma(calibration[i].iterations);
        comma(calibration[i].minimum); comma(calibration[i].maximum);
        uart1_write_string("\r\n");
        for (uint32_t k = 0u; k < job_counts[i]; ++k) {
            const job_record_t * const r = &records[offsets[i] + k];
            configASSERT(r->state == 2u);
            uart1_write_string("# JOB2"); comma(i + 1u); comma(k);
            comma(r->release); comma(r->finish); comma(r->wall);
            comma(r->tick_cycles); comma(r->pend_cycles);
            comma(r->tick_count); comma(r->pend_count); comma(r->resumes);
            uart1_write_string("\r\n");
        }
    }
    for (uint32_t k = 0u; k < 2u; ++k) {
        uart1_write_string("# IRQ"); comma(k); comma(snapshot[k].count);
        comma(snapshot[k].minimum); comma(snapshot[k].maximum); comma(snapshot[k].total);
        uart1_write_string("\r\n");
    }
    (void)tracer_dump_uart();
    uart1_write_string("# W1E3 DONE\r\n");
    uart1_flush();
}
