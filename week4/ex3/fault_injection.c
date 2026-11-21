#include "w4e3_internal.h"
#include "event_groups.h"
#include "params.h"
#include "../../week3/ex3/include/params.h"

#include <stddef.h>

enum {
    RECORD_ARMED = 1u, RECORD_CAPTURED, RECORD_HARVESTED,
    IWDG_DIV8 = 1u, IWDG_RELOAD = 1109u,
    HANG_DELAY_MS = 5u, DIAGNOSTIC_AGE_MS = 120u, MAX_KICK_AGE_FOR_STOP_MS = 60u
};

_Static_assert((IWDG_RELOAD + 1u) * 8u * 1000u <=
                   30000u * W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS,
               "IWDG deadline at minimum LSI frequency");

typedef struct {
    uint32_t canary, reset_reason_code, fault_kind, hung_role;
    uint32_t control_deadline_missed, stacked_pc, stacked_lr, cfsr;
    uint32_t uptime_ticks, state, checksum;
} reset_record_t;

/* Retain the armed record if reset interrupts the write of a captured frame. */
static volatile reset_record_t journal[2] __attribute__((section(".noinit"), aligned(4)));
static StaticEventGroup_t fault_storage;
static EventGroupHandle_t fault_events;
static volatile w4e3_fault_kind_t active_kind = W4E3_FAULT_COUNT;
static volatile uint32_t target_role;
static volatile TickType_t armed_at, last_kick;
static volatile uint32_t baseline_misses;
static volatile bool reset_armed, hung_now;
static bool pending, api_waiting, track_result;
static bool capture_written, capture_miss, capture_frame, miss_announced;
static uint32_t uart_generation;
static volatile uint32_t missing_role = W4E3_ROLE_UNKNOWN;
static w4e3_fault_result_t last_result = { .kind = W4E3_FAULT_COUNT };
static reset_record_t last_record = { .hung_role = W4E3_ROLE_UNKNOWN };

static uint32_t checksum(const volatile reset_record_t *record)
{
    const volatile uint8_t *byte = (const volatile uint8_t *)record;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(reset_record_t, checksum); ++i) {
        hash = (hash ^ byte[i]) * 16777619u;
    }
    return hash;
}

static void commit_record(unsigned slot, const reset_record_t *record)
{
    journal[slot] = *record;
    __DSB();
    journal[slot].checksum = checksum(&journal[slot]);
    __DSB();
}

static void harvest_records(void)
{
    journal[0].state = RECORD_HARVESTED;
    journal[1].state = RECORD_HARVESTED;
    __DSB();
}

static bool valid_record(unsigned slot)
{
    return journal[slot].canary == W3E3_NOINIT_CANARY &&
           journal[slot].fault_kind < W4E3_FAULT_COUNT &&
           journal[slot].hung_role <= W4E3_ROLE_UNKNOWN &&
           journal[slot].reset_reason_code != 0u &&
           journal[slot].state == (slot == 0u ? RECORD_ARMED : RECORD_CAPTURED) &&
           journal[slot].checksum == checksum(&journal[slot]);
}

static reset_record_t record_for(w4e3_fault_kind_t kind, uint32_t role,
                                 TickType_t now, uint32_t state)
{
    return (reset_record_t){
        .canary = W3E3_NOINIT_CANARY,
        .reset_reason_code = (((uint32_t)kind + 1u) << 8u) | (role + 1u),
        .fault_kind = kind, .hung_role = role, .uptime_ticks = now, .state = state
    };
}

static void read_frame(reset_record_t *record, const uint32_t *frame)
{
    const uintptr_t address = (uintptr_t)frame;
    if ((address & 3u) == 0u && address >= SRAM_BASE &&
        address <= SRAM_BASE + 20u * 1024u - 8u * sizeof(uint32_t)) {
        record->stacked_lr = frame[5];
        record->stacked_pc = frame[6];
    }
    record->cfsr = SCB->CFSR;
}

