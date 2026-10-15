#ifndef W1E3_RTA_MEASURE_H
#define W1E3_RTA_MEASURE_H

#include <stdint.h>

#define RTA_HYPERPERIODS 5u

void rta_switched_in(void);
void rta_switched_out(void);

void rta_measure_init(void);
void rta_register_task(uint32_t index, void *handle);

uint32_t rta_selected_cycles(uint32_t index);

void rta_job_begin(uint32_t index);
void rta_job_end(
    uint32_t index,
    uint32_t release,
    uint32_t cpu_cycles
);

void rta_task_done(void);

uint32_t rta_epoch_cycles(void);
uint32_t rta_hyperperiod_ms(void);
uint32_t rta_start_tick(void);

#endif