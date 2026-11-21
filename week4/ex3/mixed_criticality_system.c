#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"
#include "timers.h"

#include "mixed_criticality_system.h"
#include "../ex2/include/params.h"
#include "w4e3_internal.h"

#include <stdatomic.h>
#include <string.h>

enum task_role {
    CONTROL = W4E1_ROLE_CONTROL, IMU = W4E1_ROLE_IMU, TELEMETRY = W4E1_ROLE_TELEMETRY,
    SHELL = W4E1_ROLE_SHELL, IDLE_AUX = W4E1_ROLE_IDLE_AUX
};
enum {
    IMU_RING_CAPACITY = 16u,
    HEARTBEAT_MASK = (1u << W4E2_NUM_TASKS) - 1u,
    HI_HEARTBEAT = (1u << CONTROL) | (1u << IMU) | (1u << IDLE_AUX),
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
static volatile uint32_t control_misses;
static uint32_t control_jitter_max_cycles, control_wcrt_max_cycles, jitter_window;
static volatile uint32_t control_release, imu_release, imu_release_runtime, control_sequence;
static volatile bool control_pending;
void *volatile w4e3_running_task;
static struct {
    uint8_t cpu_percent, heartbeat_missing, stack_low;
    bool ready;
} diagnostics;
static volatile float control_output;
static uint32_t imu_divider;
static volatile uint32_t control_epoch;
static volatile bool quiesced, hi_mode, shell_parked, leave_hi_pending;
static bool load_parked;
static TaskHandle_t load_task_handle;
static TaskHandle_t volatile stats_waiter;
static uint32_t stats_wait_cycles;
static StaticTask_t load_storage;
static StackType_t load_stack[128];
static bool shell_line_open;
static char command[SHELL_LINE_BYTES], reply[W4E3_UART_TX_BYTES];
static struct {
    TickType_t started, last_command, heartbeat_at, rx_window;
    uint32_t rx_bytes, uart_faults;
    size_t used;
    bool observe_after_ack;
} console;
static uint32_t telemetry_frame[20u];

static TaskHandle_t tasks[W4E2_NUM_TASKS];
static StaticTask_t task_storage[W4E2_NUM_TASKS], idle_storage, timer_storage;
static StackType_t idle_aux_stack[192], shell_stack[256], telemetry_stack[192];
static StackType_t imu_stack[192], control_stack[192];
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StackType_t timer_stack[configTIMER_TASK_STACK_DEPTH];
static StaticEventGroup_t heartbeat_storage;
static EventGroupHandle_t heartbeat;
/* Require a role for a complete watchdog window after it is resumed. */
static EventBits_t heartbeat_window_mask = HEARTBEAT_MASK;

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
    bool valid = false, degraded = false, cut_reported = false;
    float integral = 0.0f;
    uint32_t epoch = 0u;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        w4e3_fault_check_hang(CONTROL);
        taskENTER_CRITICAL();
        const uint32_t sequence = control_sequence;
        const uint32_t release = control_release;
        const uint32_t start = DWT->CYCCNT;
        const bool run = control_pending && !quiesced;
        taskEXIT_CRITICAL();
        if (!run) {
            continue;
        }
        GPIOA->BSRR = GPIO_BSRR_BS4;
        const uint32_t latency = start - release;
        if (epoch != control_epoch) {
            epoch = control_epoch;
            valid = false;
            degraded = false;
            cut_reported = false;
            integral = 0.0f;
        }
        valid = latest_imu(&sample) || valid;
        const bool cut = w4e3_imu_is_cut();
        const bool stale = !valid ||
            xTaskGetTickCount() - sample.sample_tick > W4E1_DESIGN_IMU_STALE_TICKS;
        const bool report = stale && (cut ? !cut_reported :
                                      !degraded && (valid || w4e3_imu_recoveries() != 0u));
        if (stale) {
            control_output = 0.0f;
            ++status.imu_stale_count;
        } else {
            const float error = -sample.accel_g[0];
            integral = clamp_control(integral + 0.001f * error);
            control_output = clamp_control(error + integral - 0.01f * sample.gyro_dps[1]);
        }
        cut_reported = cut && (cut_reported || report);
        degraded = stale && (degraded || report);
        (void)xEventGroupSetBits(heartbeat, 1u << CONTROL);
        if (report) {
            w4e3_fault_observe(W4E3_FAULT_SENSOR_UNPLUG);
        }
        GPIOA->BRR = GPIO_BRR_BR4;

