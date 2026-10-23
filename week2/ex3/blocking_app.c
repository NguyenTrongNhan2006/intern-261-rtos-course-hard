#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "params.h"
#include "priority_ceiling.h"
#include "tracer.h"
#include "uart.h"

/* STEP 1: Bộ nhớ static và dữ liệu đo. */
enum { TASK_L, TASK_M1, TASK_M2, TASK_H };
typedef enum { LOCK_A, LOCK_B, NO_LOCK } resource_t;

#define STACK_WORDS 256u

static TaskHandle_t s_tasks[W2E3_NUM_TASKS];
static StaticTask_t s_tcbs[W2E3_NUM_TASKS];
_Alignas(8) static StackType_t
    s_stacks[W2E3_NUM_TASKS][STACK_WORDS];

static StaticTask_t s_idle_tcb;
_Alignas(8) static StackType_t
    s_idle_stack[configMINIMAL_STACK_SIZE];

static SemaphoreHandle_t s_lock_a, s_lock_b;
static StaticSemaphore_t s_storage_a, s_storage_b;
static pc_mutex_t s_ceiling_a, s_ceiling_b;

/* H/L ghi dữ liệu; M1 chỉ đọc sau cửa sổ thử nghiệm. */
static volatile uint32_t s_h_take_cycles[2], s_h_take_mask;
static volatile TickType_t s_h_ready_ticks;
static volatile uint32_t s_h_done, s_l_waiting_b;
static volatile resource_t s_h_pending = NO_LOCK;

/* STEP 2: Bộ nhớ Idle Task và hook tràn stack. */
void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &s_idle_tcb;
    *stack = s_idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;

    __disable_irq();
    for (;;) {
        __NOP();
    }
}

/* STEP 3: Chọn một kịch bản cho mỗi lần build. */
enum {
    INVERSION,
    CHAINED_BLOCKING,
    DEADLOCK,
    CEILING_INVERSION,
    CEILING_CHAINED,
    CEILING_DEADLOCK
};

#ifndef TEST_CASE
#define TEST_CASE INVERSION
#endif

static void init_locks(void)
{
    if (TEST_CASE >= CEILING_INVERSION) {
        const UBaseType_t ceiling =
            (UBaseType_t)W2E3_TASK_PRIORITY[TASK_H];

        pc_mutex_create_static(&s_ceiling_a, ceiling);
        pc_mutex_create_static(&s_ceiling_b, ceiling);
        return;
    }

    if (TEST_CASE == INVERSION) {
        s_lock_a = xSemaphoreCreateBinaryStatic(&s_storage_a);
    } else {
        s_lock_a = xSemaphoreCreateMutexStatic(&s_storage_a);
    }

    s_lock_b = xSemaphoreCreateMutexStatic(&s_storage_b);
    configASSERT(s_lock_a != NULL && s_lock_b != NULL);

    /* Binary semaphore mới tạo đang rỗng: mở khóa ban đầu. */
    if (TEST_CASE == INVERSION) {
        const BaseType_t given = xSemaphoreGive(s_lock_a);
        configASSERT(given == pdTRUE);
    }
}

/* STEP 4: Hiệu chuẩn workload CPU bằng DWT. */
static uint32_t s_loops_per_ms;

__attribute__((noinline))
static void work_loop(uint32_t count)
{
    while (count-- != 0u) {
        __NOP();
    }
}

static void calibrate_work(void)
{
    const uint32_t saved_mask = __get_PRIMASK();
    __disable_irq();

    const uint32_t start = DWT->CYCCNT;
    work_loop(10000u);
    const uint32_t elapsed = DWT->CYCCNT - start;

    __set_PRIMASK(saved_mask);
    configASSERT(elapsed != 0u);

    s_loops_per_ms = (uint32_t)(
        (10000ULL * (SystemCoreClock / 1000u)) / elapsed);
    configASSERT(s_loops_per_ms != 0u);
}

static void work_ms(uint32_t ms)
{
    while (ms-- != 0u) {
        work_loop(s_loops_per_ms);
    }
}

/* STEP 5: Tag cố định; không thay đổi khi priority thay đổi. */
static BaseType_t tag_l(void *arg)
{
    (void)arg;
    return 1;
}

static BaseType_t tag_m1(void *arg)
{
    (void)arg;
    return 2;
}

