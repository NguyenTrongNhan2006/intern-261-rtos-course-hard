/* Reference-only layout for FreeRTOS-Kernel V11 tasks.c's TCB_t and the
 * List_t/ListItem_t it is linked through, to help you cast raw GDB memory
 * dumps into readable structs when debugging w3e4.elf (which you have NO
 * source for). This header is NOT compiled against the black-box firmware
 * and is NOT guaranteed to match its exact build configuration (some
 * fields only exist depending on FreeRTOSConfig.h options such as
 * configUSE_MUTEXES, configUSE_TRACE_FACILITY, configNUM_THREAD_LOCAL_
 * STORAGE_POINTERS, configGENERATE_RUN_TIME_STATS, portSTACK_GROWTH...).
 * Verify field presence/order yourself against the real FreeRTOS-Kernel
 * V11 tasks.c before trusting an offset - that verification is part of
 * the exercise, not something to skip.
 *
 * Typical GDB usage once you believe you have the right shape:
 *   (gdb) print *(TCB_t_ref *)pxCurrentTCB
 *   (gdb) print ((TCB_t_ref *)pxCurrentTCB)->pcTaskName
 */
#ifndef W3E4_TCB_REFERENCE_H
#define W3E4_TCB_REFERENCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ListItem_t (list.h): one node. xItemValue is the sort key (e.g. a
 * task's priority-inverted value in the ready list, or wake tick in the
 * delayed list). pvOwner points back to the TCB (or other object) that
 * embeds this node; pvContainer points to the List_t this node is
 * currently linked into (NULL if not linked anywhere). */
typedef struct ListItem_t_ref {
  uint32_t xItemValue;
  struct ListItem_t_ref *pxNext;
  struct ListItem_t_ref *pxPrevious;
  void *pvOwner;
  void *pvContainer;
} ListItem_t_ref;

/* List_t (list.h): circular doubly-linked list with a permanent "end
 * marker" item (xListEnd) whose xItemValue is portMAX_DELAY, used as the
 * traversal sentinel - walk pxNext until you come back to &xListEnd. */
typedef struct {
  uint32_t uxNumberOfItems;
  ListItem_t_ref *pxIndex;
  ListItem_t_ref xListEnd; /* NOTE: embeds an extra ListItem_t_ref, not a pointer */
} List_t_ref;

/* TCB_t (tasks.c) - reference shape only, GCC_ARM_CM3 port, heap_4,
 * configSUPPORT_STATIC_ALLOCATION=1, configUSE_MUTEXES=1,
 * configUSE_TRACE_FACILITY=0, configUSE_TASK_NOTIFICATIONS=1. The real
 * .elf may differ - cross-check with `ptype` / field offsets you derive
 * yourself from disassembly before relying on this. */
typedef struct {
  void *pxTopOfStack;                 /* first member always: saved SP */
  ListItem_t_ref xStateListItem;      /* links into pxReadyTasksLists[] or a blocked/suspended list */
  ListItem_t_ref xEventListItem;      /* links into a queue/semaphore/notify wait list */
  uint32_t uxPriority;
  void *pxStack;                      /* low address of the stack allocation */
  char pcTaskName[16];                /* configMAX_TASK_NAME_LEN, adjust if yours differs */
  void *pxEndOfStack;                 /* only when portSTACK_GROWTH > 0 or configRECORD_STACK_HIGH_ADDRESS */
  uint32_t uxBasePriority;            /* only if configUSE_MUTEXES == 1 */
  uint32_t uxMutexesHeld;             /* only if configUSE_MUTEXES == 1 */
  uint32_t ulNotifiedValue[1];        /* configTASK_NOTIFICATION_ARRAY_ENTRIES, often 1 */
  uint8_t  ucNotifyState[1];
} TCB_t_ref;

#ifdef __cplusplus
}
#endif

#endif /* W3E4_TCB_REFERENCE_H */