        taskENTER_CRITICAL();
        const uint32_t response = DWT->CYCCNT - release;
        if (sequence == control_sequence) {
            control_pending = false;
            if (response >= SystemCoreClock / W4E1_CONTROL_LOOP_HZ) {
                ++control_misses;
            }
        }
        taskEXIT_CRITICAL();
        if (latency > control_jitter_max_cycles) {
            control_jitter_max_cycles = latency;
        }
        if (latency > jitter_window) {
            jitter_window = latency;
        }
        if (response > control_wcrt_max_cycles) {
            control_wcrt_max_cycles = response;
        }
        w4e3_measure(CONTROL, response);
    }
}

static void imu_task(void *arg)
{
    (void)arg;
    enum { IMU_ASLEEP, IMU_STARTING, IMU_READY } state = IMU_ASLEEP;
    TickType_t hold_since = 0u, hold_ticks = 0u;
    bool was_cut = false;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        taskENTER_CRITICAL();
        const uint32_t release = imu_release;
        const uint32_t release_runtime = imu_release_runtime;
        taskEXIT_CRITICAL();
        w4e3_fault_check_hang(IMU);
        const TickType_t now = xTaskGetTickCount();
        const bool cut = w4e3_imu_is_cut();
        if (!quiesced && (now - hold_since >= hold_ticks || cut != was_cut)) {
            const uint32_t epoch = control_epoch;
            bool ok;
            if (state == IMU_ASLEEP) {
                ok = w4e3_imu_wake();
                if (ok) {
                    state = IMU_STARTING;
                    hold_since = xTaskGetTickCount();
                    hold_ticks = pdMS_TO_TICKS(100u);
                }
            } else if (state == IMU_STARTING) {
                ok = w4e3_imu_configure();
                if (ok) {
                    state = IMU_READY;
                    hold_ticks = 0u;
                }
            } else {
                w4e2_imu_sample_t sample;
                ok = w4e3_imu_read(&sample);
                if (epoch == control_epoch) {
                    if (ok) {
                        publish_imu(&sample);
                    }
                    const uint32_t elapsed = DWT->CYCCNT - release;
                    const uint32_t bus_wait = w4e3_imu_bus_wait_cycles();
                    if (elapsed >= bus_wait) {
                        w4e3_measure(IMU, elapsed - bus_wait);
                    }
                    if (ok) {
                        /* TIM2 keeps counting when shallow sleep gates DWT.
                         * Use it when the clocks disagree beyond one TIM2 tick. */
                        const uint32_t cycles_per_rt = SystemCoreClock /
                            (1000000u / W4E3_RUNTIME_TICK_US);
                        const uint32_t wall = (w3e1_runtime_stats_now() - release_runtime) *
                                              cycles_per_rt;
                        w4e3_measure_imu_e2e(wall > elapsed && wall - elapsed > cycles_per_rt ?
                                             wall : elapsed);
                    }
                }
            }
            if (!ok) {
                state = IMU_ASLEEP;
                hold_since = xTaskGetTickCount();
                hold_ticks = pdMS_TO_TICKS(20u);
            }
        }
        was_cut = cut;
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
    telemetry_frame[3] = control_misses;
    telemetry_frame[4] = status.imu_stale_count;
    telemetry_frame[5] = control_jitter_max_cycles;
    telemetry_frame[6] = control_wcrt_max_cycles;
    telemetry_frame[7] = w4e3_uart_rx_faults();
    telemetry_frame[8] = w4e3_uart_tx_errors();
    telemetry_frame[9] = w4e3_imu_recoveries();
    telemetry_frame[10] = w4e3_imu_recovery_pulses();
    telemetry_frame[11] = w4e3_stop_entries();
    telemetry_frame[12] = w4e3_stop_faults();
    telemetry_frame[13] = ((uint32_t)diagnostics.stack_low << 16u) |
                         ((uint32_t)diagnostics.heartbeat_missing << 8u) |
                         diagnostics.cpu_percent;
    for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
        telemetry_frame[14u + role] = w4e3_stack_hwm(W4E1_NUM_ROLES - 1u - role);
    }
    taskEXIT_CRITICAL();
    telemetry_frame[19] = crc32((const uint8_t *)telemetry_frame,
                               sizeof telemetry_frame - sizeof telemetry_frame[0]);
    (void)w4e3_uart_send(telemetry_frame, sizeof telemetry_frame, 0u);
}

