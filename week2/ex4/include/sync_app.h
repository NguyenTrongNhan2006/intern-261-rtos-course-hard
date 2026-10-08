/* Interface for the W2.4 event-group barrier + notification-based
 * signalling (with its seeded eSetValueWithOverwrite bug, found and then
 * fixed). Task count / sync bits / timeout come from params.h
 * (W2E4_NUM_TASKS, W2E4_SYNC_BITS_MASK, W2E4_SYNC_TIMEOUT_MS). Event-group
 * creation, the notification-based send/receive pair, the tracer
 * instrumentation used to count sends vs. receives, and the fixed version
 * are entirely yours to implement. */
#ifndef W2E4_SYNC_APP_H
#define W2E4_SYNC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Barrier participant body: does its share of work, then calls
 * xEventGroupSync() with its own bit ORed into both "bits to set" and
 * "bits to wait for" (W2E4_SYNC_BITS_MASK), bounded by
 * W2E4_SYNC_TIMEOUT_MS. idx (0..W2E4_NUM_TASKS-1) is passed as
 * pvParameters and selects which bit this task owns. */
void vTaskBarrierParticipant(void *pvParameters);

/* Creates the W2E4_NUM_TASKS barrier participants and the shared static
 * event group. Call once before vTaskStartScheduler(). */
void sync_app_init_barrier(void);

/* Buggy variant: signals "new event" to a single receiver task via
 * xTaskNotify(..., eSetValueWithOverwrite) from one or more sender
 * contexts. Intentionally loses events when a second send arrives before
 * the receiver's xTaskNotifyWait() has consumed the first. */
void sync_app_notify_send_buggy(uint32_t value);
void sync_app_notify_receive_buggy(uint32_t *out_value);

/* Fixed variant: same send/receive shape, but using a notification
 * strategy that cannot silently drop an event under the same timing
 * (e.g. eSetBits with an accumulating bit per event, or eIncrement with a
 * matching drain loop on the receive side — your choice, document it in
 * REPORT.md). */
void sync_app_notify_send_fixed(uint32_t value);
void sync_app_notify_receive_fixed(uint32_t *out_value);

/* Returns the running totals your tracer instrumentation has counted so
 * far for the variant currently wired up: how many sends vs. how many
 * events actually observed by the receiver. A mismatch is the proof of
 * the lost-event bug (or its absence after the fix). */
void sync_app_get_counts(uint32_t *sent_count, uint32_t *received_count);

#ifdef __cplusplus
}
#endif

#endif /* W2E4_SYNC_APP_H */
