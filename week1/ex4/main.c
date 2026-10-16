#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"

#if W1E4_HEAP_STAGE

#include "heap_experiment.h"

void vApplicationMallocFailedHook(void)
{
    __disable_irq();

    uart1_write_string("# MALLOC FAILED\r\n");
    uart1_flush();

    for (;;) {
        __NOP();
    }
}

#else

#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "gpio.h"

#if W1E4_APP == 1

#include "../ex1/include/params.h"

#define USER_TASKS 2u

static const char * const names[USER_TASKS] = {
    "BlinkA", "BlinkB"
};

#elif W1E4_APP == 2

#include "tracer.h"

#define USER_TASKS 2u

static const char * const names[USER_TASKS] = {
    "TraceA", "TraceB"
};

#elif W1E4_APP == 3

#include "tracer.h"
#include "../ex3/include/params.h"

#define USER_TASKS W1E3_NUM_TASKS

static const char * const names[USER_TASKS] = {
    "RTA0", "RTA1", "RTA2", "RTA3"
};

_Static_assert(USER_TASKS == 4u, "four RTA tags required");

#else
#error Invalid W1E4_APP
#endif

#define USER_STACK_WORDS 128u
#define TIMER_STACK_WORDS configTIMER_TASK_STACK_DEPTH
#define MIN_FREE_WORDS 32u

/* Bộ nhớ tồn tại trong toàn bộ thời gian chạy */
_Alignas(8) static StackType_t user_stacks[USER_TASKS][USER_STACK_WORDS];
_Alignas(8) static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
_Alignas(8) static StackType_t timer_stack[TIMER_STACK_WORDS];

static StaticTask_t user_tcbs[USER_TASKS];
static StaticTask_t idle_tcb;
static StaticTask_t timer_tcb;

static TaskHandle_t handles[USER_TASKS];
static uint32_t indices[USER_TASKS];
static UBaseType_t watermarks[USER_TASKS + 2u];

static StaticQueue_t queue_cb;
static StaticQueue_t mutex_cb;

static uint8_t queue_storage[4u * sizeof(uint32_t)];

static QueueHandle_t queue_handle;
static SemaphoreHandle_t mutex_handle;

static uint32_t reported;

#if W1E4_APP != 1

typedef struct {
    uint32_t target;
    uint32_t iterations;
    uint32_t minimum;
    uint32_t maximum;
} calibration_t;

static calibration_t calibration[USER_TASKS];

/* Số vòng lặp cố định không bị rút ngắn khi task bị preempt */
__attribute__((noinline, noclone))
static void fixed_work(uint32_t iterations)
{
    __asm volatile(
        "1: subs %0, %0, #1\n"
        "bne 1b\n"
        : "+r"(iterations)
        :
        : "cc"
    );
}

__attribute__((noinline, noclone))
static uint32_t measure_work(uint32_t iterations)
{
    __DSB();
    __ISB();

    const uint32_t start = DWT->CYCCNT;

    fixed_work(iterations);

    return DWT->CYCCNT - start;
}

/* Hiệu chỉnh trước scheduler, trong khi IRQ đang tắt */
static void calibrate(uint32_t index, uint32_t microseconds)
{
    calibration_t * const c = &calibration[index];

    c->target = microseconds * (SystemCoreClock / 1000000u);
    configASSERT(c->target != 0u);

    uint32_t low = 1u;
    uint32_t high = c->target;

    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;

        if (measure_work(middle) < c->target) {
            low = middle + 1u;
        } else {
            high = middle;
        }
    }

    c->iterations = low;

    for (uint32_t attempt = 0u; attempt < 32u; ++attempt) {
        c->minimum = UINT32_MAX;
        c->maximum = 0u;

        for (uint32_t sample = 0u; sample < 8u; ++sample) {
            const uint32_t elapsed = measure_work(c->iterations);

            if (elapsed < c->minimum) {
                c->minimum = elapsed;
            }

            if (elapsed > c->maximum) {
                c->maximum = elapsed;
            }
        }

        if (c->minimum >= c->target) {
            break;
        }

        ++c->iterations;
    }

    configASSERT(c->minimum >= c->target);
    configASSERT(c->maximum - c->target <= SystemCoreClock / 1000000u);
}

static BaseType_t tag1(void *p)
{
    (void)p;
    return 1;
}

static BaseType_t tag2(void *p)
{
    (void)p;
    return 2;
}

#if W1E4_APP == 3

static BaseType_t tag3(void *p)
{
    (void)p;
    return 3;
}

static BaseType_t tag4(void *p)
{
    (void)p;
    return 4;
}

static TaskHookFunction_t const tags[USER_TASKS] = {
    tag1, tag2, tag3, tag4
};

static uint32_t hyperperiod;
static volatile uint32_t completed;