static void telemetry_task(void *arg)
{
    (void)arg;
    TickType_t last_sent = xTaskGetTickCount() - pdMS_TO_TICKS(TELEMETRY_MIN_MS);
    for (;;) {
        const uint32_t start = DWT->CYCCNT;
        w4e3_fault_check_hang(TELEMETRY);
        (void)xEventGroupSetBits(heartbeat, 1u << TELEMETRY);
        TickType_t wait = pdMS_TO_TICKS(SHELL_WAIT_MS);
        if (!quiesced) {
            const TickType_t now = xTaskGetTickCount();
            const uint32_t period_ms = status.telemetry_period_ms;
            const uint32_t effective_ms = w4e3_uart_fault_active() && period_ms < 100u ?
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
        w4e3_measure(TELEMETRY, DWT->CYCCNT - start);
        (void)ulTaskNotifyTake(pdTRUE, wait);
    }
}

static bool service_shell(TickType_t max_wait)
{
    const TickType_t timeout = pdMS_TO_TICKS(W4E2_SHELL_CMD_TIMEOUT_MS);
    const TickType_t window = pdMS_TO_TICKS(SHELL_WAIT_MS);
    const TickType_t before_receive = xTaskGetTickCount();

    if (before_receive - console.heartbeat_at >= window / 2u) {
        (void)xEventGroupSetBits(heartbeat, 1u << SHELL);
        console.heartbeat_at = before_receive;
    }
    const uint32_t uart_faults = w4e3_uart_rx_faults();
    if (uart_faults != console.uart_faults) {
        console.uart_faults = uart_faults;
        w4e3_fault_observe_uart(uart_faults, w4e3_uart_misses_at_cooldown());
    }
    if (w4e3_uart_cooling()) {
        shell_line_open = false;
        console.used = 0u;
        return false;
    }
    const TickType_t age = before_receive - console.started;
    if (shell_line_open && age >= timeout) {
        w4e3_uart_bad_frame();
        return false;
    }
    const TickType_t wait = shell_line_open && timeout - age < max_wait ?
        timeout - age : max_wait;
    uint8_t byte;
    if (w4e3_uart_receive(&byte, wait) == 0u || w4e3_uart_rx_faults() != uart_faults) {
        return false;
    }
    const uint32_t start_cycles = DWT->CYCCNT;
    const TickType_t now = xTaskGetTickCount();
    if (now - console.rx_window >= window) {
        console.rx_window = now;
        console.rx_bytes = 0u;
    }
    if (++console.rx_bytes > SHELL_BYTES_PER_WINDOW ||
        (shell_line_open && now - console.started >= timeout)) {
        w4e3_uart_bad_frame();
        return false;
    }
    if (byte == '\n' || byte == '\r') {
        shell_line_open = false;
        if (console.used == 0u) {
            return true;
        }
        command[console.used] = '\0';
        console.used = 0u;
        if (now - console.last_command < window) {
            w4e3_uart_bad_frame();
            return false;
        }

        console.last_command = now;
        stats_wait_cycles = 0u;
        const bool ok = mixed_criticality_system_shell_exec(command, reply, sizeof reply);
        const TickType_t spent = xTaskGetTickCount() - console.started;
        if (spent < timeout) {
            const char *message = ok ? reply : "ERR\r\n";
            const TickType_t room = timeout - spent;
            const TickType_t tx_wait = hi_mode ? 0u : pdMS_TO_TICKS(20u);
            (void)w4e3_uart_send(message, strlen(message), room < tx_wait ? room : tx_wait);
        }
        if (!hi_mode) {
            const uint32_t elapsed = DWT->CYCCNT - start_cycles;
            w4e3_measure(SHELL, elapsed > stats_wait_cycles ? elapsed - stats_wait_cycles : 0u);
        }
        if (console.observe_after_ack) {
            console.observe_after_ack = false;
            const TickType_t start = xTaskGetTickCount();
            do {
                /* The ACK is already queued. Keep publishing shell liveness
                 * while LO overload is still being measured. */
                (void)xEventGroupSetBits(heartbeat, 1u << SHELL);
            } while (!w4e3_fault_wait_step(pdMS_TO_TICKS(20u)) &&
                     xTaskGetTickCount() - start < pdMS_TO_TICKS(500u));
        }
    } else if (byte < ' ' || byte > '~' || console.used == sizeof command - 1u) {
        w4e3_uart_bad_frame();
    } else {
        if (!shell_line_open) {
            console.started = now;
            shell_line_open = true;
        }
        command[console.used++] = (char)byte;
    }
    return true;
}

static void shell_task(void *arg)
{
    (void)arg;
    for (;;) {
        w4e3_fault_check_hang(SHELL);
        if (hi_mode) {
            /* Park outside any stream-buffer receive or TX mutex. */
            shell_parked = true;
            vTaskSuspend(NULL);
            continue;
        }
        if (w4e3_uart_cooling()) {
            service_shell(0u);
            vTaskDelay(pdMS_TO_TICKS(5u));
        } else {
            service_shell(pdMS_TO_TICKS(10u));
        }
    }
}

static void load_task(void *arg)
{
    (void)arg;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        w4e3_fault_check_hang(W4E3_ROLE_LOAD);
        while (w4e3_fault_lo_load()) {
            /* An 8 ms burst with a 2-tick pause leaves the supervisor runnable. */
            const uint32_t started = DWT->CYCCNT;
            const uint32_t cycles = SystemCoreClock / 125u;
            while (DWT->CYCCNT - started < cycles && w4e3_fault_lo_load()) {
                __NOP();
            }
            vTaskDelay(pdMS_TO_TICKS(2u));
        }
    }
}

