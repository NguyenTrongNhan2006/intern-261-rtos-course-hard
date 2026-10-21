#define W2E2_SPSC_RING_IMPLEMENTATION
#include "spsc_ring.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "uart.h"

/* STEP 1: Ba lượt chạy; mọi bộ nhớ đều static. */
typedef enum {
    STRESS,
    BENCH_RING,
    BENCH_QUEUE
} phase_t;

static spsc_ring_t s_ring;
static QueueHandle_t s_queue;
static StaticQueue_t s_queue_tcb;
static spsc_elem_t s_queue_storage[W2E2_RING_CAPACITY];

static volatile phase_t s_phase;
static volatile uint32_t s_limit, s_produced, s_consumed, s_overrun;
static volatile uint64_t s_push_cycles;
static uint32_t s_overhead;

static StaticTask_t s_consumer_tcb, s_idle_tcb;
_Alignas(8) static StackType_t s_consumer_stack[256];
_Alignas(8) static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];

/* STEP 2: FreeRTOS cần bộ nhớ Idle và hook stack overflow. */
void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb, StackType_t **stack, configSTACK_DEPTH_TYPE *depth)
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

/* STEP 3: Chặn compiler dời truy cập bộ nhớ qua mốc đo. */
static inline uint32_t cycle_now(void)
{
    __COMPILER_BARRIER();
    const uint32_t value = DWT->CYCCNT;
    __COMPILER_BARRIER();
    return value;
}

static uint32_t measure_overhead(void)
{
    uint32_t best = UINT32_MAX;

    for (uint32_t i = 0u; i < 64u; ++i) {
        const uint32_t start = cycle_now();
        const uint32_t elapsed = cycle_now() - start;
        if (elapsed < best) {
            best = elapsed;
        }
    }
    return best;
}

/* STEP 4: TIM2 clock 72 MHz -> 1 MHz -> 10 kHz, mỗi tick 100 us. */
static void timer_init(void)
{
    NVIC_DisableIRQ(TIM2_IRQn);
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;

    TIM2->PSC = 71u;
    TIM2->ARR = 99u;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM2_IRQn,
                     configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
}

static void timer_stop(void)
{
    NVIC_DisableIRQ(TIM2_IRQn);
    TIM2->CR1 = 0u;
    __DSB();
    __ISB();
}

