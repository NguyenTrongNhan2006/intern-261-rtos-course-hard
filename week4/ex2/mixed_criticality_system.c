#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"
#include "timers.h"

#include "mixed_criticality_system.h"
#include "params.h"
#include "../ex1/include/mixed_criticality_design.h"
#include "w4e2_internal.h"

#include <stdatomic.h>
#include <string.h>

enum task_role { IDLE_AUX, SHELL, TELEMETRY, IMU, CONTROL };
enum {
    IMU_RING_CAPACITY = 16u,
    HEARTBEAT_MASK = (1u << W4E2_NUM_TASKS) - 1u,
    PAUSED_HEARTBEAT = (1u << SHELL) | (1u << IDLE_AUX),
    TELEMETRY_MIN_MS = 10u, TELEMETRY_MAX_MS = 1000u,
    SHELL_WAIT_MS = 50u, SHELL_LINE_BYTES = 128u, SHELL_BYTES_PER_WINDOW = 256u
};

typedef struct {
    w4e2_imu_sample_t sample;
    uint32_t sequence;
} imu_slot_t;

_Static_assert(W4E2_NUM_TASKS == 5, "W4.2 requires five tasks");
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "IMU indices must be lock-free");
_Static_assert(sizeof(imu_slot_t) == 32u, "W4.1 IMU slot budget");

static imu_slot_t imu_ring[IMU_RING_CAPACITY];
static _Atomic uint32_t imu_head, imu_tail;
static w4e2_system_status_t status = { .telemetry_period_ms = 10u };
static uint32_t control_jitter_max_cycles, control_wcrt_max_cycles;
static struct {
    uint16_t hwm[W4E2_NUM_TASKS];
    uint8_t runtime_percent[W4E2_NUM_TASKS + 2u]; /* Five roles, kernel idle, timer. */
    uint8_t cpu_percent, heartbeat_missing, stack_low;
    bool ready;
} diagnostics;
static volatile float control_output;
static uint32_t imu_divider, control_releases;
static volatile uint32_t control_epoch;
static volatile bool quiesced;
static bool shell_line_open;
static char command[SHELL_LINE_BYTES], reply[W4E2_UART_TX_BYTES];
static uint32_t telemetry_frame[20u];

static TaskHandle_t tasks[W4E2_NUM_TASKS];
static StaticTask_t task_storage[W4E2_NUM_TASKS], idle_storage, timer_storage;
static StackType_t idle_aux_stack[192], shell_stack[256], telemetry_stack[192];
static StackType_t imu_stack[192], control_stack[192];
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StackType_t timer_stack[configTIMER_TASK_STACK_DEPTH];
static StaticEventGroup_t heartbeat_storage;
static EventGroupHandle_t heartbeat;

static void publish_imu(const w4e2_imu_sample_t *sample)
{
    const uint32_t head = atomic_load_explicit(&imu_head, memory_order_relaxed);
    const uint32_t tail = atomic_load_explicit(&imu_tail, memory_order_acquire);
    if (head - tail < IMU_RING_CAPACITY) {
        imu_ring[head & (IMU_RING_CAPACITY - 1u)] = (imu_slot_t){ *sample, head };
        /* ARM acquire/release supplies the DMB ordering from W2.2. */
        atomic_store_explicit(&imu_head, head + 1u, memory_order_release);
    }
}

static bool latest_imu(w4e2_imu_sample_t *sample)
{
    const uint32_t tail = atomic_load_explicit(&imu_tail, memory_order_relaxed);
    const uint32_t head = atomic_load_explicit(&imu_head, memory_order_acquire);
    if (head == tail) {
        return false;
    }
    const imu_slot_t *slot = &imu_ring[(head - 1u) & (IMU_RING_CAPACITY - 1u)];
    configASSERT(slot->sequence == head - 1u);
    *sample = slot->sample;
    atomic_store_explicit(&imu_tail, head, memory_order_release);
    return true;
}

