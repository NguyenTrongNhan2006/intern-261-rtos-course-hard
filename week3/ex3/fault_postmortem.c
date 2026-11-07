#include "fault_postmortem.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"

#include "params.h"
#include "stm32f1xx.h"
#include "uart.h"

#define SRAM_START            0x20000000u
#define SRAM_END              0x20005000u
#define FRAME_BYTES           32u
#define FILL_INVALID          0xDEADDEADu
#define STATE_ARMED           0xA5A50001u
#define STATE_CAPTURED        0xA5A50002u
#define EXC_RETURN_PSP        0x4u
#define CFSR_MMARVALID        (1u << 7)
#define CFSR_BFARVALID        (1u << 15)
#define NULL_GUARD_MASK_BITS  10u   /* bỏ qua 10 bit thấp: canh 0x000..0x3FF */
#define DWT_FUNC_RW_WATCH     0x7u  /* sự kiện debug khi đọc hoặc ghi địa chỉ khớp */
#define UART_DRAIN_LOOPS      200000u
#define TASK_NAME_MAX         15u

_Static_assert(sizeof(fault_record_t) % 4u == 0u, "Ban ghi phai chia het cho 4 byte");
_Static_assert(offsetof(fault_record_t, checksum) + 4u == sizeof(fault_record_t),
               "checksum phai la truong cuoi");

/* STEP 1: Bản ghi nằm trong .noinit. Linker script đặt section này NOLOAD sau .bss,
 * và Reset_Handler chỉ xóa _sbss.._ebss nên reset mềm không chạm vào bản ghi. */
static fault_record_t s_record __attribute__((section(".noinit"), aligned(4)));

void hardfault_c_handler(const uint32_t *sp, uint32_t exc_return) __attribute__((used, noreturn));
void debugmon_c_handler(const uint32_t *sp, uint32_t exc_return) __attribute__((used));

/* STEP 2: FNV-1a trên mọi byte đứng trước checksum. Bản ghi được xóa về 0 trước khi điền,
 * nên byte đệm cũng xác định và checksum không phụ thuộc rác SRAM. */
