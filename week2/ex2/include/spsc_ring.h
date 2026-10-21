#ifndef W2E2_SPSC_RING_H
#define W2E2_SPSC_RING_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx.h"
#include "params.h"

#if (W2E2_RING_CAPACITY == 0u) || \
    ((W2E2_RING_CAPACITY & (W2E2_RING_CAPACITY - 1u)) != 0u)
#error "W2E2_RING_CAPACITY must be a power of two"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* W2.2 - STEP 1: Dữ liệu tĩnh; mỗi bộ đếm chỉ có một bên ghi. */
typedef struct {
    uint32_t seq;
} spsc_elem_t;

typedef struct {
    spsc_elem_t storage[W2E2_RING_CAPACITY];
    volatile uint32_t head; /* Producer ghi: số phần tử đã push. */
    volatile uint32_t tail; /* Consumer ghi: số phần tử đã pop. */
} spsc_ring_t;

void spsc_ring_init(spsc_ring_t *ring);
bool spsc_ring_push_from_isr(spsc_ring_t *ring, spsc_elem_t elem);
bool spsc_ring_pop(spsc_ring_t *ring, spsc_elem_t *out);
uint32_t spsc_ring_count(const spsc_ring_t *ring);

#ifdef W2E2_SPSC_RING_IMPLEMENTATION

/* W2.2 - STEP 2: Gọi trước khi ISR và task bắt đầu dùng ring. */
void spsc_ring_init(spsc_ring_t *ring)
{
    ring->head = 0u;
    ring->tail = 0u;
}

/* W2.2 - STEP 3: ISR ghi dữ liệu trước, công bố head sau. */
bool spsc_ring_push_from_isr(spsc_ring_t *ring, spsc_elem_t elem)
{
    const uint32_t head = ring->head;
    const uint32_t tail = ring->tail;

    if ((uint32_t)(head - tail) == W2E2_RING_CAPACITY) {
        return false;
    }

    __DMB(); /* Quan sát tail trước khi tái sử dụng slot. */
    ring->storage[head & (W2E2_RING_CAPACITY - 1u)] = elem;
    __DMB(); /* Ghi xong payload trước khi công bố head. */
    ring->head = head + 1u;

    return true;
}

/* W2.2 - STEP 4: Task đọc dữ liệu trước, trả slot qua tail sau. */
bool spsc_ring_pop(spsc_ring_t *ring, spsc_elem_t *out)
{
    const uint32_t tail = ring->tail;
    const uint32_t head = ring->head;

    if (tail == head) {
        return false;
    }

    __DMB(); /* Quan sát head trước khi đọc payload. */
    *out = ring->storage[tail & (W2E2_RING_CAPACITY - 1u)];
    __DMB(); /* Đọc xong payload trước khi trả slot. */
    ring->tail = tail + 1u;

    return true;
}

/* W2.2 - STEP 5: Snapshot chẩn đoán từ producer hoặc consumer. */
uint32_t spsc_ring_count(const spsc_ring_t *ring)
{
    const uint32_t head = ring->head;
    const uint32_t tail = ring->tail;

    return (uint32_t)(head - tail);
}

#endif /* W2E2_SPSC_RING_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* W2E2_SPSC_RING_H */