/* STEP 5: Một ISR producer; không log và không chờ trong ISR. */
void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) == 0u) {
        return;
    }
    TIM2->SR = 0u;

    const phase_t phase = s_phase;
    const uint32_t seq = s_produced;

    if (s_overrun != 0u || seq == s_limit) {
        return;
    }

    if (phase != STRESS) {
        if (seq != s_consumed) {
            return; /* Benchmark: chờ task lấy mẫu trước. */
        }
        __DMB();
    }

    const spsc_elem_t elem = { seq };
    BaseType_t higher_priority_task_woken = pdFALSE;
    bool ok;
    uint32_t elapsed;

    if (phase == BENCH_QUEUE) {
        const uint32_t start = cycle_now();
        ok = xQueueSendFromISR(s_queue, &elem,
                              &higher_priority_task_woken) == pdPASS;
        elapsed = cycle_now() - start;
    } else {
        const uint32_t start = cycle_now();
        ok = spsc_ring_push_from_isr(&s_ring, elem);
        elapsed = cycle_now() - start;
    }

    if (!ok) {
        s_overrun = 1u; /* Dừng ngay ở lần đầy đầu tiên. */
        TIM2->CR1 = 0u;
    } else {
        if (phase != STRESS) {
            s_push_cycles += elapsed > s_overhead
                           ? elapsed - s_overhead : 0u;
            __DMB(); /* Hoàn tất mẫu đo trước khi báo task. */
        }
        s_produced = seq + 1u;
        if (seq + 1u == s_limit) {
            TIM2->CR1 = 0u;
        }
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/* STEP 6: Một task consumer; chạy riêng từng lượt để so sánh. */
static bool run_phase(phase_t phase, uint32_t limit)
{
    timer_stop();
    spsc_ring_init(&s_ring);
    s_phase = phase;
    s_limit = limit;
    s_produced = 0u;
    s_consumed = 0u;
    s_overrun = 0u;
    s_push_cycles = 0u;

    uint32_t received = 0u;
    uint64_t pop_cycles = 0u;
    bool mismatch = false;
    bool api_failed = false;
    spsc_elem_t elem = { 0u };

    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    __DMB();
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;

    while (received < limit) {
        if (s_overrun != 0u) {
            break;
        }

        if (phase != STRESS) {
            if (s_produced == received) {
                continue;
            }
            __DMB(); /* Task đã quan sát producer công bố mẫu. */
        }

        bool ok;
        uint32_t elapsed;

        if (phase == BENCH_QUEUE) {
            const uint32_t start = cycle_now();
            ok = xQueueReceive(s_queue, &elem, 0u) == pdPASS;
            elapsed = cycle_now() - start;
        } else {
            const uint32_t start = cycle_now();
            ok = spsc_ring_pop(&s_ring, &elem);
            elapsed = cycle_now() - start;
        }

        if (!ok) {
            if (phase != STRESS) {
                api_failed = true;
                break;
            }
            continue; /* Ring rỗng trong stress là bình thường. */
        }

        const uint32_t expected = received++;
        if (elem.seq != expected) {
            mismatch = true;
            break;
        }

        if (phase != STRESS) {
            pop_cycles += elapsed > s_overhead
                        ? elapsed - s_overhead : 0u;
        }

        if (phase != STRESS) {
            __DMB(); /* Chép và kiểm tra xong trước khi cho mẫu mới. */
            s_consumed = received;
        }
    }

    timer_stop(); /* ISR đã dừng trước khi đọc tổng 64 bit. */

    const bool pass = !mismatch && !api_failed &&
                      s_overrun == 0u &&
                      received == limit && s_produced == limit;

    uart1_write_string(phase == STRESS ? "STRESS" :
                       phase == BENCH_RING ? "RING" : "QUEUE");
    uart1_write_string(pass ? ",PASS," : ",FAIL,");
    uart1_write_u32(s_produced);
    uart1_write_char(',');
    uart1_write_u32(received);
    uart1_write_char(',');
    uart1_write_u32(s_overrun);

    if (phase != STRESS && pass) {
        uart1_write_char(',');
        uart1_write_u32((uint32_t)(s_push_cycles / limit));
        uart1_write_char(',');
        uart1_write_u32((uint32_t)(pop_cycles / limit));
    } else {
        uart1_write_string(",NA,NA");
    }
    uart1_write_string("\r\n");

    if (mismatch) {
        uart1_write_string("FIRST_ERROR,expected=");
        uart1_write_u32(received - 1u);
        uart1_write_string(",actual=");
        uart1_write_u32(elem.seq);
        uart1_write_string("\r\n");
    }
    if (s_overrun != 0u) {
        uart1_write_string("FIRST_OVERRUN,seq=");
        uart1_write_u32(s_produced);
        uart1_write_string("\r\n");
    }
    if (api_failed) {
        uart1_write_string("ERROR: benchmark receive failed\r\n");
    }
    return pass;
}

/* STEP 7: Stress trước; chỉ benchmark khi lượt trước PASS. */
static void consumer_task(void *argument)
{
    (void)argument;
    s_overhead = measure_overhead();
    configASSERT(s_overhead != 0u);

    uart1_write_string("# DWT_OVERHEAD=");
    uart1_write_u32(s_overhead);
    uart1_write_string("\r\n");

    if (run_phase(STRESS, W2E2_STRESS_COUNT) &&
        run_phase(BENCH_RING, W2E2_BENCH_ITERATIONS)) {
        (void)run_phase(BENCH_QUEUE, W2E2_BENCH_ITERATIONS);
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000u));
    }
}

/* STEP 8: Khởi tạo board, Queue và một task rồi chạy scheduler. */
int main(void)
{
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3u);
    __DSB();
    __ISB();
    configASSERT(SystemCoreClock == 72000000u);
    configASSERT((RCC->CFGR & RCC_CFGR_PPRE1) ==
                 RCC_CFGR_PPRE1_DIV2);

    const bool uart_ready = uart1_init(115200u);
    configASSERT(uart_ready);

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
    timer_init();

    s_queue = xQueueCreateStatic(W2E2_RING_CAPACITY,
                                sizeof(spsc_elem_t),
                                (uint8_t *)s_queue_storage,
                                &s_queue_tcb);
    configASSERT(s_queue != NULL);

    uart1_write_string("PHASE,STATUS,PRODUCED,RECEIVED,OVERRUN,"
                       "MEAN_PUSH,MEAN_POP\r\n");

    TaskHandle_t task = xTaskCreateStatic(
        consumer_task, "consumer", 256u, NULL, 1u,
        s_consumer_stack, &s_consumer_tcb);
    configASSERT(task != NULL);

    vTaskStartScheduler();
    configASSERT(0);
    for (;;) {
        __NOP();
    }
}