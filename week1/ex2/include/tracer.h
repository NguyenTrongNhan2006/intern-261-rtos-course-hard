/* Interface for the W1.2 trace-hook tracer. Wire these into
 * traceTASK_SWITCHED_IN / traceTASK_SWITCHED_OUT in your own
 * FreeRTOSConfig.h; the ring-buffer storage, GPIO tagging and UART dump
 * are entirely yours to implement. */
#ifndef W1E2_TRACER_H
#define W1E2_TRACER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Event values stored in tracer_record_t.event. */
#define TRACER_EVENT_IN  0u
#define TRACER_EVENT_OUT 1u

typedef struct {
  uint32_t timestamp_cycles; /* DWT->CYCCNT at the moment of the event */
  uint8_t  task_tag;         /* value set via vTaskSetApplicationTaskTag() */
  uint8_t  event;            /* 0 = switched-in, 1 = switched-out */
} tracer_record_t;

/* Call once before vTaskStartScheduler(): enables DWT->CYCCNT and resets
 * the ring buffer. */
void tracer_init(void);

/* Call from traceTASK_SWITCHED_IN() / traceTASK_SWITCHED_OUT(). */
void tracer_on_switched_in(void);
void tracer_on_switched_out(void);

/* Dumps the ring buffer over UART1 as the text/binary framing you define
 * (document the format for tools/trace_gantt.py in REPORT.md). Returns the
 * number of records dumped. */
uint32_t tracer_dump_uart(void);

/* Additional application helpers; the original interface above is preserved.
 * Append an event from a single producer and update its GPIO marker.
 * Returns false if recording is stopped, input is invalid or the buffer
 * is full. */
bool tracer_record(
    uint32_t timestamp_cycles,
    uint8_t task_tag,
    uint8_t event
);

/* Freeze recording and clear the trace GPIOs before dumping the buffer. */
void tracer_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* W1E2_TRACER_H */