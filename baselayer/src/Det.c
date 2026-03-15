/* Det.c — Default Error Tracer implementation (ADR-005 / P3).
 *
 * Forwards errors to ctx.log_fn and counts them.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Det.h"
#include <stdio.h>

/* Forward declaration — log function pointer is stored by Base_Entry.c. */
extern void (*Base_GetLogFn(void))(uint32_t level, const char* msg);

static uint32_t g_error_count = 0;

void Det_Init(void) {
    g_error_count = 0;
}

Std_ReturnType Det_ReportError(uint16_t ModuleId,
                               uint8_t  InstanceId,
                               uint8_t  ApiId,
                               uint8_t  ErrorId) {
    g_error_count++;

    /* Format error message. */
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Det: Module=%u Inst=%u Api=%u Err=%u",
             (unsigned)ModuleId, (unsigned)InstanceId,
             (unsigned)ApiId, (unsigned)ErrorId);

    /* Try to forward to the Rust log callback.
     * Level 4 = error in our convention. */
    void (*log_fn)(uint32_t, const char*) = Base_GetLogFn();
    if (log_fn != NULL_PTR) {
        log_fn(4, buf);
    }

    return E_OK;
}

uint32_t Det_GetErrorCount(void) {
    return g_error_count;
}
