#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "mixed_criticality_system.h"
#include "w4e2_internal.h"

int main(void)
{
    NVIC_SetPriorityGrouping(0u);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gpio_clock_enable(GPIOA);
    gpio_config_output_pp_2mhz(GPIOA, 4u);
    gpio_clear(GPIOA, 4u);

    w4e2_power_init();
    mixed_criticality_system_init();
    vTaskStartScheduler();
    configASSERT(false);
    return 1;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    configASSERT(false);
}
