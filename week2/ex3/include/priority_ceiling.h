/* Interface for the W2.3 Immediate Priority Ceiling Protocol (IPCP)
 * wrapper, built on top of the raw FreeRTOS semaphore/mutex API (no
 * modification to the kernel itself). Task priorities and lock hold
 * times for the 4-task / 2-mutex scenario come from params.h
 * (W2E3_TASK_PRIORITY[4], W2E3_LOCK_HOLD_MS_A/B).
 * blocking_app.c runs the three scenarios with and without ceiling. */
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

/* Raise the CALLING task to ceiling_prio before taking the mutex. Save its
 * previous priority only after acquisition succeeds. This demonstration
 * assumes correct ceilings, no time slicing or self-suspension while a
 * lock is held, no recursive acquisition, and LIFO release of nested locks. */
void pc_mutex_lock(pc_mutex_t *m);

/* Give the mutex, then restore the priority saved by the matching lock.
 * The caller must own this mutex and release nested locks in LIFO order. */
void pc_mutex_unlock(pc_mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif /* W2E3_PRIORITY_CEILING_H */