static float clamp_control(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void control_task(void *arg)
{
    (void)arg;
    w4e2_imu_sample_t sample;
    bool valid = false;
    float integral = 0.0f;
    uint32_t epoch = 0u;
    uint32_t handled_release = 0u;
    uint32_t counted_misses = 0u;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        taskENTER_CRITICAL();
        const uint32_t sequence = control_releases;
        const uint32_t release = w4e2_control_release_cycles();
        const uint32_t missed = w4e2_control_missed_releases();
        const uint32_t start = DWT->CYCCNT;
        taskEXIT_CRITICAL();
        if (sequence == handled_release) {
            continue;
        }
        GPIOA->BSRR = GPIO_BSRR_BS4;
        const uint32_t latency = start - release;
        if (epoch != control_epoch) {
            epoch = control_epoch;
            valid = false;
            integral = 0.0f;
        }
        valid = latest_imu(&sample) || valid;
        if (!valid || xTaskGetTickCount() - sample.sample_tick > W4E1_DESIGN_IMU_STALE_TICKS) {
            control_output = 0.0f;
            ++status.imu_stale_count;
        } else {
            const float error = -sample.accel_g[0];
            integral = clamp_control(integral + 0.001f * error);
            control_output = clamp_control(error + integral - 0.01f * sample.gyro_dps[1]);
        }
        (void)xEventGroupSetBits(heartbeat, 1u << CONTROL);
        GPIOA->BRR = GPIO_BRR_BR4;
        const uint32_t response = DWT->CYCCNT - release;
        const uint32_t period = SystemCoreClock / W4E1_CONTROL_LOOP_HZ;
        status.control_overrun_count += sequence - handled_release - 1u + missed - counted_misses;
        handled_release = sequence;
        counted_misses = missed;
        if (response >= period) {
            ++status.control_overrun_count;
        }
        if (latency > control_jitter_max_cycles) {
            control_jitter_max_cycles = latency;
        }
        if (response > control_wcrt_max_cycles) {
            control_wcrt_max_cycles = response;
        }
    }
}

static void imu_task(void *arg)
{
    (void)arg;
    enum { IMU_ASLEEP, IMU_STARTING, IMU_READY } state = IMU_ASLEEP;
    TickType_t hold_since = 0u, hold_ticks = 0u;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const TickType_t now = xTaskGetTickCount();
        if (!quiesced && now - hold_since >= hold_ticks) {
            const uint32_t epoch = control_epoch;
            bool ok;
            if (state == IMU_ASLEEP) {
                ok = w4e2_imu_wake();
                if (ok) {
                    state = IMU_STARTING;
                    hold_since = xTaskGetTickCount();
                    hold_ticks = pdMS_TO_TICKS(100u);
                }
            } else if (state == IMU_STARTING) {
                ok = w4e2_imu_configure();
                if (ok) {
                    state = IMU_READY;
                    hold_ticks = 0u;
                }
            } else {
                w4e2_imu_sample_t sample;
                ok = w4e2_imu_read(&sample);
                if (ok && epoch == control_epoch) {
                    publish_imu(&sample);
                }
            }
            if (!ok) {
                state = IMU_ASLEEP;
                hold_since = xTaskGetTickCount();
                hold_ticks = pdMS_TO_TICKS(20u);
            }
        }
        (void)xEventGroupSetBits(heartbeat, 1u << IMU);
    }
}

static uint32_t crc32(const uint8_t *data, size_t len)
{
    /* Reflected IEEE CRC-32, as in W3.1. */
    static const uint32_t nibble[16] = {
        0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
        0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
        0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
        0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu
    };
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0u; i < len; ++i) {
        crc ^= data[i];
        crc = (crc >> 4u) ^ nibble[crc & 15u];
        crc = (crc >> 4u) ^ nibble[crc & 15u];
    }
    return ~crc;
}

static void send_telemetry(TickType_t now)
{
    /* 20 little-endian words, magic W4E2, followed by CRC32 over words 0-18. */
    taskENTER_CRITICAL();
    telemetry_frame[0] = 0x32453457u;
    telemetry_frame[1] = now;
    telemetry_frame[2] = status.telemetry_period_ms;
    telemetry_frame[3] = status.control_overrun_count;
    telemetry_frame[4] = status.imu_stale_count;
    telemetry_frame[5] = control_jitter_max_cycles;
    telemetry_frame[6] = control_wcrt_max_cycles;
    telemetry_frame[7] = w4e2_uart_rx_faults();
    telemetry_frame[8] = w4e2_uart_tx_errors();
    telemetry_frame[9] = w4e2_imu_recoveries();
    telemetry_frame[10] = w4e2_imu_recovery_pulses();
    telemetry_frame[11] = w4e2_stop_entries();
    telemetry_frame[12] = w4e2_stop_faults();
    telemetry_frame[13] = ((uint32_t)diagnostics.stack_low << 16u) |
                         ((uint32_t)diagnostics.heartbeat_missing << 8u) |
                         diagnostics.cpu_percent;
    for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
        telemetry_frame[14u + role] = diagnostics.hwm[role];
    }
    taskEXIT_CRITICAL();
    telemetry_frame[19] = crc32((const uint8_t *)telemetry_frame,
                               sizeof telemetry_frame - sizeof telemetry_frame[0]);
    (void)w4e2_uart_send(telemetry_frame, sizeof telemetry_frame, 0u);
}

