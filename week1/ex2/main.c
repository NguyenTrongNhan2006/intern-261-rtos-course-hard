#include "FreeRTOS.h"
#include "task.h"

#include "trace_app.h"
#include "tracer.h"
#include "uart.h"

_Static_assert(
    configPRIO_BITS == 4u,
    "STM32F103 requires 4 interrupt priority bits"
);

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();

    for (;;) {
    }
}

static void nvic_priority_init(void)
{
    SCB->AIRCR =
        (0x5FAu << SCB_AIRCR_VECTKEY_Pos) |
        (3u << SCB_AIRCR_PRIGROUP_Pos);

    __DSB();
    __ISB();
}

int main(void)
{
    nvic_priority_init();

    if (!uart1_init(115200u)) {
        for (;;) {
        }
    }

    tracer_init();
    trace_app_init();

    uart1_write_string("# W1E2 SCHEDULER TRACE\r\n");
    uart1_flush();

    vTaskStartScheduler();

    configASSERT(0);

    for (;;) {
        __NOP();
    }
}