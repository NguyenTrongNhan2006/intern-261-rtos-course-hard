#include "FreeRTOS.h"
#include "task.h"

#include "trace_app.h"
#include "tracer.h"
#include "uart.h"

static TaskHandle_t task_a_handle;
static TaskHandle_t task_b_handle;

static BaseType_t task_a_tag(void *argument)
{
    (void)argument;
    return 1;
}

static BaseType_t task_b_tag(void *argument)
{
    (void)argument;
    return 2;
}

static void busy_wait_us(uint32_t duration_us)
{
    const uint32_t cycles =
        (SystemCoreClock / 1000000u) * duration_us;

    const uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
    }
}

static void task_a(void *argument)
{
    (void)argument;

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        busy_wait_us(200u);

        (void)xTaskDelayUntil(
            &last_wake,
            pdMS_TO_TICKS(2u)
        );
    }
}

static void task_b(void *argument)
{
    (void)argument;

    TickType_t last_wake = xTaskGetTickCount();
    BaseType_t dumped_once = pdFALSE;

    for (;;) {
        busy_wait_us(300u);

        if (dumped_once == pdFALSE) {
            const uint32_t dumped = tracer_dump_uart();

            if (dumped != 0u) {
                uart1_write_string("# dumped=");
                uart1_write_u32(dumped);
                uart1_write_string("\r\n# CAPTURE DONE\r\n");

                uart1_flush();

                dumped_once = pdTRUE;
            }
        }

        (void)xTaskDelayUntil(
            &last_wake,
            pdMS_TO_TICKS(3u)
        );
    }
}

void trace_app_init(void)
{
    BaseType_t result = xTaskCreate(
        task_a,
        "TraceA",
        256u,
        NULL,
        tskIDLE_PRIORITY + 1u,
        &task_a_handle
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        task_b,
        "TraceB",
        256u,
        NULL,
        tskIDLE_PRIORITY + 1u,
        &task_b_handle
    );

    configASSERT(result == pdPASS);

    vTaskSetApplicationTaskTag(task_a_handle, task_a_tag);
    vTaskSetApplicationTaskTag(task_b_handle, task_b_tag);
}