static BaseType_t tag_m2(void *arg)
{
    (void)arg;
    return 3;
}

static BaseType_t tag_h(void *arg)
{
    (void)arg;
    return 4;
}

static const TaskHookFunction_t s_tags[W2E3_NUM_TASKS] = {
    tag_l, tag_m1, tag_m2, tag_h
};

/* STEP 6: Giữ cùng thao tác lấy/nhả khi đổi cơ chế khóa. */
static void take_lock(resource_t resource)
{
    if (TEST_CASE >= CEILING_INVERSION) {
        pc_mutex_lock(resource == LOCK_A
            ? &s_ceiling_a : &s_ceiling_b);
    } else {
        SemaphoreHandle_t handle = resource == LOCK_A
            ? s_lock_a : s_lock_b;

        const BaseType_t taken =
            xSemaphoreTake(handle, portMAX_DELAY);
        configASSERT(taken == pdTRUE);
    }
}

static void give_lock(resource_t resource)
{
    if (TEST_CASE >= CEILING_INVERSION) {
        pc_mutex_unlock(resource == LOCK_A
            ? &s_ceiling_a : &s_ceiling_b);
    } else {
        SemaphoreHandle_t handle = resource == LOCK_A
            ? s_lock_a : s_lock_b;

        const BaseType_t given = xSemaphoreGive(handle);
        configASSERT(given == pdTRUE);
    }
}

/* STEP 7: L giữ A; riêng deadlock lấy thêm B. */
static void task_l(void *arg)
{
    (void)arg;
    take_lock(LOCK_A);

    if (TEST_CASE == DEADLOCK ||
        TEST_CASE == CEILING_DEADLOCK) {
        configASSERT(W2E3_LOCK_HOLD_MS_A >= 3u);

        work_ms(3u);
        s_l_waiting_b = 1u;
        take_lock(LOCK_B);
        s_l_waiting_b = 0u;

        work_ms(W2E3_LOCK_HOLD_MS_B);
        give_lock(LOCK_B);
        work_ms(W2E3_LOCK_HOLD_MS_A - 3u);
    } else {
        work_ms(W2E3_LOCK_HOLD_MS_A);
    }

    give_lock(LOCK_A);

    for (;;) {
        vTaskSuspend(NULL);
    }
}

/* STEP 8: M2 gây nhiễu CPU hoặc giữ B. */
static void task_m2(void *arg)
{
    (void)arg;
    TickType_t release = (TickType_t)configINITIAL_TICK_COUNT;

    if (TEST_CASE == INVERSION ||
        TEST_CASE == CEILING_INVERSION) {
        xTaskDelayUntil(&release, pdMS_TO_TICKS(4u));
        work_ms(30u);
    } else if (TEST_CASE == CHAINED_BLOCKING ||
               TEST_CASE == CEILING_CHAINED) {
        xTaskDelayUntil(&release, pdMS_TO_TICKS(1u));

        take_lock(LOCK_B);
        work_ms(W2E3_LOCK_HOLD_MS_B);
        give_lock(LOCK_B);
    }

    for (;;) {
        vTaskSuspend(NULL);
    }
}

/* STEP 9: Đo mỗi lần H lấy khóa, kể cả thời gian chờ. */
static void timed_take(resource_t resource)
{
    s_h_pending = resource;
    const uint32_t start = DWT->CYCCNT;

    take_lock(resource);

    s_h_take_cycles[resource] = DWT->CYCCNT - start;
    s_h_take_mask |= 1u << resource;
    s_h_pending = NO_LOCK;
}

/* STEP 10: H được kích hoạt tại tick 2. */
static void task_h(void *arg)
{
    (void)arg;
    TickType_t release = (TickType_t)configINITIAL_TICK_COUNT;

    xTaskDelayUntil(&release, pdMS_TO_TICKS(2u));
    s_h_ready_ticks = xTaskGetTickCount() - release;

    if (TEST_CASE == DEADLOCK ||
        TEST_CASE == CEILING_DEADLOCK) {
        timed_take(LOCK_B);
        work_ms(1u);
        timed_take(LOCK_A);

        give_lock(LOCK_A);
        give_lock(LOCK_B);
    } else {
        timed_take(LOCK_A);
        give_lock(LOCK_A);

        if (TEST_CASE == CHAINED_BLOCKING ||
            TEST_CASE == CEILING_CHAINED) {
            timed_take(LOCK_B);
            give_lock(LOCK_B);
        }
    }

    s_h_done = 1u;

    for (;;) {
        vTaskSuspend(NULL);
    }
}