void w4e3_start_load(bool high)
{
    vTaskPrioritySet(load_task_handle, high ? 9u : 6u);
    (void)xTaskNotifyGive(load_task_handle);
}

void w4e3_wake_aux(void)
{
    (void)xTaskAbortDelay(tasks[IDLE_AUX]);
}

void w4e3_set_hi_mode(void)
{
    vTaskSuspendAll();
    if (!hi_mode) {
        hi_mode = true;
        heartbeat_window_mask &= HI_HEARTBEAT;
        vTaskSuspend(tasks[TELEMETRY]);
        if (w4e3_fault_lo_load()) {
            vTaskSuspend(load_task_handle);
            load_parked = true;
        }
    }
    (void)xTaskResumeAll();
    w4e3_fault_observe(W4E3_FAULT_CPU_OVERLOAD);
}

void w4e3_leave_hi_mode(void)
{
    if (hi_mode) {
        leave_hi_pending = true;
    }
}

static void finish_shell_handoff(void)
{
    vTaskSuspendAll();
    hi_mode = false;
    shell_parked = false;
    leave_hi_pending = false;
    if (load_parked) {
        load_parked = false;
        vTaskResume(load_task_handle);
    }
    vTaskResume(tasks[TELEMETRY]);
    vTaskResume(tasks[SHELL]);
    (void)xTaskResumeAll();
}

