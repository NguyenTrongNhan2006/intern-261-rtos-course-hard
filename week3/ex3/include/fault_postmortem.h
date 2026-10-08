/* Interface for the W3.3 HardFault post-mortem record. Assigned fault
 * names / canary come from params.h (W3E3_NUM_ASSIGNED_FAULTS,
 * W3E3_ASSIGNED_FAULT_NAMES, W3E3_NOINIT_CANARY). The naked
 * HardFault_Handler (MSP/PSP selection from EXC_RETURN), placing
 * fault_record_t into your own .noinit linker section, and the fault
 * trigger functions themselves are entirely yours to implement. */
#ifndef W3E3_FAULT_POSTMORTEM_H
#define W3E3_FAULT_POSTMORTEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lives in .noinit: must survive a HardFault-triggered reset untouched. */
typedef struct {
  uint32_t canary;       /* write W3E3_NOINIT_CANARY right before a deliberate fault */
  uint32_t stacked_pc;
  uint32_t stacked_lr;
  uint32_t stacked_xpsr;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t bfar;
  uint32_t mmfar;
  bool     bfar_valid;   /* SCB->CFSR BFARVALID bit */
  bool     mmfar_valid;  /* SCB->CFSR MMARVALID bit */
  char     task_name[16];
} fault_record_t;

/* Called from your C fault handler (invoked by the naked
 * HardFault_Handler with a pointer to the stacked exception frame on
 * whichever stack - MSP or PSP - was active): fills in a fault_record_t
 * from SCB->CFSR/HFSR/BFAR/MMFAR, pcTaskGetName(xTaskGetCurrentTaskHandle()),
 * and the stacked PC/LR/xPSR, then writes it into the .noinit record
 * (canary assumed already set by the code that triggered the fault). Does
 * not return (halt or controlled reset - your choice, document it). */
void fault_postmortem_capture(const uint32_t *stacked_frame);

/* Call early in main(), before anything else touches the record: returns
 * true if the .noinit record's canary matches W3E3_NOINIT_CANARY (i.e. a
 * valid post-mortem is waiting), in which case *out is filled in. */
bool fault_postmortem_check_and_read(fault_record_t *out);

/* Prints a fault_record_t over UART1 in whatever human-readable format you
 * choose - must include every field needed to run addr2line on
 * stacked_pc and to identify which of W3E3_ASSIGNED_FAULT_NAMES this was. */
void fault_postmortem_print(const fault_record_t *rec);

/* Writes W3E3_NOINIT_CANARY into the record right before you deliberately
 * trigger one of W3E3_ASSIGNED_FAULT_NAMES, so capture() above can later
 * confirm .noinit really did survive the reset. */
void fault_postmortem_arm_canary(void);

#ifdef __cplusplus
}
#endif

#endif /* W3E3_FAULT_POSTMORTEM_H */
