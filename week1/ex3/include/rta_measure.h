#ifndef W1E3_RTA_MEASURE_H
#define W1E3_RTA_MEASURE_H
#include <stdint.h>
#define RTA_HYPERPERIODS 5u
void rta_measure_init(void);
void rta_register_task(uint32_t index, void *handle);
void rta_switched_in(void);
void rta_switched_out(void);
void rta_irq_enter(uint32_t kind);
void rta_irq_exit(uint32_t kind);
void rta_job_begin(uint32_t index, uint32_t sequence);
uint32_t rta_run_work(uint32_t index);
void rta_job_end(uint32_t index, uint32_t sequence, uint32_t wall);
void rta_task_done(void);
uint32_t rta_hyperperiod_ms(void);
uint32_t rta_start_tick(void);
#endif