static void idle_aux_task(void *arg)
{
    (void)arg;
    TickType_t last_check = xTaskGetTickCount();
    (void)xEventGroupSetBits(heartbeat, 1u << IDLE_AUX);
    for (;;) {
        const TickType_t elapsed = xTaskGetTickCount() - last_check;
        const TickType_t period = pdMS_TO_TICKS(100u);
        const TickType_t time_to_check = elapsed < period ? period - elapsed : 0u;
        vTaskDelay(hi_mode && time_to_check > pdMS_TO_TICKS(5u) ?
                   pdMS_TO_TICKS(5u) : time_to_check);
        const uint32_t start = DWT->CYCCNT;
        w4e3_fault_check_hang(IDLE_AUX);
        const TickType_t now = xTaskGetTickCount();
        if (now - last_check >= pdMS_TO_TICKS(100u)) {
            last_check = now;
            taskENTER_CRITICAL();
            const EventBits_t observed = xEventGroupClearBits(heartbeat, HEARTBEAT_MASK);
            const EventBits_t expected = heartbeat_window_mask;
            heartbeat_window_mask = hi_mode ? HI_HEARTBEAT :
                (quiesced ? PAUSED_HEARTBEAT : HEARTBEAT_MASK);
            taskEXIT_CRITICAL();
            const uint8_t missing = (uint8_t)(expected & ~observed);
            w4e3_watchdog_check(missing);
            (void)xEventGroupSetBits(heartbeat, 1u << IDLE_AUX);

            const uint32_t cpu = w4e3_verification_collect();
            uint8_t low = 0u;
            for (uint32_t role = 0u; role < W4E1_NUM_ROLES; ++role) {
                if (w4e3_stack_hwm(role) < W4E2_STACK_MARGIN_TOLERANCE_WORDS) {
                    low |= 1u << (W4E1_NUM_ROLES - 1u - role);
                }
            }
            taskENTER_CRITICAL();
            const uint32_t jitter = jitter_window;
            jitter_window = 0u;
            diagnostics.cpu_percent = (uint8_t)cpu;
            diagnostics.stack_low = low;
            diagnostics.heartbeat_missing = 0u;
            for (uint32_t role = 0u; role < W4E1_NUM_ROLES; ++role) {
                diagnostics.heartbeat_missing |= ((missing >> role) & 1u) <<
                                                  (W4E1_NUM_ROLES - 1u - role);
            }
            diagnostics.ready = true;
            taskEXIT_CRITICAL();
            if (w4e3_system_running() && missing == 0u &&
                (cpu > W4E1_CPU_BUDGET_PERCENT_MAX ||
                 jitter > W4E1_DESIGN_JITTER_ALARM_US * (SystemCoreClock / 1000000u))) {
                w4e3_set_hi_mode();
            }
            w4e3_measure(IDLE_AUX, DWT->CYCCNT - start);
        }
        if (shell_parked) {
            /* Bounded maintenance receive while the LO shell stays suspended. */
            for (unsigned i = 0u; i < 32u && !leave_hi_pending; ++i) {
                if (!service_shell(0u)) {
                    break;
                }
            }
            if (leave_hi_pending) {
                finish_shell_handoff();
            }
        }
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

    console.last_command = 0u - pdMS_TO_TICKS(SHELL_WAIT_MS);
    console.heartbeat_at = console.last_command;
    console.rx_window = console.last_command;
    heartbeat = xEventGroupCreateStatic(&heartbeat_storage);
    configASSERT(heartbeat != NULL);
    w4e3_imu_init();
    w4e3_uart_init();
    for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
        const UBaseType_t priority = W4E2_TASK_PRIORITY[W4E1_NUM_ROLES - 1u - role];
        configASSERT(priority == W4E1_RTA_TABLE[role].priority);
        tasks[role] = xTaskCreateStatic(config[role].run, config[role].name,
                                       config[role].words, NULL, priority,
                                       config[role].stack, &task_storage[role]);
        configASSERT(tasks[role] != NULL);
    }
    load_task_handle = xTaskCreateStatic(load_task, "load", 128u, NULL, 6u,
                                         load_stack, &load_storage);
    configASSERT(load_task_handle != NULL);
    w4e3_verification_init(tasks, load_task_handle);
}

void w4e3_control_release_from_isr(uint32_t release, uint32_t release_runtime,
                                   uint32_t skipped, const uint32_t *frame, uint32_t exc_return)
{
    BaseType_t woken = pdFALSE;
    control_misses += skipped + (control_pending ? 1u : 0u);
    control_release = release;
    control_pending = true;
    ++control_sequence;
    vTaskNotifyGiveFromISR(tasks[CONTROL], &woken);
    imu_divider ^= 1u;
    if (imu_divider == 0u) {
        imu_release = release;
        imu_release_runtime = release_runtime;
        vTaskNotifyGiveFromISR(tasks[IMU], &woken);
    }
    if (stats_waiter != NULL) {
        vTaskNotifyGiveIndexedFromISR(stats_waiter, W4E3_STATS_NOTIFY_INDEX, &woken);
        stats_waiter = NULL;
    }
    w4e3_fault_timer(frame, exc_return, &woken);
    portYIELD_FROM_ISR(woken);
}

