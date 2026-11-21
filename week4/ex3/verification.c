#include "w4e3_internal.h"
#include "timers.h"

#include <string.h>

enum { IDLE_COUNTER = W4E3_ROLE_LOAD + 1u, TIMER_COUNTER, RUNTIME_TASKS };
_Static_assert(RUNTIME_TASKS == 8u, "five roles, load, idle and timer daemon");

static const char *const role_names[] = { "ctl", "imu", "tel", "sh", "aux", "load", "?" };
static TaskHandle_t handles[RUNTIME_TASKS];
static TaskStatus_t s_status_array[8];
static uint32_t max_cycles[W4E1_NUM_ROLES], imu_e2e_cycles;
static uint32_t previous[RUNTIME_TASKS], previous_total;
static uint32_t hwm[W4E1_NUM_ROLES];
static uint8_t cpu[RUNTIME_TASKS];

/* Keep the application implementation callable via the FreeRTOS macro. */
void vTaskGetRunTimeStatistics(char *pcWriteBuffer, size_t uxBufferLength)
    __attribute__((noinline));

const char *w4e3_role_name(uint32_t role)
{
    return role_names[role < W4E3_ROLE_UNKNOWN ? role : W4E3_ROLE_UNKNOWN];
}

void w4e3_append_text(reply_writer_t *writer, const char *text)
{
    const size_t count = strlen(text);
    if (count >= writer->room) {
        writer->full = true;
        return;
    }
    memcpy(writer->next, text, count + 1u);
    writer->next += count;
    writer->room -= count;
}

