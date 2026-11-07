#ifndef W3E3_FAULT_POSTMORTEM_H
#define W3E3_FAULT_POSTMORTEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* STEP 1: Bản ghi post-mortem nằm trong .noinit, phải giữ nguyên qua reset do fault.
 * Phần đầu giữ đúng các trường của đề. Phần mở rộng thêm stacked frame đủ 8 thanh ghi,
 * EXC_RETURN, DFSR, bước kịch bản, trạng thái và checksum để phân biệt dữ liệu thật với
 * SRAM rác lúc cấp nguồn. checksum luôn là trường cuối. */
typedef struct {
    uint32_t canary;       /* W3E3_NOINIT_CANARY ghi ngay trước khi cố ý gây fault */
    uint32_t stacked_pc;
    uint32_t stacked_lr;
    uint32_t stacked_xpsr;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
    uint32_t mmfar;
    bool     bfar_valid;   /* SCB->CFSR.BFARVALID */
    bool     mmfar_valid;  /* SCB->CFSR.MMARVALID */
    char     task_name[16];
    bool     sp_valid;     /* stacked frame nằm trọn trong SRAM và căn 4 byte */
    uint8_t  source;       /* fault_source_t */
    uint32_t stacked_r0;
    uint32_t stacked_r1;
    uint32_t stacked_r2;
    uint32_t stacked_r3;
    uint32_t stacked_r12;
    uint32_t exc_return;
    uint32_t stack_pointer;
    uint32_t dfsr;         /* SCB->DFSR, cho đường bắt NULL bằng DWT + DebugMonitor */
    uint32_t fault_step;   /* chỉ số trong W3E3_ASSIGNED_FAULT_NAMES */
    uint32_t state;
    uint32_t checksum;
} fault_record_t;

typedef enum {
    FAULT_SOURCE_NONE = 0,
    FAULT_SOURCE_HARDFAULT = 1,
    FAULT_SOURCE_DEBUGMON = 2
} fault_source_t;

typedef enum {
    FAULT_PM_NONE = 0,        /* cold boot hoặc checksum sai: .noinit không có bản ghi */
    FAULT_PM_ARMED_ONLY = 1,  /* đã ghi canary nhưng reset không do handler */
    FAULT_PM_CAPTURED = 2     /* handler đã điền bản ghi */
} fault_pm_status_t;

#define FAULT_STEP_UNKNOWN 0xFFFFFFFFu

/* STEP 2: Bật bẫy chia 0, truy cập lệch, tắt write buffer để BusFault chính xác và
 * bật bộ canh NULL bằng DWT. Trả true nếu bộ canh NULL hoạt động. */
bool fault_postmortem_init(void);

/* Điền bản ghi từ stacked frame rồi reset có kiểm soát. Không trả về. */
void fault_postmortem_capture(const uint32_t *stacked_frame);

/* Gọi đầu main(): true nếu bản ghi do handler điền, checksum đúng và canary khớp
 * W3E3_NOINIT_CANARY. */
bool fault_postmortem_check_and_read(fault_record_t *out);

/* Đọc trạng thái bản ghi .noinit, kể cả khi canary không khớp. */
fault_pm_status_t fault_postmortem_inspect(fault_record_t *out);

/* In bản ghi qua UART1, kèm giải mã cờ và lệnh addr2line. */
void fault_postmortem_print(const fault_record_t *rec);

/* Ghi canary, bước kịch bản và checksum ngay trước khi cố ý gây fault. */
void fault_postmortem_arm(uint32_t fault_step);
void fault_postmortem_arm_canary(void);

/* Xóa bản ghi sau khi đã in, để reset tay kế tiếp không đọc lại bản ghi cũ. */
void fault_postmortem_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* W3E3_FAULT_POSTMORTEM_H */