void w4e3_wait_for_stats_window(void)
{
    if (quiesced) {
        return;
    }
    /* uxTaskGetSystemState holds the scheduler while scanning the stacks.
     * Start after a fresh control release to leave time before the next one. */
    const uint32_t wait_start = DWT->CYCCNT;
    (void)ulTaskNotifyTakeIndexed(W4E3_STATS_NOTIFY_INDEX, pdTRUE, 0u);
    taskENTER_CRITICAL();
    stats_waiter = xTaskGetCurrentTaskHandle();
    taskEXIT_CRITICAL();
    (void)ulTaskNotifyTakeIndexed(W4E3_STATS_NOTIFY_INDEX, pdTRUE, pdMS_TO_TICKS(2u));
    taskENTER_CRITICAL();
    stats_waiter = NULL;
    taskEXIT_CRITICAL();
    stats_wait_cycles += DWT->CYCCNT - wait_start;
}

uint32_t w4e3_control_misses(void)
{
    return control_misses;
}

uint32_t w4e3_current_role(void)
{
    const void *task = w4e3_running_task;
    for (uint32_t role = 0u; task != NULL && role < W4E1_NUM_ROLES; ++role) {
        if (task == tasks[role]) {
            return role;
        }
    }
    return task != NULL && task == load_task_handle ? W4E3_ROLE_LOAD : W4E3_ROLE_UNKNOWN;
}

void mixed_criticality_system_get_status(w4e2_system_status_t *out)
{
    if (out == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    *out = status;
    out->control_overrun_count = control_misses;
    taskEXIT_CRITICAL();
}

bool w4e3_system_allows_stop(void)
{
    return quiesced && !hi_mode && !shell_line_open && w4e3_fault_allows_stop();
}

bool w4e3_system_running(void)
{
    return !quiesced && !hi_mode;
}

static void format_status(reply_writer_t *writer)
{
    w4e2_system_status_t current;
    uint32_t jitter, wcrt;
    taskENTER_CRITICAL();
    current = status;
    current.control_overrun_count = control_misses;
    jitter = control_jitter_max_cycles;
    wcrt = control_wcrt_max_cycles;
    taskEXIT_CRITICAL();
    const uint32_t cycles_per_us = SystemCoreClock / 1000000u;
    w4e3_append_text(writer, "period_ms=");
    w4e3_append_number(writer, current.telemetry_period_ms);
    w4e3_append_text(writer, " overrun=");
    w4e3_append_number(writer, current.control_overrun_count);
    w4e3_append_text(writer, " stale=");
    w4e3_append_number(writer, current.imu_stale_count);
    w4e3_append_text(writer, " jitter_us=");
    w4e3_append_number(writer, jitter / cycles_per_us);
    w4e3_append_text(writer, " wcrt_us=");
    w4e3_append_number(writer, wcrt / cycles_per_us);
    w4e3_append_text(writer, " stop=");
    w4e3_append_number(writer, w4e3_stop_entries());
    w4e3_append_text(writer, hi_mode ? " HI\r\n" : " LO\r\n");
}

static void format_runtime_stats(reply_writer_t *writer)
{
    /* Keep the W4.2 stats layout and report the last 100 ms window. */
    if (!diagnostics.ready) {
        w4e3_append_text(writer, "stats warming up\r\n");
        return;
    }
    w4e3_append_text(writer, "aux/sh/tel/imu/ctl\r\nHWMw=");
    for (uint32_t role = 0u; role < W4E2_NUM_TASKS; ++role) {
        if (role != 0u) {
            w4e3_append_text(writer, "/");
        }
        w4e3_append_number(writer, w4e3_stack_hwm(W4E1_NUM_ROLES - 1u - role));
    }
    w4e3_append_text(writer, "\r\nRT%(+idle/tmr)=");
    for (uint32_t i = 0u; i < W4E2_NUM_TASKS + 2u; ++i) {
        if (i != 0u) {
            w4e3_append_text(writer, "/");
        }
        w4e3_append_number(writer, w4e3_runtime_percent(i < W4E1_NUM_ROLES ? W4E1_NUM_ROLES - 1u - i : i));
    }
    w4e3_append_text(writer, " cpu=");
    w4e3_append_number(writer, diagnostics.cpu_percent);
    w4e3_append_text(writer, " hb_miss=");
    w4e3_append_number(writer, diagnostics.heartbeat_missing);
    w4e3_append_text(writer, " low=");
    w4e3_append_number(writer, diagnostics.stack_low);
    w4e3_append_text(writer, "\r\n");
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
            w4e3_control_clock_stop();
            control_pending = false;
            heartbeat_window_mask &= PAUSED_HEARTBEAT;
            ++control_epoch;
            atomic_store_explicit(&imu_tail, atomic_load_explicit(&imu_head, memory_order_acquire),
                                  memory_order_release);
            control_output = 0.0f;
        } else {
            imu_divider = 0u;
            w4e3_control_clock_start();
        }
        quiesced = pause;
    }
    taskEXIT_CRITICAL();
    (void)xTaskNotifyGive(tasks[TELEMETRY]);
}