static void telemetry_task(void *arg)
{
    (void)arg;
    TickType_t last_sent = xTaskGetTickCount() - pdMS_TO_TICKS(TELEMETRY_MIN_MS);
    for (;;) {
        (void)xEventGroupSetBits(heartbeat, 1u << TELEMETRY);
        TickType_t wait = pdMS_TO_TICKS(SHELL_WAIT_MS);
        if (!quiesced) {
            const TickType_t now = xTaskGetTickCount();
            const uint32_t period_ms = status.telemetry_period_ms;
            const uint32_t effective_ms = w4e2_uart_fault_active() && period_ms < 100u ?
                100u : period_ms;
            const TickType_t period = pdMS_TO_TICKS(effective_ms);
            const TickType_t elapsed = now - last_sent;
            if (elapsed >= period) {
                send_telemetry(now);
                last_sent = now;
            }
            const TickType_t remaining = elapsed >= period ? period : period - elapsed;
            wait = remaining < wait ? remaining : wait;
        }
        (void)ulTaskNotifyTake(pdTRUE, wait);
    }
}

static void shell_task(void *arg)
{
    (void)arg;
    const TickType_t timeout = pdMS_TO_TICKS(W4E2_SHELL_CMD_TIMEOUT_MS);
    const TickType_t max_wait = pdMS_TO_TICKS(SHELL_WAIT_MS);
    TickType_t started = 0u, last_command = xTaskGetTickCount() - max_wait;
    TickType_t heartbeat_at = last_command, rx_window = last_command;
    uint32_t rx_bytes = 0u;
    size_t used = 0u;
    for (;;) {
        const TickType_t before_receive = xTaskGetTickCount();
        if (before_receive - heartbeat_at >= max_wait / 2u) {
            (void)xEventGroupSetBits(heartbeat, 1u << SHELL);
            heartbeat_at = before_receive;
        }
        if (w4e2_uart_cooling()) {
            shell_line_open = false;
            used = 0u;
            vTaskDelay(pdMS_TO_TICKS(5u));
            continue;
        }

        const TickType_t age = before_receive - started;
        if (shell_line_open && age >= timeout) {
            w4e2_uart_bad_frame();
            continue;
        }
        const TickType_t wait = shell_line_open && timeout - age < max_wait ?
            timeout - age : max_wait;
        uint8_t byte;
        if (w4e2_uart_receive(&byte, wait) == 0u || w4e2_uart_fault_active()) {
            continue;
        }
        const TickType_t now = xTaskGetTickCount();
        if (now - rx_window >= max_wait) {
            rx_window = now;
            rx_bytes = 0u;
        }
        if (++rx_bytes > SHELL_BYTES_PER_WINDOW ||
            (shell_line_open && now - started >= timeout)) {
            w4e2_uart_bad_frame();
            continue;
        }
        if (byte == '\n' || byte == '\r') {
            shell_line_open = false;
            if (used != 0u) {
                command[used] = '\0';
                used = 0u;
                if (now - last_command < max_wait) {
                    w4e2_uart_bad_frame();
                    continue;
                }
                last_command = now;
                const bool ok = mixed_criticality_system_shell_exec(command, reply, sizeof reply);
                const TickType_t spent = xTaskGetTickCount() - started;
                if (spent < timeout) {
                    const char *message = ok ? reply : "ERR\r\n";
                    const TickType_t room = timeout - spent;
                    const TickType_t tx_wait = pdMS_TO_TICKS(20u);
                    (void)w4e2_uart_send(message, strlen(message), room < tx_wait ? room : tx_wait);
                }
            }
        } else if (byte < ' ' || byte > '~' || used == sizeof command - 1u) {
            w4e2_uart_bad_frame();
        } else {
            if (!shell_line_open) {
                started = now;
                shell_line_open = true;
            }
            command[used++] = (char)byte;
        }
    }
}

