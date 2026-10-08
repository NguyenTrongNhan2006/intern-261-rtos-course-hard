/* Interface for the W1.4 heap_4 fragmentation experiment and the static
 * conversion that follows it. Block sizes / free order / repeat count come
 * from params.h (W1E4_NUM_BLOCKS, W1E4_BLOCK_SIZE_BYTES, W1E4_FREE_ORDER,
 * W1E4_REPEAT_CYCLES). Everything here is yours to implement. */
#ifndef W1E4_HEAP_EXPERIMENT_H
#define W1E4_HEAP_EXPERIMENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t step;
  uint32_t free_bytes_now;      /* xPortGetFreeHeapSize() */
  uint32_t free_bytes_min_ever; /* xPortGetMinimumEverFreeHeapSize() */
} heap_log_entry_t;

/* Runs W1E4_REPEAT_CYCLES cycles of: allocate all W1E4_NUM_BLOCKS blocks
 * (sizes from params.h) with pvPortMalloc, then free them in
 * W1E4_FREE_ORDER, logging a heap_log_entry_t after every alloc/free.
 * Dump the log over UART (reuse your W1.1 driver) in whatever format you
 * document in REPORT.md. heap_4-only: call this BEFORE flipping
 * configSUPPORT_DYNAMIC_ALLOCATION to 0. */
void heap_experiment_run(void);

#ifdef __cplusplus
}
#endif

#endif /* W1E4_HEAP_EXPERIMENT_H */