void fault_injection_init(uint32_t *out_prev_reason)
{
    fault_events = xEventGroupCreateStatic(&fault_storage);
    configASSERT(fault_events != NULL);
    const uint32_t flags = RCC->CSR;
    const bool watchdog = (flags & RCC_CSR_IWDGRSTF) != 0u;
    const bool software = (flags & RCC_CSR_SFTRSTF) != 0u && valid_record(1u) &&
                          (journal[1].reset_reason_code & 0x60000000u) != 0u;
    if (watchdog || software) {
        if (valid_record(1u)) {
            last_record = journal[1];
        } else if (valid_record(0u)) {
            last_record = journal[0];
        } else {
            last_record = record_for(W4E3_FAULT_TASK_HANG, W4E3_ROLE_UNKNOWN, 0u, 0u);
            last_record.reset_reason_code |= 0x80000000u;
        }
        last_result = (w4e3_fault_result_t){
            .kind = (w4e3_fault_kind_t)last_record.fault_kind,
            .control_deadline_missed = last_record.control_deadline_missed != 0u,
            .was_declared_controlled_degradation = true,
            .watchdog_reset_reason_code = last_record.reset_reason_code
        };
        (void)xEventGroupSetBits(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED |
                                             (watchdog ? W4E3_BIT_WATCHDOG_RECOVERED : 0u));
    }
    if (out_prev_reason != NULL) {
        *out_prev_reason = last_result.watchdog_reset_reason_code;
    }
    harvest_records();
    RCC->CSR |= RCC_CSR_RMVF;

    IWDG->KR = 0xccccu;
    IWDG->KR = 0x5555u;
    IWDG->PR = IWDG_DIV8;
    IWDG->RLR = IWDG_RELOAD;
    while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) {
        __NOP();
    }
    IWDG->KR = 0xaaaau;
}

static void seal_result(void)
{
    if (track_result) {
        last_result.control_deadline_missed |= w4e3_control_misses() != baseline_misses;
        track_result = false;
    }
}

static bool request(w4e3_fault_kind_t kind, uint32_t role, bool high, bool wait)
{
    if ((unsigned)kind >= W4E3_FAULT_COUNT) {
        return false;
    }
    taskENTER_CRITICAL();
    if (api_waiting || pending || reset_armed || !w4e3_system_running()) {
        taskEXIT_CRITICAL();
        return false;
    }
    seal_result();
    (void)xEventGroupClearBits(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED |
                                            W4E3_BIT_WATCHDOG_RECOVERED);
    last_result = (w4e3_fault_result_t){ .kind = kind };
    baseline_misses = w4e3_control_misses();
    armed_at = xTaskGetTickCount();
    capture_written = false;
    capture_miss = false;
    capture_frame = false;
    miss_announced = false;
    journal[1].state = RECORD_HARVESTED;
    active_kind = kind;
    target_role = role;
    api_waiting = wait;
    pending = true;
    reset_armed = kind == W4E3_FAULT_TASK_HANG || high;
    const reset_record_t armed = record_for(kind, role, armed_at, RECORD_ARMED);
    commit_record(0u, &armed);
    taskEXIT_CRITICAL();

    if (kind == W4E3_FAULT_SENSOR_UNPLUG) {
        w4e3_imu_set_cut(true);
    } else if (kind == W4E3_FAULT_UART_FLOOD) {
        w4e3_uart_bad_frame();
        w4e3_fault_observe_uart(w4e3_uart_rx_faults(), w4e3_uart_misses_at_cooldown());
    } else if (kind == W4E3_FAULT_CPU_OVERLOAD) {
        w4e3_start_load(high);
    } else if (role == W4E1_ROLE_IDLE_AUX) {
        w4e3_wake_aux();
    }
    return true;
}

bool w4e3_fault_request(w4e3_fault_kind_t kind, uint32_t role, bool high)
{
    return request(kind, role, high, false);
}

void fault_injection_trigger(w4e3_fault_kind_t kind)
{
    const uint32_t role = kind == W4E3_FAULT_TASK_HANG ? W4E1_ROLE_TELEMETRY :
                          kind == W4E3_FAULT_CPU_OVERLOAD ? W4E3_ROLE_LOAD :
                          W4E3_ROLE_UNKNOWN;
    if (request(kind, role, false, true)) {
        /* A hardware reset ends this invocation. Boot restores the bits. */
        (void)xEventGroupWaitBits(fault_events, W4E3_BIT_WATCHDOG_RECOVERED |
                                              W4E3_BIT_DEGRADED_MODE_ENTERED,
                                 pdFALSE, pdFALSE, pdMS_TO_TICKS(500u));
        taskENTER_CRITICAL();
        api_waiting = false;
        taskEXIT_CRITICAL();
    }
}

static void observe(w4e3_fault_kind_t kind, uint32_t misses_at_start)
{
    taskENTER_CRITICAL();
    if (!reset_armed && (pending ? active_kind == kind : !api_waiting)) {
        if (!pending) {
            seal_result();
            baseline_misses = misses_at_start;
        }
        last_result = (w4e3_fault_result_t){
            .kind = kind,
            .control_deadline_missed = w4e3_control_misses() != baseline_misses,
            .was_declared_controlled_degradation = true
        };
        last_record = (reset_record_t){ .hung_role = W4E3_ROLE_UNKNOWN };
        pending = false;
        track_result = true;
        active_kind = W4E3_FAULT_COUNT;
        harvest_records();
        (void)xEventGroupSetBits(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED);
    }
    taskEXIT_CRITICAL();
}

