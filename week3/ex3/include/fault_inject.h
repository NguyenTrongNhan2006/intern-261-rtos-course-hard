#ifndef W3E3_FAULT_INJECT_H
#define W3E3_FAULT_INJECT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* STEP 1: Gây đúng loại fault có tên trong W3E3_ASSIGNED_FAULT_NAMES.
 * Trả false nếu tên không có trong bảng. Trả true nếu đã thực hiện lệnh gây lỗi
 * mà CPU vẫn chạy tiếp, tức là phần cứng không sinh fault. */
bool fault_inject_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* W3E3_FAULT_INJECT_H */
