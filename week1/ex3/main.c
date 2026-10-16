#include "FreeRTOS.h"
#include "task.h"
#include "rta_app.h"
#include "rta_measure.h"
#include "tracer.h"
#include "uart.h"
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);
extern void SVC_Handler(void);
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
    const volatile uintptr_t * const vectors = (const volatile uintptr_t *)SCB->VTOR;
    configASSERT(vectors[11] == (uintptr_t)SVC_Handler);
    configASSERT(vectors[14] == (uintptr_t)PendSV_Handler);
    configASSERT(vectors[15] == (uintptr_t)SysTick_Handler);
    /* Calibrate before the scheduler; tracer_init then resets CYCCNT. */
    rta_measure_init();
    tracer_init();
    rta_app_init();
    uart1_write_string("# W1E3 V2 calibrated iterations + IRQ profiling\r\n");
    uart1_flush();
    vTaskStartScheduler();
    configASSERT(0);
    for (;;) { }
}
