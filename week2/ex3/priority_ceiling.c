#include "priority_ceiling.h"
#include "task.h"

/* STEP 1: Tạo mutex và lưu ceiling. */
void pc_mutex_create_static(pc_mutex_t *m, UBaseType_t ceiling_prio)
{
    configASSERT(m != NULL);
    configASSERT(ceiling_prio < configMAX_PRIORITIES);

    m->ceiling_prio = ceiling_prio;
    m->saved_prio = 0u;
    m->handle = xSemaphoreCreateMutexStatic(&m->storage);
    configASSERT(m->handle != NULL);
}

/* STEP 2: Nâng priority rồi take; thành công mới lưu owner priority. */
void pc_mutex_lock(pc_mutex_t *m)
{
    configASSERT(m != NULL && m->handle != NULL);
    const UBaseType_t previous_prio = uxTaskPriorityGet(NULL);

    if (previous_prio < m->ceiling_prio) {
        vTaskPrioritySet(NULL, m->ceiling_prio);
    }

    const BaseType_t taken = xSemaphoreTake(m->handle, portMAX_DELAY);
    configASSERT(taken == pdTRUE);
    m->saved_prio = previous_prio;
}

/* STEP 3: Nhả mutex trước khi trả priority. */
void pc_mutex_unlock(pc_mutex_t *m)
{
    configASSERT(m != NULL && m->handle != NULL);
    const UBaseType_t previous_prio = m->saved_prio;

    const BaseType_t given = xSemaphoreGive(m->handle);
    configASSERT(given == pdTRUE);
    vTaskPrioritySet(NULL, previous_prio);
}