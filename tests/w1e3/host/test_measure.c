#include "FreeRTOS.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
host_dwt_t host_dwt;
host_debug_t host_debug;
uint32_t host_primask, SystemCoreClock = 72000000u;
static uint32_t tick, inject_tick, executed_iterations;
static TaskHandle_t current;
static char uart_output[65536];
static uint32_t uart_size;

/* Compile the actual production C source into this translation unit. */
#include "../../../week1/ex3/rta_measure.c"

TickType_t xTaskGetTickCountFromISR(void) { return tick; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current; }
uint32_t xPortGetFreeHeapSize(void) { return 2048u; }
uint32_t xPortGetMinimumEverFreeHeapSize(void) { return 2000u; }
void tracer_on_switched_in(void) { }
void tracer_on_switched_out(void) { }
uint32_t tracer_dump_uart(void) { return 0u; }
void uart1_write_char(char c)
{
    assert(uart_size + 1u < sizeof(uart_output));
    uart_output[uart_size++] = c;
    uart_output[uart_size] = '\0';
}
void uart1_write_string(const char *s) { while (*s) uart1_write_char(*s++); }
void uart1_write_u32(uint32_t n)
{
    char text[16];
    (void)snprintf(text, sizeof(text), "%u", n);
    uart1_write_string(text);
}
void uart1_flush(void) { }
void rta_work_iterations(uint32_t n)
{
    assert(n != 0u);
    executed_iterations = n;
    host_dwt.CYCCNT += 2u * n;
    if (inject_tick != 0u) {
        inject_tick = 0u;
        assert(host_primask == 0u);
        rta_irq_enter(0u);
        host_dwt.CYCCNT += 200u;
        tick = 21u;
        vApplicationTickHook();
        rta_irq_exit(0u);
    }
    host_dwt.CYCCNT += 2u * n;
}
int main(void)
{
    rta_measure_init();
    assert(total_records == 205u && hyperperiod == 1200u);
    assert(job_counts[0] == 100u && job_counts[3] == 15u);
    for (uint32_t i = 0u; i < 4u; ++i) {
        assert(calibration[i].minimum >= calibration[i].target);
        assert(calibration[i].maximum - calibration[i].target <= 3u);
    }
    assert(irq_stats[0].total == 0u && active_irq == -1);
    host_dwt.CYCCNT = 1000u;
    tick = 20u;
    rta_irq_enter(0u);
    host_dwt.CYCCNT = 1030u;
    vApplicationTickHook();
    assert(records[0].release == 1030u && records[0].tick_cycles == 30u);
    host_dwt.CYCCNT = 1100u;
    rta_irq_exit(0u);
    rta_irq_enter(1u);
    host_dwt.CYCCNT += 80u;
    rta_irq_exit(1u);
    rta_job_begin(0u, 0u);
    inject_tick = 1u;
    const uint32_t wall = rta_run_work(0u);
    assert(executed_iterations == calibration[0].iterations);
    assert(wall == calibration[0].target + 200u);
    host_dwt.CYCCNT += 40u;
    rta_job_end(0u, 0u, wall);
    assert(records[0].tick_cycles == 270u);
    assert(records[0].pend_cycles == 80u);
    assert(records[0].tick_count == 2u && records[0].pend_count == 1u);
    assert(records[0].state == 2u && host_primask == 0u);
    /* No overwrite at a tick that is not a release. */
    assert(records[1].state == 0u);
    tick = 6020u;
    vApplicationTickHook();
    assert(records[1].state == 0u);
    for (uint32_t n = 0u; n < total_records; ++n) records[n].state = 2u;
    done = 4u;
    vApplicationIdleHook();
    assert(strstr(uart_output, "# VERSION,2\r\n") != NULL);
    assert(strstr(uart_output, "# W1E3 DONE\r\n") != NULL);
    uint32_t count = 0u;
    const char *p = uart_output;
    while ((p = strstr(p, "# JOB2,")) != NULL) { ++count; ++p; }
    assert(count == 205u);
    const uint32_t previous = uart_size;
    vApplicationIdleHook();
    assert(uart_size == previous);
    puts("PASS: real C calibration/search, fixed iteration count with injected IRQ, release clipping, job counters, 205-record framing");
    return 0;
}