void w4e3_append_number(reply_writer_t *writer, uint32_t value)
{
    char digits[11];
    char *first = digits + sizeof digits - 1u;
    *first = '\0';
    do {
        *--first = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    w4e3_append_text(writer, first);
}

void w4e3_verification_init(const TaskHandle_t *roles, TaskHandle_t load)
{
    memcpy(handles, roles, W4E1_NUM_ROLES * sizeof handles[0]);
    handles[W4E3_ROLE_LOAD] = load;
}

void w4e3_measure(uint32_t role, uint32_t cycles)
{
    if (cycles > max_cycles[role]) {
        max_cycles[role] = cycles;
    }
}

void w4e3_measure_imu_e2e(uint32_t cycles)
{
    if (cycles > imu_e2e_cycles) {
        imu_e2e_cycles = cycles;
    }
}

static uint32_t percent(uint32_t ticks, uint32_t total)
{
    if (total == 0u) {
        return 0u;
    }
    return ticks >= total ? 100u : (uint32_t)((uint64_t)ticks * 100u / total);
}

uint32_t w4e3_verification_collect(void)
{
    uint32_t counters[RUNTIME_TASKS];
    uint32_t marks[W4E1_NUM_ROLES];
    taskENTER_CRITICAL();
    const uint32_t total = w3e1_runtime_stats_now();
    for (uint32_t i = 0u; i <= W4E3_ROLE_LOAD; ++i) {
        counters[i] = ulTaskGetRunTimeCounter(handles[i]);
    }
    counters[IDLE_COUNTER] = ulTaskGetIdleRunTimeCounter();
    counters[TIMER_COUNTER] = ulTaskGetRunTimeCounter(xTimerGetTimerDaemonTaskHandle());
    taskEXIT_CRITICAL();

    for (uint32_t i = 0u; i < W4E1_NUM_ROLES; ++i) {
        marks[i] = uxTaskGetStackHighWaterMark(handles[i]);
    }
    const uint32_t window = total - previous_total;
    uint8_t values[RUNTIME_TASKS];
    for (uint32_t i = 0u; i < RUNTIME_TASKS; ++i) {
        values[i] = (uint8_t)percent(counters[i] - previous[i], window);
    }
    memcpy(previous, counters, sizeof previous);
    previous_total = total;
    taskENTER_CRITICAL();
    memcpy(hwm, marks, sizeof hwm);
    memcpy(cpu, values, sizeof cpu);
    taskEXIT_CRITICAL();
    return window != 0u ? 100u - values[IDLE_COUNTER] : 0u;
}

uint32_t w4e3_stack_hwm(uint32_t role)
{
    return hwm[role];
}

uint32_t w4e3_runtime_percent(uint32_t role)
{
    return cpu[role == W4E1_NUM_ROLES ? IDLE_COUNTER :
               role == W4E1_NUM_ROLES + 1u ? TIMER_COUNTER : role];
}

void fault_injection_get_verification_sample(uint32_t role, w4e3_verification_sample_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = (w4e3_verification_sample_t){0};
    if (role < W4E1_NUM_ROLES) {
        const uint32_t clocks_per_us = SystemCoreClock / 1000000u;
        taskENTER_CRITICAL();
        *out = (w4e3_verification_sample_t){
            .wcrt_predicted_us = W4E1_RTA_TABLE[role].response_time_us,
            .wcrt_measured_us = (max_cycles[role] + clocks_per_us - 1u) / clocks_per_us,
            .cpu_load_percent = cpu[role],
            .stack_high_water_mark_words = hwm[role]
        };
        taskEXIT_CRITICAL();
    }
}

static void append_cycles_us(reply_writer_t *writer, uint32_t us)
{
    if (us > 9999u) {
        w4e3_append_text(writer, ">9k");
    } else {
        w4e3_append_number(writer, us);
    }
}

void w4e3_format_wcrt(reply_writer_t *writer, bool lo)
{
    /* Nominal response times documented alongside W4E1_RTA_TABLE. */
    static const uint16_t lo_us[W4E1_NUM_ROLES] = {70u, 235u, 415u, 835u, 1155u};
    w4e3_append_text(writer, lo ? "LO/M,%/w\n" : "HI/M,%/w\n");
    for (uint32_t role = 0u; role < W4E1_NUM_ROLES; ++role) {
        w4e3_verification_sample_t sample;
        fault_injection_get_verification_sample(role, &sample);
        w4e3_append_text(writer, w4e3_role_name(role));
        w4e3_append_text(writer, " ");
        w4e3_append_number(writer, lo ? lo_us[role] : sample.wcrt_predicted_us);
        w4e3_append_text(writer, "/");
        append_cycles_us(writer, sample.wcrt_measured_us);
        w4e3_append_text(writer, ",");
        w4e3_append_number(writer, sample.cpu_load_percent);
        w4e3_append_text(writer, "/");
        w4e3_append_number(writer, sample.stack_high_water_mark_words);
        w4e3_append_text(writer, "\n");
    }
    w4e3_append_text(writer, "E2E ");
    const uint32_t cycles_per_us = SystemCoreClock / 1000000u;
    append_cycles_us(writer, (imu_e2e_cycles + cycles_per_us - 1u) / cycles_per_us);
    w4e3_append_text(writer, "/");
    w4e3_append_number(writer, W4E1_DESIGN_IMU_E2E_US);
    w4e3_append_text(writer, "\n");
}

void vTaskGetRunTimeStatistics(char *pcWriteBuffer, size_t uxBufferLength)
{
    if (pcWriteBuffer == NULL || uxBufferLength == 0u) {
        return;
    }
    pcWriteBuffer[0] = '\0';
    reply_writer_t writer = { pcWriteBuffer, uxBufferLength, false };
    uint32_t total = 0u;
    uint8_t values[RUNTIME_TASKS] = {0};

    /* Only the active console calls this formatter. It hands off serially
     * between the shell and the HI-mode maintenance path. */
    handles[IDLE_COUNTER] = xTaskGetIdleTaskHandle();
    handles[TIMER_COUNTER] = xTimerGetTimerDaemonTaskHandle();
    const UBaseType_t count = uxTaskGetSystemState(s_status_array, RUNTIME_TASKS, &total);
    for (UBaseType_t i = 0u; i < count; ++i) {
        for (uint32_t task = 0u; task < RUNTIME_TASKS; ++task) {
            if (s_status_array[i].xHandle == handles[task]) {
                values[task] = (uint8_t)percent(s_status_array[i].ulRunTimeCounter, total);
                break;
            }
        }
    }
    w4e3_append_text(&writer, "RT% ctl/imu/tel/sh/aux/load/idle/tmr\r\n");
    for (uint32_t task = 0u; task < RUNTIME_TASKS; ++task) {
        if (task != 0u) {
            w4e3_append_text(&writer, "/");
        }
        w4e3_append_number(&writer, values[task]);
    }
    w4e3_append_text(&writer, " cpu=");
    w4e3_append_number(&writer, total == 0u ? 0u : 100u - values[IDLE_COUNTER]);
    w4e3_append_text(&writer, "\r\n");
}

void w4e3_format_runtime(reply_writer_t *writer)
{
    static char buffer[configSTATS_BUFFER_MAX_LENGTH];
    w4e3_wait_for_stats_window();
    vTaskGetRunTimeStats(buffer);
    w4e3_append_text(writer, buffer);
}
