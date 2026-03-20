/* vecu_det_logging.c — Det callout hooks forwarding to vecu log_fn.
 *
 * The MICROSAR Det module calls these hooks for every Det_ReportError,
 * Det_ReportRuntimeError, and Det_ReportTransientFault.  We format the
 * error information and forward it to the Rust tracing bridge via the
 * log_fn callback stored in vecu_base_context_t.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Std_Types.h"
#include <stdio.h>

extern void (*Base_GetLogFn(void))(uint32 level, const char* msg);

Std_ReturnType CddDetLogging_CalloutErrorHook(uint16 ModuleId,
                                               uint8  InstanceId,
                                               uint8  ApiId,
                                               uint8  ErrorId)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Det ERROR  Module=%u Inst=%u Api=%u Err=%u",
             (unsigned)ModuleId, (unsigned)InstanceId,
             (unsigned)ApiId, (unsigned)ErrorId);

    void (*log_fn)(uint32, const char*) = Base_GetLogFn();
    if (log_fn != NULL_PTR) {
        log_fn(3u, buf);
    }
    return E_OK;
}

Std_ReturnType CddDetLogging_CalloutRuntimeError(uint16 ModuleId,
                                                   uint8  InstanceId,
                                                   uint8  ApiId,
                                                   uint8  ErrorId)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Det RUNTIME Module=%u Inst=%u Api=%u Err=%u",
             (unsigned)ModuleId, (unsigned)InstanceId,
             (unsigned)ApiId, (unsigned)ErrorId);

    void (*log_fn)(uint32, const char*) = Base_GetLogFn();
    if (log_fn != NULL_PTR) {
        log_fn(4u, buf);
    }
    return E_OK;
}

Std_ReturnType CddDetLogging_CalloutTransientFault(uint16 ModuleId,
                                                     uint8  InstanceId,
                                                     uint8  ApiId,
                                                     uint8  ErrorId)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Det TRANSIENT Module=%u Inst=%u Api=%u Err=%u",
             (unsigned)ModuleId, (unsigned)InstanceId,
             (unsigned)ApiId, (unsigned)ErrorId);

    void (*log_fn)(uint32, const char*) = Base_GetLogFn();
    if (log_fn != NULL_PTR) {
        log_fn(4u, buf);
    }
    return E_OK;
}