void w4e3_fault_observe(w4e3_fault_kind_t kind)
{
    observe(kind, w4e3_control_misses());
}

void w4e3_fault_observe_uart(uint32_t generation, uint32_t misses_at_start)
{
    taskENTER_CRITICAL();
    if (generation != uart_generation || (pending && active_kind == W4E3_FAULT_UART_FLOOD)) {
        uart_generation = generation;
        observe(W4E3_FAULT_UART_FLOOD, misses_at_start);
    }
    taskEXIT_CRITICAL();
}

bool w4e3_fault_wait_step(TickType_t wait)
{
    const EventBits_t mask = W4E3_BIT_DEGRADED_MODE_ENTERED | W4E3_BIT_WATCHDOG_RECOVERED;
    return (xEventGroupWaitBits(fault_events, mask, pdFALSE, pdFALSE, wait) & mask) != 0u;
}

bool w4e3_fault_clear(void)
{
    taskENTER_CRITICAL();
    if (reset_armed || api_waiting) {
        taskEXIT_CRITICAL();
        return false;
    }
    seal_result();
    pending = false;
    active_kind = W4E3_FAULT_COUNT;
    harvest_records();
    (void)xEventGroupClearBits(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED |
                                            W4E3_BIT_WATCHDOG_RECOVERED);
    w4e3_imu_set_cut(false);
    w4e3_uart_clear_throttle();
    taskEXIT_CRITICAL();
    w4e3_leave_hi_mode();
    return true;
}

bool w4e3_fault_lo_load(void)
{
    return active_kind == W4E3_FAULT_CPU_OVERLOAD && !reset_armed;
}

bool w4e3_fault_allows_stop(void)
{
    return active_kind == W4E3_FAULT_COUNT;
}

void w4e3_fault_check_hang(uint32_t role)
{
    if (!reset_armed || target_role != role) {
        return;
    }
    const TickType_t spent = xTaskGetTickCount() - armed_at;
    if (spent < pdMS_TO_TICKS(HANG_DELAY_MS)) {
        vTaskDelay(pdMS_TO_TICKS(HANG_DELAY_MS) - spent);
    }
    hung_now = true;
    for (;;) {
        __NOP();
    }
}

bool w4e3_watchdog_allows_stop(void)
{
    /* A full 80 ms STOP leaves at least 8 ms at the fastest LSI. */
    return !reset_armed && xTaskGetTickCount() - last_kick <=
                           pdMS_TO_TICKS(MAX_KICK_AGE_FOR_STOP_MS);
}

void w4e3_watchdog_check(uint32_t missing_mask)
{
    taskENTER_CRITICAL();
    if (missing_mask == 0u && !reset_armed) {
        IWDG->KR = 0xaaaau;
        last_kick = xTaskGetTickCount();
        missing_role = W4E3_ROLE_UNKNOWN;
        capture_written = false;
        if (journal[1].state == RECORD_CAPTURED) {
            journal[1].state = RECORD_HARVESTED;
        }
    } else if (missing_mask != 0u) {
        missing_role = (uint32_t)__builtin_ctz(missing_mask);
    }
    taskEXIT_CRITICAL();
}

void w4e3_fault_timer(const uint32_t *frame, uint32_t exc_return, BaseType_t *woken)
{
    const TickType_t now = xTaskGetTickCountFromISR();
    const bool missed = reset_armed && w4e3_control_misses() != baseline_misses;
    const bool thread_frame = (exc_return & 4u) != 0u;
    const bool target_frame = reset_armed && hung_now && thread_frame &&
                              w4e3_current_role() == target_role;

    /* At 60 kHz the watchdog can expire after 148 ms from the last feed. */
    if ((target_frame || missed || now - last_kick >= pdMS_TO_TICKS(DIAGNOSTIC_AGE_MS)) &&
        (!capture_written || (missed && !capture_miss) || (target_frame && !capture_frame))) {
        const uint32_t role = reset_armed ? target_role :
            (missing_role == W4E3_ROLE_UNKNOWN && thread_frame ? w4e3_current_role() : missing_role);
        const w4e3_fault_kind_t kind = reset_armed ? active_kind : W4E3_FAULT_TASK_HANG;
        reset_record_t record = record_for(kind, role, now, RECORD_CAPTURED);
        if (target_frame || !reset_armed) {
            read_frame(&record, frame);
        } else if (capture_frame) {
            record.stacked_pc = journal[1].stacked_pc;
            record.stacked_lr = journal[1].stacked_lr;
            record.cfsr = journal[1].cfsr;
        }
        record.control_deadline_missed = missed;
        commit_record(1u, &record);
        capture_written = true;
        capture_miss = missed;
        capture_frame |= target_frame;
    }
    if (missed && !miss_announced) {
        /* The timer daemon applies the bit if runnable. Boot restores it on IWDG reset. */
        miss_announced = xEventGroupSetBitsFromISR(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED,
                                                  woken) == pdPASS;
    }
}

