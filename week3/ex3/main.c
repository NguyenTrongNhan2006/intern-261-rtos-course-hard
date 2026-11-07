#include "FreeRTOS.h"
#include "task.h"

#include "fault_inject.h"
#include "fault_postmortem.h"
#include "params.h"
#include "stm32f1xx.h"
#include "uart.h"

#define LOG_BAUD            115200u
#define STEP_DELAY_MS       500u
#define FAULT_STACK_WORDS   256u

/* STEP 1: Cấp phát tĩnh stack và khối điều khiển cho task gây fault và Idle. */
static StaticTask_t s_fault_tcb;
static StackType_t s_fault_stack[FAULT_STACK_WORDS];
static StaticTask_t s_idle_tcb;
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];
static uint32_t s_first_step;
static bool s_halt_sequence;

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *stack_words)
{
    *tcb = &s_idle_tcb;
    *stack = s_idle_stack;
    *stack_words = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    configASSERT(0);
}

/* STEP 2: In nguyên nhân reset từ RCC_CSR để phân biệt reset mềm sau fault với
 * cấp nguồn lại (POR) hay brown-out, là các trường hợp làm mất .noinit. */
static void print_reset_cause(uint32_t csr)
{
    uart1_write_string("W3E3 BOOT reset=");
    if ((csr & RCC_CSR_LPWRRSTF) != 0u) { uart1_write_string("LPWR "); }
    if ((csr & RCC_CSR_WWDGRSTF) != 0u) { uart1_write_string("WWDG "); }
    if ((csr & RCC_CSR_IWDGRSTF) != 0u) { uart1_write_string("IWDG "); }
    if ((csr & RCC_CSR_SFTRSTF) != 0u)  { uart1_write_string("SOFT "); }
    if ((csr & RCC_CSR_PORRSTF) != 0u)  { uart1_write_string("POR "); }
    if ((csr & RCC_CSR_PINRSTF) != 0u)  { uart1_write_string("PIN "); }
    uart1_write_string("\r\n");
}

static void print_step(const char *event, uint32_t step, const char *name)
{
    uart1_write_string("W3E3 ");
    uart1_write_string(event);
    uart1_write_string(" step=");
    uart1_write_u32(step);
    uart1_write_string(" fault=");
    uart1_write_string(name);
    uart1_write_string("\r\n");
}

/* STEP 3: Task gây lần lượt từng fault còn lại của kịch bản. Mỗi lần ghi canary và
 * bước vào .noinit rồi mới gây lỗi. Nếu lệnh gây lỗi chạy qua mà không có fault thì
 * báo NO_FAULT và sang bước kế, nên chuỗi luôn kết thúc sau tối đa 4 lần reset. */
static void fault_task(void *arg)
{
    (void)arg;
    if (s_halt_sequence) {
        uart1_write_string("W3E3 HALT fault ngoai kich ban, dung chuoi de tranh boot-loop\r\n");
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }
    for (uint32_t step = s_first_step; step < W3E3_NUM_ASSIGNED_FAULTS; ++step) {
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
        const char *name = W3E3_ASSIGNED_FAULT_NAMES[step];
        print_step("ARM", step, name);
        uart1_flush();
        fault_postmortem_arm(step);
        const bool known = fault_inject_by_name(name);
        fault_postmortem_clear();
        print_step(known ? "NO_FAULT" : "UNKNOWN_NAME", step, name);
    }
    uart1_write_string("W3E3 DONE da chay het W3E3_ASSIGNED_FAULT_NAMES, nhan RESET de chay lai\r\n");
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

/* STEP 4: Đọc .noinit trước mọi thứ khác, in bản ghi, chọn bước kế tiếp, xóa bản ghi,
 * bật bẫy fault rồi khởi động bộ lập lịch. */
int main(void)
{
    fault_record_t record;
    const fault_pm_status_t status = fault_postmortem_inspect(&record);
    const uint32_t reset_flags = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;

    const bool uart_ready = uart1_init(LOG_BAUD);
    configASSERT(uart_ready);
    uart1_write_string("\r\n");
    print_reset_cause(reset_flags);

    if (status == FAULT_PM_CAPTURED) {
        fault_postmortem_print(&record);
        s_halt_sequence = record.fault_step >= W3E3_NUM_ASSIGNED_FAULTS;
        s_first_step = record.fault_step + 1u;
    } else if (status == FAULT_PM_ARMED_ONLY) {
        uart1_write_string("W3E3 PM armed nhung reset khong do handler, xem reset= o tren\r\n");
        s_halt_sequence = record.fault_step >= W3E3_NUM_ASSIGNED_FAULTS;
        s_first_step = record.fault_step + 1u;
    } else {
        uart1_write_string("W3E3 PM none: .noinit khong co ban ghi hop le (cold boot hoac bi khoi tao lai)\r\n");
        s_first_step = 0u;
    }
    fault_postmortem_clear();

    const bool null_guard = fault_postmortem_init();
    uart1_write_string("W3E3 TRAPS div0+unalign=1 disdefwbuf=1 null_guard=");
    uart1_write_u32(null_guard ? 1u : 0u);
    uart1_write_string(null_guard ? "\r\n" :
                       " (C_DEBUGEN=1 hoac DWT khong ho tro: rut nguon board sau khi nap)\r\n");
    if (!null_guard) {
        /* Chẩn đoán vì sao bộ canh NULL tắt: C_DEBUGEN = 1 là do debugger, còn NUMCOMP = 0
         * hoặc MASK0 đọc lại khác 10 là do lõi không hỗ trợ bộ so sánh như mong đợi. */
        uart1_write_string("W3E3 DWT c_debugen=");
        uart1_write_u32((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0u ? 1u : 0u);
        uart1_write_string(" numcomp=");
        uart1_write_u32((DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) >> DWT_CTRL_NUMCOMP_Pos);
        uart1_write_string(" mask0=");
        uart1_write_u32(DWT->MASK0);
        uart1_write_string("\r\n");
    }

    const TaskHandle_t task = xTaskCreateStatic(fault_task, "FaultGen", FAULT_STACK_WORDS, NULL,
                                                tskIDLE_PRIORITY + 1u, s_fault_stack, &s_fault_tcb);
    configASSERT(task != NULL);
    vTaskStartScheduler();
    configASSERT(0);
    return 0;
}