static uint32_t record_checksum(const fault_record_t *rec)
{
    const uint8_t *bytes = (const uint8_t *)rec;
    uint32_t hash = 2166136261u;
    for (size_t i = 0u; i < offsetof(fault_record_t, checksum); ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static void record_zero(void)
{
    uint32_t *words = (uint32_t *)&s_record;
    for (size_t i = 0u; i < sizeof(s_record) / 4u; ++i) {
        words[i] = 0u;
    }
}

static bool record_intact(void)
{
    return s_record.checksum == record_checksum(&s_record) &&
           (s_record.state == STATE_ARMED || s_record.state == STATE_CAPTURED);
}

void fault_postmortem_arm(uint32_t fault_step)
{
    record_zero();
    s_record.canary = W3E3_NOINIT_CANARY;
    s_record.fault_step = fault_step;
    s_record.state = STATE_ARMED;
    s_record.checksum = record_checksum(&s_record);
    __DSB();
}

void fault_postmortem_arm_canary(void)
{
    fault_postmortem_arm(FAULT_STEP_UNKNOWN);
}

void fault_postmortem_clear(void)
{
    record_zero();
    __DSB();
}

/* STEP 3: Lấy tên task chỉ khi fault xảy ra ở Thread mode dùng PSP, bộ lập lịch đã chạy
 * và có task hiện hành. Chép tối đa 15 ký tự, byte 16 luôn là '\0'. */
static void copy_task_name(uint32_t exc_return)
{
    const char *name = "ISR/BOOT";
    if ((exc_return & EXC_RETURN_PSP) != 0u &&
        xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        const TaskHandle_t current = xTaskGetCurrentTaskHandle();
        if (current != NULL) {
            name = pcTaskGetName(current);
        }
    }
    size_t i = 0u;
    while (name != NULL && i < TASK_NAME_MAX && name[i] != '\0') {
        s_record.task_name[i] = name[i];
        ++i;
    }
    s_record.task_name[i] = '\0';
}

/* STEP 4: Điền bản ghi. Chỉ đọc stacked frame khi con trỏ nằm trọn trong SRAM và căn
 * 4 byte, nếu không thì đọc sp hỏng sẽ gây BusFault lồng và Cortex-M3 rơi vào LOCKUP. */
static void capture(const uint32_t *frame, uint32_t exc_return, uint8_t source)
{
    const bool armed = record_intact() && s_record.state == STATE_ARMED;
    const uint32_t canary = armed ? s_record.canary : 0u;
    const uint32_t step = armed ? s_record.fault_step : FAULT_STEP_UNKNOWN;

    record_zero();
    s_record.canary = canary;
    s_record.fault_step = step;
    s_record.source = source;
    s_record.exc_return = exc_return;

    const uintptr_t sp = (uintptr_t)frame;
    const bool sp_valid = (sp & 0x3u) == 0u && sp >= SRAM_START && sp <= SRAM_END - FRAME_BYTES;
    s_record.sp_valid = sp_valid;
    s_record.stack_pointer = (uint32_t)sp;
    s_record.stacked_r0 = sp_valid ? frame[0] : FILL_INVALID;
    s_record.stacked_r1 = sp_valid ? frame[1] : FILL_INVALID;
    s_record.stacked_r2 = sp_valid ? frame[2] : FILL_INVALID;
    s_record.stacked_r3 = sp_valid ? frame[3] : FILL_INVALID;
    s_record.stacked_r12 = sp_valid ? frame[4] : FILL_INVALID;
    s_record.stacked_lr = sp_valid ? frame[5] : FILL_INVALID;
    s_record.stacked_pc = sp_valid ? frame[6] : FILL_INVALID;
    s_record.stacked_xpsr = sp_valid ? frame[7] : FILL_INVALID;

    /* MMFAR/BFAR chỉ có nghĩa khi bit VALID tương ứng trong CFSR bằng 1. */
    const uint32_t cfsr = SCB->CFSR;
    const uint32_t hfsr = SCB->HFSR;
    const uint32_t dfsr = SCB->DFSR;
    s_record.cfsr = cfsr;
    s_record.hfsr = hfsr;
    s_record.dfsr = dfsr;
    s_record.mmfar_valid = (cfsr & CFSR_MMARVALID) != 0u;
    s_record.bfar_valid = (cfsr & CFSR_BFARVALID) != 0u;
    s_record.mmfar = s_record.mmfar_valid ? SCB->MMFAR : 0u;
    s_record.bfar = s_record.bfar_valid ? SCB->BFAR : 0u;

    copy_task_name(exc_return);
    s_record.state = STATE_CAPTURED;
    s_record.checksum = record_checksum(&s_record);

    /* Xóa cờ dính sau khi đã lưu (ghi 1 để xóa). */
    SCB->CFSR = cfsr;
    SCB->HFSR = hfsr;
    SCB->DFSR = dfsr;
}

/* STEP 5: Reset có kiểm soát. Chờ có giới hạn cho UART1 phát xong byte dở, DSB để mọi
 * lệnh ghi vào .noinit đã tới SRAM, rồi NVIC_SystemReset. Chọn reset thay vì treo để
 * bản ghi được in ngay ở lần chạy sau và chuỗi kịch bản tự tiếp tục. */
static void reset_after_capture(void) __attribute__((noreturn));
static void reset_after_capture(void)
{
    if ((USART1->CR1 & USART_CR1_UE) != 0u) {
        for (uint32_t i = 0u; i < UART_DRAIN_LOOPS && (USART1->SR & USART_SR_TC) == 0u; ++i) {
        }
    }
    __DSB();
    NVIC_SystemReset();
}

void hardfault_c_handler(const uint32_t *sp, uint32_t exc_return)
{
    capture(sp, exc_return, FAULT_SOURCE_HARDFAULT);
    reset_after_capture();
}

/* Cấu hình DWT và DEMCR thuộc miền debug, có thể còn nguyên sau NVIC_SystemReset. VTOR thì
 * luôn về 0 khi reset và chỉ fault_postmortem_init() đặt lại 0x08000000. Nếu VTOR khác thì
 * sự kiện là của bộ canh còn sót từ lần chạy trước: tắt bộ canh và trả về bình thường (asm
 * dùng lệnh b nên LR vẫn là EXC_RETURN), không ghi bản ghi để tránh vòng lặp reset. */
void debugmon_c_handler(const uint32_t *sp, uint32_t exc_return)
{
    if (SCB->VTOR != FLASH_BASE) {
        DWT->FUNCTION0 = 0u;
        CoreDebug->DEMCR &= ~CoreDebug_DEMCR_MON_EN_Msk;
        SCB->DFSR = SCB->DFSR;
        __DSB();
        return;
    }
    capture(sp, exc_return, FAULT_SOURCE_DEBUGMON);
    reset_after_capture();
}

void fault_postmortem_capture(const uint32_t *stacked_frame)
{
    /* Giao diện của đề không truyền EXC_RETURN, nên ghi 0 (không rõ). */
    capture(stacked_frame, 0u, FAULT_SOURCE_HARDFAULT);
    reset_after_capture();
}

/* STEP 6: Handler naked chỉ có hợp ngữ. Bit 2 của EXC_RETURN chọn stack đang dùng:
 * 0 là MSP (fault trong Handler mode hoặc trước khi chạy task), 1 là PSP (fault trong task).
 * r0 = con trỏ stacked frame, r1 = EXC_RETURN, rồi nhảy sang hàm C. */
__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst   lr, #4              \n"
        "ite   eq                  \n"
        "mrseq r0, msp             \n"
        "mrsne r0, psp             \n"
        "mov   r1, lr              \n"
        "b     hardfault_c_handler \n");
}

