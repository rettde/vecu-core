/* vecu_microsar_bridge.c — vECU plugin bridge for real MICROSAR BSW.
 *
 * Maps the vecu-core lifecycle (Base_Init / Base_Step / Base_Shutdown) to
 * the MICROSAR BSW lifecycle (EcuM_Init / SchM_MainFunction / EcuM_GoSleep).
 *
 * This file replaces the stub baselayer's Base_Entry.c when building with
 * real Vector MICROSAR sources.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <stddef.h>
#include "vecu_base_context.h"

#ifdef VECU_BUILD

#include "VMcal_Context.h"

/* Forward declarations for MICROSAR BSW entry points.
 * These are provided by the real MICROSAR EcuM, SchM, and BswM sources. */
extern void EcuM_Init(void);
extern void EcuM_MainFunction(void);
extern void SchM_Init(void);
extern void BswM_Init(void* ConfigPtr);
extern void EcuM_GoSleep(void);
extern void EcuM_Shutdown(void);

static const vecu_base_context_t* g_ctx = NULL;

const vecu_base_context_t* Base_GetCtx(void) {
    return g_ctx;
}

void (*Base_GetLogFn(void))(uint32_t level, const char* msg) {
    if (g_ctx != NULL && g_ctx->log_fn != NULL) {
        return g_ctx->log_fn;
    }
    return NULL;
}

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Base_Init(const vecu_base_context_t* ctx) {
    g_ctx = ctx;

    /* Wire up the Virtual-MCAL context so all vmcal modules can access
     * the push_tx_frame / pop_rx_frame / hsm / shm callbacks. */
    VMcal_Init(ctx);

    /* MICROSAR BSW initialization sequence:
     * EcuM_Init → Os_Init → SchM_Init → BswM_Init → all BSW _Init calls
     *
     * In the real target, EcuM_Init calls StartupHook which triggers the
     * full BSW initialization chain via BswM rules.  For vECU we call
     * the same entry point — the GenData configuration drives the rest. */
    EcuM_Init();
}

EXPORT void Base_Step(uint64_t tick) {
    (void)tick;

    /* In the real target, the OS timer ISR triggers SchM partitions which
     * call the BSW MainFunctions.  For vECU we call EcuM_MainFunction
     * which dispatches to SchM → BswM → all module MainFunctions.
     *
     * The tick value is available via the context but MICROSAR uses its
     * own OS counter incremented by the OS-Mapping layer. */
    EcuM_MainFunction();
}

EXPORT void Base_Shutdown(void) {
    EcuM_GoSleep();
    EcuM_Shutdown();
    g_ctx = NULL;
}

#endif /* VECU_BUILD */
