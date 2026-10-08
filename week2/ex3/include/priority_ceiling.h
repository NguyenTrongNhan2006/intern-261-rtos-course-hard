/* Interface for the W2.3 Immediate Priority Ceiling Protocol (IPCP)
 * wrapper, built on top of the raw FreeRTOS semaphore/mutex API (no
 * modification to the kernel itself). Task priorities / periods / lock
 * hold times for the 4-task / 2-mutex scenario come from params.h
 * (W2E3_TASK_PRIORITY[4], W2E3_TASK_PERIOD_MS[4], W2E3_LOCK_HOLD_MS_A/B).
 * The 3 demonstration scenarios (inversion, chained blocking, deadlock)
 * and the ceiling values themselves are yours to design and implement. */
#ifndef W2E3_PRIORITY_CEILING_H
#define W2E3_PRIORITY_CEILING_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  SemaphoreHandle_t   handle;      /* underlying xSemaphoreCreateMutexStatic() */
  StaticSemaphore_t   storage;     /* static backing buffer, no heap */
  UBaseType_t         ceiling_prio; /* max priority of any task that ever locks this resource */
  UBaseType_t         saved_prio;   /* owner's priority before the boost, restored on unlock */
} pc_mutex_t;

/* Initializes the static mutex and records ceiling_prio (you compute this
 * ahead of time as the highest priority among all tasks that can ever call
 * pc_mutex_lock() on this resource). */
void pc_mutex_create_static(pc_mutex_t *m, UBaseType_t ceiling_prio);

/* Immediate Priority Ceiling lock: takes the underlying mutex, then raises
 * the CALLING task's priority to ceiling_prio via vTaskPrioritySet()
 * before returning (the boost happens even if the mutex was free, which
 * is what makes this "immediate" rather than inheritance-on-contention). */
void pc_mutex_lock(pc_mutex_t *m);

/* Restores the calling task's priority to what it was before the matching
 * pc_mutex_lock() (saved_prio), then gives the underlying mutex. */
void pc_mutex_unlock(pc_mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif /* W2E3_PRIORITY_CEILING_H */
