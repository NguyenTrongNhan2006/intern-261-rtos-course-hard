#include "FreeRTOS.h"
#include "task.h"
#include "rta_app.h"
#include "rta_measure.h"
#include "tracer.h"
#include "uart.h"

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

int main(void)
{
    SCB->AIRCR = (0x5FAu << SCB_AIRCR_VECTKEY_Pos) |
                 (3u << SCB_AIRCR_PRIGROUP_Pos);
    __DSB();
    __ISB();
    configASSERT(SystemCoreClock == 72000000u);
    configASSERT(uart1_init(115200u));
    tracer_init();
    rta_measure_init();
    rta_app_init();
    uart1_write_string("# W1E3: reset after opening serial capture\r\n");
    uart1_flush();
    vTaskStartScheduler();
    configASSERT(0);
    for (;;) { }
}