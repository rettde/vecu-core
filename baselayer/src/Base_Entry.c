/* Base_Entry.c — BaseLayer entry points (ADR-005 / P3).
 *
 * Implements the three mandatory exports:
 *   Base_Init(ctx)   — stores context, initialises EcuM
 *   Base_Step(tick)   — advances Os tick, calls EcuM_MainFunction
 *   Base_Shutdown()   — calls EcuM_GoSleep, invalidates context
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <stddef.h>
#include "vecu_base_context.h"
#include "EcuM.h"
#include "Os.h"

/* ── Stored context ─────────────────────────────────────────────── */

static const vecu_base_context_t* g_ctx = NULL;

/* ── Accessor for Det.c ─────────────────────────────────────────── */

/* Returns the log function pointer from the stored context.
 * Det.c calls this to forward errors to the Rust bridge. */
void (*Base_GetLogFn(void))(uint32_t level, const char* msg) {
    if (g_ctx != NULL && g_ctx->log_fn != NULL) {
        return g_ctx->log_fn;
    }
    return NULL;
}

/* ── Platform export macro ──────────────────────────────────────── */

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

/* ── Mandatory exports ──────────────────────────────────────────── */

EXPORT void Base_Init(const vecu_base_context_t* ctx) {
    g_ctx = ctx;
    EcuM_Init();
}

EXPORT void Base_Step(uint64_t tick) {
    Os_SetTick(tick);
    EcuM_MainFunction();
}

EXPORT void Base_Shutdown(void) {
    EcuM_GoSleep();
    g_ctx = NULL;
}