static uint32_t gcd(uint32_t a, uint32_t b)
{
    while (b != 0u) {
        const uint32_t r = a % b;
        a = b;
        b = r;
    }

    return a;
}

#else

static TaskHookFunction_t const tags[USER_TASKS] = {
    tag1, tag2
};

#endif
#endif

static void user_task(void *argument)
{
    const uint32_t index = *(const uint32_t *)argument;

#if W1E4_APP == 1

    const TickType_t period = pdMS_TO_TICKS(
        index == 0u
            ? W1E1_TASK_A_PERIOD_MS
            : W1E1_TASK_B_PERIOD_MS
    );

    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        (void)xTaskDelayUntil(&wake, period);

        taskENTER_CRITICAL();

        if ((W1E1_TAG_GPIO_PORT->ODR &
             (1u << W1E1_TAG_GPIO_PIN)) != 0u) {
            gpio_clear(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);
        } else {
            gpio_set(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);
        }

        taskEXIT_CRITICAL();
    }

#elif W1E4_APP == 2

    const TickType_t period =
        pdMS_TO_TICKS(index == 0u ? 2u : 3u);

    TickType_t wake = xTaskGetTickCount();
    BaseType_t dumped = pdFALSE;

    for (;;) {
        (void)measure_work(calibration[index].iterations);

        if (index == 1u && dumped == pdFALSE) {
            if (tracer_dump_uart() != 0u) {
                dumped = pdTRUE;
            }
        }

        (void)xTaskDelayUntil(&wake, period);
    }

#else

    const TickType_t period =
        pdMS_TO_TICKS(W1E3_PERIOD_MS[index]);

    /* Các task bắt đầu tại tick 20, chạy 5 hyperperiod */
    TickType_t wake = (TickType_t)20u - period;

    const uint32_t jobs =
        hyperperiod * 5u / W1E3_PERIOD_MS[index];

    for (uint32_t job = 0u; job < jobs; ++job) {
        (void)xTaskDelayUntil(&wake, period);

        (void)measure_work(calibration[index].iterations);
    }

    taskENTER_CRITICAL();
    completed |= UINT32_C(1) << index;
    taskEXIT_CRITICAL();

    for (;;) {
        vTaskSuspend(NULL);
    }

#endif
}

/* Callback trả lại bộ nhớ tĩnh cho kernel */
void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *depth
)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(
    StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *depth
)
{
    *tcb = &timer_tcb;
    *stack = timer_stack;
    *depth = TIMER_STACK_WORDS;
}

void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *name
)
{
    (void)task;
    (void)name;

    __disable_irq();

    for (;;) {
        __NOP();
    }
}

static void number(uint32_t value)
{
    uart1_write_char(',');
    uart1_write_u32(value);
}

static void print_stack(
    const char *name,
    uint32_t depth,
    uint32_t free_words
)
{
    uart1_write_string("STACK,");
    uart1_write_string(name);

    number(depth);
    number(free_words);
    number(depth - free_words);
    number(free_words * sizeof(StackType_t));

    uart1_write_string("\r\n");
}

void vApplicationIdleHook(void)
{
#if W1E4_APP == 3

    if (completed != ((UINT32_C(1) << USER_TASKS) - 1u)) {
        return;
    }

#else

    if (xTaskGetTickCount() < pdMS_TO_TICKS(10000u)) {
        return;
    }

#endif

    if (reported != 0u) {
        return;
    }

    reported = 1u;

    /* Kiểm tra queue và mutex trong ngữ cảnh task thật */
    const uint32_t sent = UINT32_C(0x2452882);
    uint32_t received = 0u;

    configASSERT(xQueueSend(queue_handle, &sent, 0u) == pdPASS);

    configASSERT(
        xQueueReceive(queue_handle, &received, 0u) == pdPASS
    );

    configASSERT(received == sent);

    configASSERT(
        xSemaphoreTake(mutex_handle, 0u) == pdTRUE
    );

    configASSERT(
        xSemaphoreGive(mutex_handle) == pdTRUE
    );

    uart1_write_string("# QUEUE_MUTEX_SELFTEST,PASS\r\n");

#if W1E4_APP != 1

    (void)tracer_dump_uart();

    for (uint32_t i = 0u; i < USER_TASKS; ++i) {
        uart1_write_string("CAL");

        number(i);
        number(calibration[i].target);
        number(calibration[i].iterations);
        number(calibration[i].minimum);
        number(calibration[i].maximum);

        uart1_write_string("\r\n");
    }

#endif

    for (uint32_t i = 0u; i < USER_TASKS; ++i) {
        watermarks[i] =
            uxTaskGetStackHighWaterMark(handles[i]);
    }

    watermarks[USER_TASKS + 1u] =
        uxTaskGetStackHighWaterMark(
            xTimerGetTimerDaemonTaskHandle()
        );

    uint32_t margin_ok = 1u;

    uart1_write_string(
        "# STACK,name,depth_words,free_words,used_words,free_bytes\r\n"
    );

    for (uint32_t i = 0u; i < USER_TASKS; ++i) {
        print_stack(
            names[i],
            USER_STACK_WORDS,
            watermarks[i]
        );
    }

    /* Đo Idle sau khi đã chạy đường gọi hàm in báo cáo */
    watermarks[USER_TASKS] =
        uxTaskGetStackHighWaterMark(
            xTaskGetIdleTaskHandle()
        );

    print_stack(
        "Idle",
        configMINIMAL_STACK_SIZE,
        watermarks[USER_TASKS]
    );

    print_stack(
        "Timer",
        TIMER_STACK_WORDS,
        watermarks[USER_TASKS + 1u]
    );

    for (uint32_t i = 0u; i < USER_TASKS + 2u; ++i) {
        if (watermarks[i] < MIN_FREE_WORDS) {
            margin_ok = 0u;
        }
    }

    uart1_write_string("# STACK_MARGIN_32_WORDS");
    number(margin_ok);

    uart1_write_string(
        "\r\n# W1E4 STATIC DONE\r\n"
    );

    uart1_flush();
}

