/* vecu_microsar_bridge.c — vECU plugin bridge for real MICROSAR BSW.
 *
 * Maps the vecu-core lifecycle (Base_Init / Base_Step / Base_Shutdown) to
 * the MICROSAR BSW lifecycle (EcuM_Init / EcuM_StartupTwo / EcuM_GoDown).
 *
 * This file replaces the stub baselayer's Base_Entry.c when building with
 * real Vector MICROSAR sources.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <stddef.h>
#include "vecu_base_context.h"
#include "vecu_bsw_scheduler.h"

#ifdef VECU_BUILD

#include "VMcal_Context.h"

/* Forward declarations for MICROSAR BSW entry points.
 * These are provided by the real MICROSAR EcuM, SchM, and BswM sources. */
extern void EcuM_Init(void);
extern void EcuM_StartupTwo(void);
extern void BswM_Deinit(void);

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

    if (ctx->log_fn != NULL) {
        ctx->log_fn(2u, "vecu_microsar_bridge: Base_Init starting");
    }

    /* Wire up the Virtual-MCAL context so all vmcal modules can access
     * the push_tx_frame / pop_rx_frame / hsm / shm callbacks. */
    VMcal_Init(ctx);

    /* MICROSAR BSW initialization sequence:
     * EcuM_Init  → DriverInitZero (Det_Init, *_InitMemory)
     *             → DriverInitOne  (Mcu, Port, Can, Eth, ... _Init)
     *             → EcuM_StartOS   (no-op stub on vECU)
     *
     * EcuM_StartupTwo → SchM_Init → BswM_Init → state = RUN
     *
     * On the real target the OS calls EcuM_StartupTwo from StartupHook.
     * On vECU we call it explicitly after EcuM_Init returns. */
    EcuM_Init();
    EcuM_StartupTwo();
    VecuBswScheduler_Init();
    if (ctx->log_fn != NULL) {
        ctx->log_fn(2u, "vecu_microsar_bridge: Base_Init complete");
    }
}

EXPORT void Base_Step(uint64_t tick) {
    /* In the real target, the OS runs cyclic tasks (alarms) that call BSW
     * MainFunctions at configured intervals (5 ms / 10 ms).  The vECU BSW
     * scheduler replicates the Core0 Rte task dispatch pattern extracted
     * from the generated Rte_OS_Application_Core0_QM.c. */
    VecuBswScheduler_Step(tick);
}

EXPORT void Base_Shutdown(void) {
    if (g_ctx != NULL && g_ctx->log_fn != NULL) {
        g_ctx->log_fn(2u, "vecu_microsar_bridge: Base_Shutdown");
    }
    BswM_Deinit();
    g_ctx = NULL;
}

EXPORT void Appl_Init(void) {
}

EXPORT void Appl_MainFunction(void) {
}

EXPORT void Appl_Shutdown(void) {
}

#endif /* VECU_BUILD */
