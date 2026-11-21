#include "gpio.h"
#include "w4e3_internal.h"

int main(void)
{
    NVIC_SetPriorityGrouping(0u);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gpio_clock_enable(GPIOA);
    gpio_config_output_pp_2mhz(GPIOA, 4u);
    gpio_clear(GPIOA, 4u);

    w4e3_power_init();
    mixed_criticality_system_init();
    fault_injection_init(NULL);
    vTaskStartScheduler();
    configASSERT(false);
    return 1;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    w4e3_fault_stack_overflow(w4e3_current_role());
}