static bool fault_command(const char *command, reply_writer_t *writer)
{
    static const struct {
        const char *command;
        w4e3_fault_kind_t kind;
        uint8_t role;
        bool high;
    } faults[] = {
        { "sensor_unplug", W4E3_FAULT_SENSOR_UNPLUG, W4E3_ROLE_UNKNOWN, false },
        { "uart_flood", W4E3_FAULT_UART_FLOOD, W4E3_ROLE_UNKNOWN, false },
        { "cpu_overload", W4E3_FAULT_CPU_OVERLOAD, W4E3_ROLE_LOAD, false },
        { "cpu_overload_hi", W4E3_FAULT_CPU_OVERLOAD, W4E3_ROLE_LOAD, true },
        { "task_hang", W4E3_FAULT_TASK_HANG, TELEMETRY, false }
    };
    for (unsigned i = 0u; i < sizeof faults / sizeof faults[0]; ++i) {
        if (strcmp(command, faults[i].command) == 0) {
            const bool armed = w4e3_fault_request(faults[i].kind, faults[i].role, faults[i].high);
            if (armed && w4e3_current_role() == SHELL) {
                console.observe_after_ack = true;
            }
            w4e3_append_text(writer, armed ? "OK\r\n" : "BUSY\r\n");
            return true;
        }
    }
    if (strncmp(command, "task_hang ", 10u) == 0) {
        static const char *const full_names[] = { "control", "imu", "telemetry", "shell", "idle_aux" };
        for (uint32_t role = 0u; role < W4E1_NUM_ROLES; ++role) {
            if (strcmp(command + 10u, w4e3_role_name(role)) == 0 ||
                strcmp(command + 10u, full_names[role]) == 0) {
                const bool armed = w4e3_fault_request(W4E3_FAULT_TASK_HANG, role, false);
                if (armed && role != SHELL && w4e3_current_role() == SHELL) {
                    console.observe_after_ack = true;
                }
                w4e3_append_text(writer, armed ? "OK\r\n" : "BUSY\r\n");
                return true;
            }
        }
    }
    return false;
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
    if (strcmp(cmd_line, "wcrt") == 0 || strcmp(cmd_line, "wcrt lo") == 0) {
        w4e3_format_wcrt(&writer, cmd_line[4] != '\0');
    } else if (strcmp(cmd_line, "runtime") == 0) {
        w4e3_format_runtime(&writer);
    } else if (strcmp(cmd_line, "fault result") == 0 || strcmp(cmd_line, "postmortem") == 0) {
        w4e3_format_fault(&writer);
    } else if (strcmp(cmd_line, "fault clear") == 0 && len >= sizeof("BUSY\r\n")) {
        w4e3_append_text(&writer, w4e3_fault_clear() ? "OK\r\n" : "BUSY\r\n");
    } else if (strncmp(cmd_line, "fault ", 6u) == 0 && len >= sizeof("BUSY\r\n")) {
        if (!fault_command(cmd_line + 6u, &writer)) {
            return false;
        }
    } else if (strcmp(cmd_line, "status") == 0) {
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
        } else if (!hi_mode && w4e3_fault_allows_stop() && strcmp(cmd_line, "quiesce on") == 0) {
            set_quiesce(true);
        } else if (strcmp(cmd_line, "quiesce off") == 0) {
            set_quiesce(false);
        } else {
            return false;
        }
        w4e3_append_text(&writer, "OK\r\n");
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