static void reset_after_fault(uint32_t role, const uint32_t *frame, uint32_t source)
    __attribute__((noreturn));

static void reset_after_fault(uint32_t role, const uint32_t *frame, uint32_t source)
{
    __disable_irq();
    reset_record_t record = record_for(reset_armed ? active_kind : W4E3_FAULT_TASK_HANG,
                                       reset_armed ? target_role : role,
                                       xTaskGetTickCount(), RECORD_CAPTURED);
    record.reset_reason_code |= source;
    record.control_deadline_missed = role == W4E1_ROLE_CONTROL ||
                                     (reset_armed && w4e3_control_misses() != baseline_misses);
    read_frame(&record, frame);
    commit_record(1u, &record);
    NVIC_SystemReset();
}

void w4e3_hardfault(const uint32_t *frame, uint32_t exc_return) __attribute__((used, noreturn));
void w4e3_hardfault(const uint32_t *frame, uint32_t exc_return)
{
    const uint32_t role = (exc_return & 4u) != 0u ? w4e3_current_role() : W4E3_ROLE_UNKNOWN;
    reset_after_fault(role, frame, 0x40000000u);
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b w4e3_hardfault\n");
}

void w4e3_fault_stack_overflow(uint32_t role)
{
    reset_after_fault(role, NULL, 0x20000000u);
}

void fault_injection_get_last_result(w4e3_fault_result_t *out)
{
    if (out != NULL) {
        taskENTER_CRITICAL();
        *out = last_result;
        out->control_deadline_missed |= track_result && w4e3_control_misses() != baseline_misses;
        taskEXIT_CRITICAL();
    }
}

static void append_hex(reply_writer_t *writer, uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    char text[9] = {0};
    for (unsigned i = 0u; i < 8u; ++i) {
        text[i] = digits[(value >> (28u - 4u * i)) & 15u];
    }
    w4e3_append_text(writer, text);
}

void w4e3_format_fault(reply_writer_t *writer)
{
    static const uint8_t assigned_index[W4E3_FAULT_COUNT] = {
        [W4E3_FAULT_TASK_HANG] = 1u, [W4E3_FAULT_UART_FLOOD] = 3u,
        [W4E3_FAULT_SENSOR_UNPLUG] = 0u, [W4E3_FAULT_CPU_OVERLOAD] = 2u
    };
    w4e3_fault_result_t result;
    taskENTER_CRITICAL();
    fault_injection_get_last_result(&result);
    const reset_record_t record = last_record;
    const bool waiting = pending;
    taskEXIT_CRITICAL();
    if (waiting || result.kind == W4E3_FAULT_COUNT) {
        w4e3_append_text(writer, waiting ? "fault pending\r\n" : "fault none\r\n");
        return;
    }
    w4e3_append_text(writer, "kind=");
    w4e3_append_text(writer, W4E3_ASSIGNED_FAULT_NAMES[assigned_index[result.kind]]);
    w4e3_append_text(writer, result.control_deadline_missed ? " miss=1" : " miss=0");
    w4e3_append_text(writer, result.was_declared_controlled_degradation ?
                    " decl=1 wdg_code=" : " decl=0 wdg_code=");
    append_hex(writer, result.watchdog_reset_reason_code);
    w4e3_append_text(writer, " pc=");
    append_hex(writer, record.stacked_pc);
    w4e3_append_text(writer, " lr=");
    append_hex(writer, record.stacked_lr);
    w4e3_append_text(writer, " role=");
    w4e3_append_text(writer, w4e3_role_name(record.hung_role));
    w4e3_append_text(writer, " cfsr=");
    append_hex(writer, record.cfsr);
    w4e3_append_text(writer, " t=");
    w4e3_append_number(writer, record.uptime_ticks);
    w4e3_append_text(writer, "\r\n");
}