static void idle_aux_task(void *arg)
{
    (void)arg;
    uint32_t previous_runtime[W4E2_NUM_TASKS + 2u] = {0};
    uint32_t previous_total = 0u;
    uint32_t epoch = control_epoch;
    for (;;) {
        (void)xEventGroupSetBits(heartbeat, 1u << IDLE_AUX);
        vTaskDelay(pdMS_TO_TICKS(100u));
        const EventBits_t observed = xEventGroupClearBits(heartbeat, HEARTBEAT_MASK);
        const EventBits_t expected = quiesced || epoch != control_epoch ?
            PAUSED_HEARTBEAT : HEARTBEAT_MASK;
        epoch = control_epoch;
        uint32_t counters[W4E2_NUM_TASKS + 2u];
        uint16_t hwm[W4E2_NUM_TASKS];
        uint8_t low = 0u;
        /* Read individual per-task stats outside scheduler suspension. */
        taskENTER_CRITICAL();
        const uint32_t total = w3e1_runtime_stats_now();
        for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
            counters[role] = ulTaskGetRunTimeCounter(tasks[role]);
        }
        counters[W4E2_NUM_TASKS] = ulTaskGetIdleRunTimeCounter();
        counters[W4E2_NUM_TASKS + 1u] = ulTaskGetRunTimeCounter(xTimerGetTimerDaemonTaskHandle());
        taskEXIT_CRITICAL();
        for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
            hwm[role] = (uint16_t)uxTaskGetStackHighWaterMark(tasks[role]);
            if (hwm[role] < W4E2_STACK_MARGIN_TOLERANCE_WORDS) {
                low |= 1u << role;
            }
        }
        const uint32_t window = total - previous_total;
        uint8_t percentages[W4E2_NUM_TASKS + 2u];
        for (uint32_t i = 0u; i < W4E2_NUM_TASKS + 2u; ++i) {
            const uint32_t delta = counters[i] - previous_runtime[i];
            percentages[i] = (uint8_t)(window == 0u ? 0u : delta >= window ? 100u :
                ((uint64_t)delta * 100u) / window);
            previous_runtime[i] = counters[i];
        }
        previous_total = total;
        taskENTER_CRITICAL();
        memcpy(diagnostics.hwm, hwm, sizeof hwm);
        memcpy(diagnostics.runtime_percent, percentages, sizeof percentages);
        diagnostics.cpu_percent = window == 0u ? 0u : 100u - percentages[W4E2_NUM_TASKS];
        diagnostics.heartbeat_missing = (uint8_t)(expected & ~observed);
        diagnostics.stack_low = low;
        diagnostics.ready = window != 0u;
        taskEXIT_CRITICAL();
    }
}

void mixed_criticality_system_init(void)
{
    static const struct {
        const char *name;
        TaskFunction_t run;
        StackType_t *stack;
        uint32_t words;
    } config[] = {
        [IDLE_AUX]  = { "idle_aux", idle_aux_task, idle_aux_stack, 192u },
        [SHELL]     = { "shell", shell_task, shell_stack, 256u },
        [TELEMETRY] = { "telemetry", telemetry_task, telemetry_stack, 192u },
        [IMU]       = { "imu", imu_task, imu_stack, 192u },
        [CONTROL]   = { "control", control_task, control_stack, 192u }
    };

    heartbeat = xEventGroupCreateStatic(&heartbeat_storage);
    configASSERT(heartbeat != NULL);
    w4e2_imu_init();
    w4e2_uart_init();
    for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
        const UBaseType_t priority = W4E2_TASK_PRIORITY[role];
        configASSERT(priority == W4E1_RTA_TABLE[CONTROL - role].priority);
        tasks[role] = xTaskCreateStatic(config[role].run, config[role].name,
                                       config[role].words, NULL, priority,
                                       config[role].stack, &task_storage[role]);
        configASSERT(tasks[role] != NULL);
    }
}

void mixed_criticality_system_on_control_tick(void)
{
    BaseType_t woken = pdFALSE;
    ++control_releases;
    vTaskNotifyGiveFromISR(tasks[CONTROL], &woken);
    imu_divider ^= 1u;
    if (imu_divider == 0u) {
        vTaskNotifyGiveFromISR(tasks[IMU], &woken);
    }
    portYIELD_FROM_ISR(woken);
}

