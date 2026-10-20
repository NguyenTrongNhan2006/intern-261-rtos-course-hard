#ifndef MOCK_TASK_H
#define MOCK_TASK_H
#include "FreeRTOS.h"
enum { eIncrement = 2 };
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks);
BaseType_t xTaskNotifyFromISR(TaskHandle_t owner, uint32_t value, int action, BaseType_t *woken);
#endif