__attribute__((naked)) void DebugMon_Handler(void)
{
    __asm volatile(
        "tst   lr, #4              \n"
        "ite   eq                  \n"
        "mrseq r0, msp             \n"
        "mrsne r0, psp             \n"
        "mov   r1, lr              \n"
        "b     debugmon_c_handler  \n");
}

/* STEP 7: Bật bẫy phần cứng. VTOR trỏ thẳng 0x08000000 để CPU không đọc bảng vector
 * qua bí danh 0x0. STM32F103 không có MPU và 0x0 là bí danh Flash, nên đọc NULL không
 * gây fault: bộ so sánh DWT 0 canh 0x000..0x3FF và sinh DebugMonitor khi có truy cập.
 * Khi debugger đang bật halting debug (C_DEBUGEN = 1), sự kiện DWT sẽ dừng CPU thay vì
 * vào DebugMonitor, nên chỉ bật bộ canh khi C_DEBUGEN = 0. */
bool fault_postmortem_init(void)
{
    SCB->VTOR = FLASH_BASE;
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk | SCB_CCR_UNALIGN_TRP_Msk;
    SCnSCB->ACTLR |= SCnSCB_ACTLR_DISDEFWBUF_Msk;
    __DSB();
    __ISB();

    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0u) {
        return false;
    }
    /* TRCENA phải bật trước khi đọc hoặc ghi thanh ghi DWT. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    __DSB();
    if ((DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) == 0u) {
        return false;
    }
    DWT->FUNCTION0 = 0u;
    DWT->COMP0 = 0u;
    DWT->MASK0 = NULL_GUARD_MASK_BITS;
    if (DWT->MASK0 != NULL_GUARD_MASK_BITS) {
        return false;
    }
    NVIC_SetPriority(DebugMonitor_IRQn, 0u);
    DWT->FUNCTION0 = DWT_FUNC_RW_WATCH;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_MON_EN_Msk;
    __DSB();
    __ISB();
    return true;
}

fault_pm_status_t fault_postmortem_inspect(fault_record_t *out)
{
    if (!record_intact()) {
        return FAULT_PM_NONE;
    }
    *out = s_record;
    return s_record.state == STATE_CAPTURED ? FAULT_PM_CAPTURED : FAULT_PM_ARMED_ONLY;
}

bool fault_postmortem_check_and_read(fault_record_t *out)
{
    fault_record_t rec;
    if (fault_postmortem_inspect(&rec) != FAULT_PM_CAPTURED || rec.canary != W3E3_NOINIT_CANARY) {
        return false;
    }
    *out = rec;
    return true;
}

/* STEP 8: In bản ghi. Mỗi dòng có tiền tố "W3E3 PM" để lọc log. */
static void write_hex32(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    uart1_write_string("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart1_write_char(digits[(value >> (uint32_t)shift) & 0xFu]);
    }
}

static void write_field(const char *label, uint32_t value)
{
    uart1_write_string(label);
    write_hex32(value);
}

typedef struct {
    uint32_t mask;
    const char *name;
} flag_name_t;

static void write_flags(uint32_t value, const flag_name_t *flags, size_t count)
{
    uart1_write_string(" [");
    bool first = true;
    for (size_t i = 0u; i < count; ++i) {
        if ((value & flags[i].mask) != 0u) {
            if (!first) {
                uart1_write_char(' ');
            }
            uart1_write_string(flags[i].name);
            first = false;
        }
    }
    uart1_write_string(first ? "-]" : "]");
}