void mixed_criticality_system_get_status(w4e2_system_status_t *out)
{
    if (out == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    *out = status;
    taskEXIT_CRITICAL();
}

bool w4e2_system_allows_stop(void)
{
    return quiesced && !shell_line_open;
}

typedef struct {
    char *next;
    size_t room;
    bool full;
} reply_writer_t;

static void append_text(reply_writer_t *writer, const char *text)
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

static void append_number(reply_writer_t *writer, uint32_t value)
{
    char digits[11];
    char *first = digits + sizeof digits - 1u;
    *first = '\0';
    do {
        *--first = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    append_text(writer, first);
}

static void format_status(reply_writer_t *writer)
{
    w4e2_system_status_t current;
    uint32_t jitter, wcrt;
    taskENTER_CRITICAL();
    current = status;
    jitter = control_jitter_max_cycles;
    wcrt = control_wcrt_max_cycles;
    taskEXIT_CRITICAL();
    const uint32_t cycles_per_us = SystemCoreClock / 1000000u;
    append_text(writer, "period_ms=");
    append_number(writer, current.telemetry_period_ms);
    append_text(writer, " overrun=");
    append_number(writer, current.control_overrun_count);
    append_text(writer, " stale=");
    append_number(writer, current.imu_stale_count);
    append_text(writer, " jitter_us=");
    append_number(writer, jitter / cycles_per_us);
    append_text(writer, " wcrt_us=");
    append_number(writer, wcrt / cycles_per_us);
    append_text(writer, " stop=");
    append_number(writer, w4e2_stop_entries());
    append_text(writer, "\r\n");
}

static void format_runtime_stats(reply_writer_t *writer)
{
    /* Static counterpart to vTaskGetRunTimeStats using cached kernel counters. */
    if (!diagnostics.ready) {
        append_text(writer, "stats warming up\r\n");
        return;
    }
    append_text(writer, "aux/sh/tel/imu/ctl\r\nHWMw=");
    for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
        if (role != 0u) {
            append_text(writer, "/");
        }
        append_number(writer, diagnostics.hwm[role]);
    }
    append_text(writer, "\r\nRT%(+idle/tmr)=");
    for (uint32_t i = 0u; i < W4E2_NUM_TASKS + 2u; ++i) {
        if (i != 0u) {
            append_text(writer, "/");
        }
        append_number(writer, diagnostics.runtime_percent[i]);
    }
    append_text(writer, " cpu=");
    append_number(writer, diagnostics.cpu_percent);
    append_text(writer, " hb_miss=");
    append_number(writer, diagnostics.heartbeat_missing);
    append_text(writer, " low=");
    append_number(writer, diagnostics.stack_low);
    append_text(writer, "\r\n");
}

static bool parse_period(const char *text, uint32_t *period)
{
    uint32_t value = 0u;
    if (*text == '\0') {
        return false;
    }
    for (uint32_t digits = 0u; *text != '\0'; ++digits, ++text) {
        if (digits == 4u || *text < '0' || *text > '9') {
            return false;
        }
        value = value * 10u + (uint32_t)(*text - '0');
    }
    *period = value;
    return value >= TELEMETRY_MIN_MS && value <= TELEMETRY_MAX_MS;
}

static void set_quiesce(bool pause)
{
    taskENTER_CRITICAL();
    if (quiesced != pause) {
        if (pause) {
            w4e2_control_clock_stop();
            ++control_epoch;
            atomic_store_explicit(&imu_tail, atomic_load_explicit(&imu_head, memory_order_acquire),
                                  memory_order_release);
            control_output = 0.0f;
        } else {
            imu_divider = 0u;
            w4e2_control_clock_start();
        }
        quiesced = pause;
    }
    taskEXIT_CRITICAL();
    (void)xTaskNotifyGive(tasks[TELEMETRY]);
}

bool mixed_criticality_system_shell_exec(const char *cmd_line, char *reply, uint32_t len)
{
    if (reply == NULL || len == 0u) {
        return false;
    }
    reply[0] = '\0';
    if (cmd_line == NULL) {
        return false;
    }
    reply_writer_t writer = { reply, len, false };
    static const char prefix[] = "set telemetry ";
    if (strcmp(cmd_line, "status") == 0) {
        format_status(&writer);
    } else if (strcmp(cmd_line, "stats") == 0) {
        format_runtime_stats(&writer);
    } else if (len >= sizeof("OK\r\n")) {
        uint32_t period;
        if (strncmp(cmd_line, prefix, sizeof prefix - 1u) == 0 &&
            parse_period(cmd_line + sizeof prefix - 1u, &period)) {
            taskENTER_CRITICAL();
            status.telemetry_period_ms = period;
            taskEXIT_CRITICAL();
            (void)xTaskNotifyGive(tasks[TELEMETRY]);
        } else if (strcmp(cmd_line, "quiesce on") == 0) {
            set_quiesce(true);
        } else if (strcmp(cmd_line, "quiesce off") == 0) {
            set_quiesce(false);
        } else {
            return false;
        }
        append_text(&writer, "OK\r\n");
    } else {
        return false;
    }
    if (writer.full) {
        reply[0] = '\0';
    }
    return !writer.full;
}

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *words)
{
    *tcb = &idle_storage;
    *stack = idle_stack;
    *words = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                    configSTACK_DEPTH_TYPE *words)
{
    *tcb = &timer_storage;
    *stack = timer_stack;
    *words = configTIMER_TASK_STACK_DEPTH;
}