/* STEP 11: Đổi cycle sang microsecond sau phép thử. */
static uint32_t cycles_to_us(uint32_t cycles)
{
    return (uint32_t)(
        ((uint64_t)cycles * 1000000u) / SystemCoreClock);
}

/* STEP 12: M1 đợi 100 ms rồi dừng trace và báo cáo. */
static void task_m1(void *arg)
{
    (void)arg;
    TickType_t release = (TickType_t)configINITIAL_TICK_COUNT;

    xTaskDelayUntil(&release, pdMS_TO_TICKS(100u));
    tracer_stop();

    if (s_h_done != 0u) {
        uart1_write_string("# STATUS=COMPLETE\r\n");
    } else if (s_l_waiting_b != 0u && s_h_pending == LOCK_A) {
        uart1_write_string("# STATUS=CIRCULAR_WAIT\r\n");
    } else {
        uart1_write_string("# STATUS=INCOMPLETE\r\n");
    }

    uart1_write_string(
        "H_DONE,READY_TICKS,TAKE_A_US,TAKE_B_US,"
        "WAIT_US_EST,L_WAIT_B,H_PENDING\r\n");

    uart1_write_u32(s_h_done);
    uart1_write_char(',');
    uart1_write_u32((uint32_t)s_h_ready_ticks);

    for (uint32_t i = 0u; i < 2u; ++i) {
        uart1_write_char(',');

        if ((s_h_take_mask & (1u << i)) != 0u) {
            uart1_write_u32(cycles_to_us(s_h_take_cycles[i]));
        } else {
            uart1_write_string("NA");
        }
    }

    uart1_write_char(',');

    if (s_h_done != 0u) {
        const uint32_t ready_us = (uint32_t)(
            ((uint64_t)s_h_ready_ticks * 1000000u)
            / configTICK_RATE_HZ);

        const uint32_t take_us = cycles_to_us(
            s_h_take_cycles[LOCK_A] + s_h_take_cycles[LOCK_B]);

        uart1_write_u32(ready_us + take_us);
    } else {
        uart1_write_string("NA");
    }

    uart1_write_char(',');
    uart1_write_u32(s_l_waiting_b);
    uart1_write_char(',');
    uart1_write_u32((uint32_t)s_h_pending);
    uart1_write_string("\r\n# TRACE: cycles,tag,event\r\n");

    const uint32_t records = tracer_dump_uart();

    uart1_write_string("# RECORDS=");
    uart1_write_u32(records);
    uart1_write_string("\r\n# END\r\n");
    uart1_flush();

    for (;;) {
        vTaskSuspend(NULL);
    }
}

/* STEP 13: Khởi tạo và chạy bốn task static. */
int main(void)
{
    NVIC_SetPriorityGrouping(0u);

    const bool uart_ok = uart1_init(115200u);
    configASSERT(uart_ok);

    tracer_init();
    calibrate_work();
    init_locks();

    static const TaskFunction_t entries[W2E3_NUM_TASKS] = {
        task_l, task_m1, task_m2, task_h
    };

    static const char *const names[W2E3_NUM_TASKS] = {
        "L", "M1", "M2", "H"
    };

    for (uint32_t i = 0u; i < W2E3_NUM_TASKS; ++i) {
        s_tasks[i] = xTaskCreateStatic(
            entries[i],
            names[i],
            STACK_WORDS,
            NULL,
            (UBaseType_t)W2E3_TASK_PRIORITY[i],
            s_stacks[i],
            &s_tcbs[i]);

        configASSERT(s_tasks[i] != NULL);
        vTaskSetApplicationTaskTag(s_tasks[i], s_tags[i]);
    }

    uart1_write_string("# CASE=");
    uart1_write_u32((uint32_t)TEST_CASE);

    uart1_write_string(
        "\r\n# TAGS: 0=IDLE,1=L,2=M1,3=M2,4=H"
        "\r\n# H_PENDING: 0=A,1=B,2=NONE\r\n");
    uart1_flush();

    DWT->CYCCNT = 0u;
    vTaskStartScheduler();

    configASSERT(0);
    for (;;) {
        __NOP();
    }
}