/* Executes the real tracer.c with fake registers and UART on the host.
 * This tests logic and framing, not MCU timing or memory-bus ordering. */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "uart.h"

DWT_Type test_dwt;
CoreDebug_Type test_core_debug;
GPIO_TypeDef test_gpio;
uint32_t test_ipsr, test_primask;
static BaseType_t callback_value;
static unsigned thread_gets, isr_gets;
static int use_null_tag;
static char output[32768];
static size_t output_size;

static BaseType_t tag_callback(void *p) { (void)p; return callback_value; }
TaskHookFunction_t xTaskGetApplicationTaskTag(void *p)
{
    (void)p; ++thread_gets; return use_null_tag ? NULL : tag_callback;
}
TaskHookFunction_t xTaskGetApplicationTaskTagFromISR(void *p)
{
    (void)p; ++isr_gets; return use_null_tag ? NULL : tag_callback;
}
void gpio_clock_enable(GPIO_TypeDef *p) { (void)p; }
void gpio_clear(GPIO_TypeDef *p, uint8_t pin) { p->BRR = 1u << pin; }
void gpio_config_output_pp_2mhz(GPIO_TypeDef *p, uint8_t pin)
{ (void)p; (void)pin; }
void uart1_write_char(char c)
{
    assert(output_size + 1 < sizeof output);
    output[output_size++] = c; output[output_size] = '\0';
}
void uart1_write_string(const char *s) { while (*s) uart1_write_char(*s++); }
void uart1_write_u32(uint32_t n)
{
    char s[11]; snprintf(s, sizeof s, "%" PRIu32, n); uart1_write_string(s);
}
void uart1_write_u64(uint64_t n)
{
    char s[21]; snprintf(s, sizeof s, "%" PRIu64, n); uart1_write_string(s);
}
void uart1_flush(void) { }

#include "../../week1/ex2/tracer.c"

static void reset_test(void)
{
    memset(&test_dwt, 0, sizeof test_dwt);
    memset(&test_gpio, 0, sizeof test_gpio);
    output_size = 0; output[0] = '\0';
    callback_value = 1; use_null_tag = 0;
    thread_gets = 0; isr_gets = 0;
    test_ipsr = 14; test_primask = 0;
    tracer_init();
    assert(tracer_state.recording == 1);
    assert(test_dwt.CTRL & DWT_CTRL_CYCCNTENA_Msk);
}

int main(void)
{
    reset_test();
    callback_value = 3;
    test_gpio.BSRR = 0x80000000u;
    test_dwt.CYCCNT = 10;
    tracer_on_switched_in();
    assert(tracer_state.records[0].task_tag == 3);
    if (TRACER_MAX_GPIO_TAG < 3u) assert(test_gpio.BSRR == 0x80000000u);
    assert(isr_gets == 1 && thread_gets == 0);
    callback_value = -1;
    test_dwt.CYCCNT = 20;
    tracer_on_switched_out();
    assert(tracer_state.records[1].task_tag == UINT8_MAX);
    callback_value = 1000;
    test_ipsr = 0;
    test_dwt.CYCCNT = 30;
    tracer_on_switched_in();
    assert(tracer_state.records[2].task_tag == UINT8_MAX);
    assert(thread_gets == 1);
    use_null_tag = 1;
    test_dwt.CYCCNT = 40;
    tracer_on_switched_out();
    assert(tracer_state.records[3].task_tag == 0);
    use_null_tag = 0;
    callback_value = 0;
    test_dwt.CYCCNT = 50;
    tracer_on_switched_in();
    assert(tracer_state.records[4].task_tag == UINT8_MAX);
    test_primask = 1;
    tracer_stop();
    assert(test_gpio.BRR == TRACER_PIN_MASK && test_primask == 1);

    reset_test();
    test_dwt.CYCCNT = 1;
    tracer_on_switched_in();
    assert(test_gpio.BSRR == (1u << (TRACER_FIRST_PIN + 1u)));
    assert(tracer_dump_uart() == 0 && output_size == 0);

    reset_test();
    for (unsigned i = 0; i < 254; ++i) {
        test_dwt.CYCCNT = 100u + i;
        if (i % 2 == 0) tracer_on_switched_in();
        else tracer_on_switched_out();
    }
    assert(tracer_state.recording == 0 && tracer_state.overflow_count == 0);
    assert(tracer_state.head == 254 && test_gpio.BRR == TRACER_PIN_MASK);
    tracer_on_switched_in();
    assert(tracer_state.head == 254);
    assert(tracer_dump_uart() == 254);
    assert(strstr(output, "# overflow=0\r\n") != NULL);
    assert(strstr(output, "353,1,OUT\r\n") != NULL);
    assert(tracer_dump_uart() == 0);

    reset_test();
    assert(tracer_record(UINT32_MAX - 5u, 2, TRACER_EVENT_IN));
    assert(tracer_record(5u, 2, TRACER_EVENT_OUT));
    assert(tracer_record(UINT32_MAX - 15u, 1, TRACER_EVENT_IN));
    assert(tracer_record(7u, 1, TRACER_EVENT_OUT));
    tracer_stop();
    assert(tracer_dump_uart() == 4);
    assert(strstr(output, "4294967290,2,IN\r\n") != NULL);
    assert(strstr(output, "4294967301,2,OUT\r\n") != NULL);
    assert(strstr(output, "8589934576,1,IN\r\n") != NULL);
    assert(strstr(output, "8589934599,1,OUT\r\n") != NULL);

    reset_test();
    for (unsigned i = 0; i < 255; ++i)
        assert(tracer_record(i, 1, TRACER_EVENT_IN));
    assert(!tracer_record(255, 1, TRACER_EVENT_OUT));
    assert(tracer_state.overflow_count == 1 && tracer_state.recording == 0);
    puts("tracer host checks PASS: tags, ISR getter, GPIO mask, freeze/dump, two wraps, overflow");
    return 0;
}