static void static_app_init(void)
{
#if W1E4_APP == 1

      gpio_clock_enable(W1E1_TAG_GPIO_PORT);

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    (void)RCC->APB2ENR;

    AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    gpio_clear(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);

    gpio_config_output_pp_2mhz(
        W1E1_TAG_GPIO_PORT,
        W1E1_TAG_GPIO_PIN
    );

#else

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    configASSERT(
        (DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) == 0u
    );

    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    __DSB();
    __ISB();

    const uint32_t saved = __get_PRIMASK();
    __disable_irq();

#if W1E4_APP == 3
    hyperperiod = 1u;
#endif

    for (uint32_t i = 0u; i < USER_TASKS; ++i) {
#if W1E4_APP == 3

        configASSERT(
            W1E3_DEADLINE_MS[i] == W1E3_PERIOD_MS[i]
        );

        configASSERT(W1E3_PERIOD_MS[i] != 0u);

        configASSERT(
            W1E3_BUSY_WAIT_US[i] > 0u &&
            W1E3_BUSY_WAIT_US[i] <
                W1E3_PERIOD_MS[i] * 1000u
        );

        const uint64_t h =
            (uint64_t)(
                hyperperiod /
                gcd(hyperperiod, W1E3_PERIOD_MS[i])
            ) * W1E3_PERIOD_MS[i];

        configASSERT(h <= 10000u);

        hyperperiod = (uint32_t)h;

        calibrate(i, W1E3_BUSY_WAIT_US[i]);

#else

        calibrate(i, i == 0u ? 200u : 300u);

#endif
    }

    __set_PRIMASK(saved);

    tracer_init();

#endif

    /* Tạo các object trước context switch đầu tiên */
    queue_handle = xQueueCreateStatic(
        4u,
        sizeof(uint32_t),
        queue_storage,
        &queue_cb
    );

    mutex_handle =
        xSemaphoreCreateMutexStatic(&mutex_cb);

    configASSERT(
        queue_handle != NULL &&
        mutex_handle != NULL
    );

    for (uint32_t i = 0u; i < USER_TASKS; ++i) {
        indices[i] = i;

        UBaseType_t priority = 1u;

#if W1E4_APP == 3

        for (uint32_t j = 0u; j < USER_TASKS; ++j) {
            if (
                W1E3_PERIOD_MS[j] > W1E3_PERIOD_MS[i] ||
                (
                    W1E3_PERIOD_MS[j] == W1E3_PERIOD_MS[i] &&
                    j > i
                )
            ) {
                ++priority;
            }
        }

#endif

        handles[i] = xTaskCreateStatic(
            user_task,
            names[i],
            USER_STACK_WORDS,
            &indices[i],
            priority,
            user_stacks[i],
            &user_tcbs[i]
        );

        configASSERT(handles[i] != NULL);

#if W1E4_APP != 1

        vTaskSetApplicationTaskTag(
            handles[i],
            tags[i]
        );

#endif
    }
}

#endif

int main(void)
{
    SCB->AIRCR =
        (0x5FAu << SCB_AIRCR_VECTKEY_Pos) |
        (3u << SCB_AIRCR_PRIGROUP_Pos);

    __DSB();
    __ISB();

    configASSERT(SystemCoreClock == 72000000u);
    configASSERT(uart1_init(115200u));

#if W1E4_HEAP_STAGE

    heap_experiment_run();

    for (;;) {
        __WFI();
    }

#else

    uart1_write_string("# W1E4 STATIC APP,");
    uart1_write_u32(W1E4_APP);
    uart1_write_string("\r\n");

    static_app_init();

    vTaskStartScheduler();

    configASSERT(0);

    for (;;) {
        __NOP();
    }

#endif
}