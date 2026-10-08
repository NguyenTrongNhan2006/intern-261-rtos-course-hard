/* Interface for the W2.2 SPSC lock-free ring buffer. Capacity, stress
 * count and benchmark iteration count come from params.h
 * (W2E2_RING_CAPACITY, W2E2_STRESS_COUNT, W2E2_BENCH_ITERATIONS).
 * Memory-barrier placement, the static storage array and the
 * timer-ISR producer / task consumer wiring are entirely yours to
 * implement. */
#ifndef W2E2_SPSC_RING_H
#define W2E2_SPSC_RING_H

#include <stdint.h>
#include <stdbool.h>
#include "params.h" /* W2E2_RING_CAPACITY, W2E2_STRESS_COUNT, W2E2_BENCH_ITERATIONS */

#ifdef __cplusplus
extern "C" {
#endif

/* Element type pushed by the ISR producer and popped by the task
 * consumer; `seq` is the monotonically increasing sequence counter used
 * by the stress test to detect loss/duplication. */
typedef struct {
  uint32_t seq;
} spsc_elem_t;

typedef struct {
  spsc_elem_t storage[W2E2_RING_CAPACITY]; /* capacity from params.h, must be a power of 2 */
  volatile uint32_t head; /* next free slot index, written only by producer */
  volatile uint32_t tail; /* next readable slot index, written only by consumer */
} spsc_ring_t;

/* Resets head/tail to 0. Call once before the producer ISR / consumer
 * task start running. Not safe to call concurrently with push/pop. */
void spsc_ring_init(spsc_ring_t *ring);

/* Producer side, called from ISR context only. Returns false if the ring
 * is full (overrun) instead of overwriting unread data. Must publish the
 * element's contents before publishing the new head index (DMB/DSB). */
bool spsc_ring_push_from_isr(spsc_ring_t *ring, spsc_elem_t elem);

/* Consumer side, called from task context only. Returns false if the ring
 * is empty. Must not reorder the read of storage[tail] ahead of observing
 * the producer's published head index. */
bool spsc_ring_pop(spsc_ring_t *ring, spsc_elem_t *out);

/* Number of elements currently queued (head - tail mod capacity). Safe to
 * call from either side for diagnostics; not required to be exact under
 * concurrent access, only safe (no crash, no out-of-range index). */
uint32_t spsc_ring_count(const spsc_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif /* W2E2_SPSC_RING_H */
