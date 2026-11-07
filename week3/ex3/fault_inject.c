#include "fault_inject.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* STEP 1: Địa chỉ đích nằm trong biến volatile, nên GCC không biết trước giá trị là NULL
 * hay lệch căn, không báo lỗi biên dịch (-Werror) và không xóa lệnh như hành vi không
 * xác định. Mỗi hàm gây lỗi là noinline để addr2line trả đúng tên hàm và dòng. */
static volatile uintptr_t s_null_address = 0u;
static volatile uintptr_t s_jump_address;
static volatile uint32_t s_sink;
static uint32_t s_words[2];

/* Đích nhảy hợp lệ, nhưng được gọi với bit Thumb = 0 nên không bao giờ thực thi. */
__attribute__((noinline)) static void fault_jump_landing(void)
{
    s_sink = 0x1A2B3C4Du;
}

/* STEP 2: NULL_DEREF_WRITE. STR vào 0x0 là bí danh Flash khi chưa bật lập trình. */
__attribute__((noinline)) static void inject_null_write(void)
{
    volatile uint32_t *const target = (volatile uint32_t *)s_null_address;
    *target = 0xDEADBEEFu;
}

/* STEP 3: NULL_DEREF_READ. LDR từ 0x0 đọc được MSP ban đầu nên không tự gây fault,
 * chỉ bộ canh DWT bắt được. */
__attribute__((noinline)) static void inject_null_read(void)
{
    const volatile uint32_t *const source = (const volatile uint32_t *)s_null_address;
    s_sink = *source;
}

/* STEP 4: INVALID_PC_JUMP. BLX tới địa chỉ có bit 0 = 0 xóa EPSR.T, lệnh kế tiếp gây
 * UsageFault INVSTATE. Lệnh ghi s_sink sau lời gọi chặn tối ưu tail-call, để LR trỏ về đây. */
__attribute__((noinline)) static void inject_invalid_pc(void)
{
    s_jump_address = (uintptr_t)&fault_jump_landing & ~(uintptr_t)1u;
    void (*const jump)(void) = (void (*)(void))s_jump_address;
    jump();
    s_sink = 0u;
}

/* STEP 5: UNALIGNED_WORD_ACCESS. LDR một word ở địa chỉ lẻ, CCR.UNALIGN_TRP biến nó
 * thành UsageFault UNALIGNED thay vì để phần cứng tự tách truy cập. */
__attribute__((noinline)) static void inject_unaligned_word(void)
{
    const volatile uintptr_t address = (uintptr_t)&s_words[0] + 1u;
    s_sink = *(const volatile uint32_t *)address;
}

typedef struct {
    const char *name;
    void (*inject)(void);
} fault_entry_t;

static const fault_entry_t s_faults[] = {
    {"NULL_DEREF_WRITE", inject_null_write},
    {"NULL_DEREF_READ", inject_null_read},
    {"INVALID_PC_JUMP", inject_invalid_pc},
    {"UNALIGNED_WORD_ACCESS", inject_unaligned_word},
};

/* STEP 6: Tra theo tên trong params.h thay vì theo chỉ số, nên thứ tự tham số đổi
 * vẫn chạy đúng hàm, và tên lạ được báo ra thay vì gây lỗi sai loại. */
bool fault_inject_by_name(const char *name)
{
    for (size_t i = 0u; i < sizeof(s_faults) / sizeof(s_faults[0]); ++i) {
        if (strcmp(name, s_faults[i].name) == 0) {
            s_faults[i].inject();
            return true;
        }
    }
    return false;
}
