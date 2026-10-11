#include "FreeRTOS.h"
#include "task.h"
#include "blinky.h"
#include "gpio.h"
#include "params.h"

static void tag_toggle(void)
{
    const uint32_t mask = 1u << W1E1_TAG_GPIO_PIN;

    taskENTER_CRITICAL();

    if ((W1E1_TAG_GPIO_PORT->ODR & mask) != 0u) {
        gpio_clear(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);
    } else {
        gpio_set(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);
    }

    taskEXIT_CRITICAL();
}

void vTaskBlinkA(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period =
        pdMS_TO_TICKS(W1E1_TASK_A_PERIOD_MS);

    for (;;) {
        (void)xTaskDelayUntil(&last_wake, period);
        tag_toggle();
    }
}

void vTaskBlinkB(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period =
        pdMS_TO_TICKS(W1E1_TASK_B_PERIOD_MS);

    for (;;) {
        (void)xTaskDelayUntil(&last_wake, period);
        tag_toggle();
    }
}

void blinky_app_init(void)
{
    BaseType_t result;

    result = xTaskCreate(
        vTaskBlinkA,
        "BlinkA",
        256u,
        NULL,
        tskIDLE_PRIORITY + 1u,
        NULL
    );
    configASSERT(result == pdPASS);

    result = xTaskCreate(
        vTaskBlinkB,
        "BlinkB",
        256u,
        NULL,
        tskIDLE_PRIORITY + 1u,
        NULL
    );
    configASSERT(result == pdPASS);
}