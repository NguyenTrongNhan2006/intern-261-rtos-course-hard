#include "FreeRTOS.h"
#include "task.h"

#include "gpio.h"
#include "tracer.h"
#include "uart.h"

#include <stdbool.h>

#define TRACER_BUFFER_CAPACITY 256u
#ifndef TRACER_FIRST_PIN
#define TRACER_FIRST_PIN 4u
#endif
#ifndef TRACER_LAST_PIN
#define TRACER_LAST_PIN 6u
#endif

_Static_assert(TRACER_FIRST_PIN <= TRACER_LAST_PIN, "invalid GPIO range");
_Static_assert(TRACER_LAST_PIN < 16u, "GPIO pin must be below 16");

#define TRACER_MAX_GPIO_TAG (TRACER_LAST_PIN - TRACER_FIRST_PIN)
#define TRACER_PIN_MASK \
    (((1u << (TRACER_MAX_GPIO_TAG + 1u)) - 1u) << TRACER_FIRST_PIN)
#define TRACER_EVENT_IN  0u
#define TRACER_EVENT_OUT 1u
#define TRACER_INVALID_TAG UINT8_MAX

typedef struct {
    tracer_record_t records[TRACER_BUFFER_CAPACITY];

    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t overflow_count;
    volatile uint32_t recording;
} tracer_state_t;

static tracer_state_t tracer_state;

static uint32_t tracer_next_index(uint32_t index)
{
    return (index + 1u) % TRACER_BUFFER_CAPACITY;
}

void tracer_init(void)
{
    tracer_state.recording = 0u;
    tracer_state.head = 0u;
    tracer_state.tail = 0u;
    tracer_state.overflow_count = 0u;

    gpio_clock_enable(GPIOA);

    for (uint8_t pin = TRACER_FIRST_PIN;
         pin <= TRACER_LAST_PIN;
         ++pin) {
        gpio_clear(GPIOA, pin);
        gpio_config_output_pp_2mhz(GPIOA, pin);
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    __DSB();

    configASSERT(
        (DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) == 0u
    );

    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    __DSB();
    __ISB();

    configASSERT(
        (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u
    );

    tracer_state.recording = 1u;
}

static void tracer_stop(void)
{
    const uint32_t saved_primask = __get_PRIMASK();

    __disable_irq();

    tracer_state.recording = 0u;
    __DMB();

    GPIOA->BRR = TRACER_PIN_MASK;

    __set_PRIMASK(saved_primask);
}

static bool tracer_record(
    uint32_t timestamp_cycles,
    uint8_t task_tag,
    uint8_t event
)
{
    if (tracer_state.recording == 0u) {
        return false;
    }

    if (event > TRACER_EVENT_OUT) {
        return false;
    }

    const uint32_t head = tracer_state.head;
    const uint32_t next = tracer_next_index(head);

    if (next == tracer_state.tail) {
        ++tracer_state.overflow_count;
        tracer_stop();
        return false;
    }

    /* Tags without a physical channel remain visible in the UART log. */
    if (task_tag <= TRACER_MAX_GPIO_TAG) {
        const uint32_t mask = 1u << (TRACER_FIRST_PIN + task_tag);

        if (event == TRACER_EVENT_IN) {
            GPIOA->BSRR = mask;
        } else {
            GPIOA->BRR = mask;
        }
    }

    tracer_state.records[head].timestamp_cycles = timestamp_cycles;
    tracer_state.records[head].task_tag = task_tag;
    tracer_state.records[head].event = event;

    __DMB();

    tracer_state.head = next;

    const uint32_t free_slots =
        (tracer_state.tail + TRACER_BUFFER_CAPACITY - next - 1u) %
        TRACER_BUFFER_CAPACITY;

    /* Stop after OUT while there is still room to preserve pairing. */
    if ((event == TRACER_EVENT_OUT) && (free_slots < 2u)) {
        tracer_stop();
    }

    return true;
}

uint32_t tracer_dump_uart(void)
{
    if (tracer_state.recording != 0u) {
        return 0u;
    }

    __DMB();

    if (tracer_state.tail == tracer_state.head) {
        return 0u;
    }

    uart1_write_string("# overflow=");
    uart1_write_u32(tracer_state.overflow_count);
    uart1_write_string("\r\n");

    /* Extend only after freezing, keeping 64-bit work out of the hooks.
     * Adjacent records must be less than one CYCCNT period apart. */
    uint64_t epoch = 0u;
    uint32_t previous_cycles =
        tracer_state.records[tracer_state.tail].timestamp_cycles;
    uint32_t dumped = 0u;

    while (tracer_state.tail != tracer_state.head) {
        const uint32_t tail = tracer_state.tail;
        const tracer_record_t record = tracer_state.records[tail];

        if (record.timestamp_cycles < previous_cycles) {
            epoch += UINT64_C(1) << 32u;
        }
        previous_cycles = record.timestamp_cycles;
        uart1_write_u64(epoch + record.timestamp_cycles);
        uart1_write_char(',');

        uart1_write_u32(record.task_tag);
        uart1_write_char(',');

        uart1_write_string(
            (record.event == TRACER_EVENT_IN)
                ? "IN\r\n"
                : "OUT\r\n"
        );

        tracer_state.tail = tracer_next_index(tail);
        ++dumped;
    }

    uart1_flush();

    return dumped;
}

static uint8_t tracer_current_tag(void)
{
    TaskHookFunction_t tag_function;

    if (__get_IPSR() == 0u) {
        tag_function = xTaskGetApplicationTaskTag(NULL);
    } else {
        tag_function = xTaskGetApplicationTaskTagFromISR(NULL);
    }

    /* The application reserves a NULL tag function for Idle (tag 0). */
    if (tag_function == NULL) {
        return 0u;
    }

    const BaseType_t tag = tag_function(NULL);

    /* Never stop PendSV for an application-provided tag value. */
    if ((tag <= 0) || ((uint32_t)tag >= TRACER_INVALID_TAG)) {
        return TRACER_INVALID_TAG;
    }

    return (uint8_t)tag;
}

void tracer_on_switched_in(void)
{
    const uint32_t timestamp = DWT->CYCCNT;

    if (tracer_state.recording == 0u) {
        return;
    }

    const uint8_t tag = tracer_current_tag();

    (void)tracer_record(
        timestamp,
        tag,
        TRACER_EVENT_IN
    );
}

void tracer_on_switched_out(void)
{
    const uint32_t timestamp = DWT->CYCCNT;

    if (tracer_state.recording == 0u) {
        return;
    }

    const uint8_t tag = tracer_current_tag();

    (void)tracer_record(
        timestamp,
        tag,
        TRACER_EVENT_OUT
    );
}