static const flag_name_t s_cfsr_flags[] = {
    {1u << 0, "IACCVIOL"},   {1u << 1, "DACCVIOL"},    {1u << 3, "MUNSTKERR"},
    {1u << 4, "MSTKERR"},    {1u << 7, "MMARVALID"},   {1u << 8, "IBUSERR"},
    {1u << 9, "PRECISERR"},  {1u << 10, "IMPRECISERR"}, {1u << 11, "UNSTKERR"},
    {1u << 12, "STKERR"},    {1u << 15, "BFARVALID"},  {1u << 16, "UNDEFINSTR"},
    {1u << 17, "INVSTATE"},  {1u << 18, "INVPC"},      {1u << 19, "NOCP"},
    {1u << 24, "UNALIGNED"}, {1u << 25, "DIVBYZERO"},
};
static const flag_name_t s_hfsr_flags[] = {
    {1u << 1, "VECTTBL"}, {1u << 30, "FORCED"}, {1u << 31, "DEBUGEVT"},
};
static const flag_name_t s_dfsr_flags[] = {
    {1u << 0, "HALTED"}, {1u << 1, "BKPT"}, {1u << 2, "DWTTRAP"},
    {1u << 3, "VCATCH"}, {1u << 4, "EXTERNAL"},
};

void fault_postmortem_print(const fault_record_t *rec)
{
    const bool known_step = rec->fault_step < W3E3_NUM_ASSIGNED_FAULTS;

    uart1_write_string("W3E3 PM step=");
    if (known_step) {
        uart1_write_u32(rec->fault_step);
    } else {
        uart1_write_string("?");
    }
    uart1_write_string(" fault=");
    uart1_write_string(known_step ? W3E3_ASSIGNED_FAULT_NAMES[rec->fault_step] : "UNEXPECTED");
    uart1_write_string(rec->source == FAULT_SOURCE_DEBUGMON ? " source=DebugMon(DWT)" : " source=HardFault");
    write_field(" canary=", rec->canary);
    uart1_write_string(rec->canary == W3E3_NOINIT_CANARY ? " MATCH" : " MISMATCH");
    uart1_write_string(" task=");
    uart1_write_string(rec->task_name);
    uart1_write_string("\r\n");

    write_field("W3E3 PM pc=", rec->stacked_pc);
    write_field(" lr=", rec->stacked_lr);
    write_field(" xpsr=", rec->stacked_xpsr);
    write_field(" exc_return=", rec->exc_return);
    write_field(" sp=", rec->stack_pointer);
    uart1_write_string(rec->sp_valid ? " sp_valid=1" : " sp_valid=0");
    uart1_write_string("\r\n");

    write_field("W3E3 PM r0=", rec->stacked_r0);
    write_field(" r1=", rec->stacked_r1);
    write_field(" r2=", rec->stacked_r2);
    write_field(" r3=", rec->stacked_r3);
    write_field(" r12=", rec->stacked_r12);
    uart1_write_string("\r\n");

    write_field("W3E3 PM cfsr=", rec->cfsr);
    write_flags(rec->cfsr, s_cfsr_flags, sizeof(s_cfsr_flags) / sizeof(s_cfsr_flags[0]));
    write_field(" hfsr=", rec->hfsr);
    write_flags(rec->hfsr, s_hfsr_flags, sizeof(s_hfsr_flags) / sizeof(s_hfsr_flags[0]));
    write_field(" dfsr=", rec->dfsr);
    write_flags(rec->dfsr, s_dfsr_flags, sizeof(s_dfsr_flags) / sizeof(s_dfsr_flags[0]));
    uart1_write_string("\r\n");

    write_field("W3E3 PM bfar=", rec->bfar);
    uart1_write_string(rec->bfar_valid ? " (valid)" : " (invalid)");
    write_field(" mmfar=", rec->mmfar);
    uart1_write_string(rec->mmfar_valid ? " (valid)" : " (invalid)");
    uart1_write_string("\r\n");

    /* LR là địa chỉ quay về sau lệnh gọi: lùi 2 byte để rơi vào chính lệnh BL/BLX. */
    uart1_write_string("W3E3 PM addr2line: arm-none-eabi-addr2line -e build/w3e3/w3e3.elf -f -p -a ");
    write_hex32(rec->stacked_pc);
    uart1_write_char(' ');
    write_hex32((rec->stacked_lr & ~1u) - 2u);
    uart1_write_string("\r\n");
}
