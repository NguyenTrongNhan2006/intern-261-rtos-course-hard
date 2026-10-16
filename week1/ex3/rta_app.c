#include "FreeRTOS.h"
#include "task.h"
#include "params.h"
#include "rta_app.h"
#include "rta_measure.h"
static TaskHandle_t handles[W1E3_NUM_TASKS];
static uint32_t indices[W1E3_NUM_TASKS];
static const char * const names[W1E3_NUM_TASKS] = {"RTA0", "RTA1", "RTA2", "RTA3"};
static BaseType_t tag0(void *p) { (void)p; return 1; }
static BaseType_t tag1(void *p) { (void)p; return 2; }
static BaseType_t tag2(void *p) { (void)p; return 3; }
static BaseType_t tag3(void *p) { (void)p; return 4; }
static TaskHookFunction_t const tags[W1E3_NUM_TASKS] = {tag0, tag1, tag2, tag3};
void vTaskPeriodic(void *p)
{
    const uint32_t i = *(const uint32_t *)p;
    const TickType_t period = pdMS_TO_TICKS(W1E3_PERIOD_MS[i]);
    TickType_t wake = (TickType_t)rta_start_tick() - period;
    const uint32_t jobs = rta_hyperperiod_ms() * RTA_HYPERPERIODS / W1E3_PERIOD_MS[i];
    for (uint32_t k = 0u; k < jobs; ++k) {
        (void)xTaskDelayUntil(&wake, period);
        rta_job_begin(i, k);
        const uint32_t wall = rta_run_work(i);
        rta_job_end(i, k, wall);
    }
    rta_task_done();
    vTaskSuspend(NULL);
    configASSERT(0);
}
void rta_app_init(void)
{
    _Static_assert(W1E3_NUM_TASKS == 4, "four tags required");
    for (uint32_t i = 0u; i < W1E3_NUM_TASKS; ++i) {
        indices[i] = i;
        configASSERT(xTaskCreate(vTaskPeriodic, names[i], 256u,
            &indices[i], 1u, &handles[i]) == pdPASS);
        rta_register_task(i, handles[i]);
        vTaskSetApplicationTaskTag(handles[i], tags[i]);
        UBaseType_t priority = 1u;
        for (uint32_t j = 0u; j < W1E3_NUM_TASKS; ++j) {
            if ((W1E3_PERIOD_MS[j] > W1E3_PERIOD_MS[i]) ||
                ((W1E3_PERIOD_MS[j] == W1E3_PERIOD_MS[i]) && (j > i))) {
                ++priority;
            }
        }
        vTaskPrioritySet(handles[i], priority);
    }
}
