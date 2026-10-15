#ifndef HOST_TASK_H
#define HOST_TASK_H
#include "FreeRTOS.h"
typedef BaseType_t (*TaskHookFunction_t)(void *);
TaskHookFunction_t xTaskGetApplicationTaskTag(void *task);
TaskHookFunction_t xTaskGetApplicationTaskTagFromISR(void *task);
#endif
