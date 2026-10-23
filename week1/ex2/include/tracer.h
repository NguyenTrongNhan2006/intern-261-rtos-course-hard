/* Interface for the W1.2 trace-hook tracer. Wire these into
 * traceTASK_SWITCHED_IN / traceTASK_SWITCHED_OUT in your own
 * FreeRTOSConfig.h; the ring-buffer storage, GPIO tagging and UART dump
 * are entirely yours to implement. */
#ifndef W1E2_TRACER_H
#define W1E2_TRACER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/* Dừng ghi trước khi xuất log. */
void tracer_stop(void);

/* Dumps the ring buffer over UART1 as the text/binary framing you define
 * (document the format for tools/trace_gantt.py in REPORT.md). Returns the
 * number of records dumped. */
uint32_t tracer_dump_uart(void);

#ifdef __cplusplus
}
#endif

#endif /* W1E2_TRACER_H */
