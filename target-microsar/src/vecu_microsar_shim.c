/* vecu_microsar_shim.c — Vector MICROSAR integration shim for vecu-core.
 *
 * Implements Base_Init / Base_Step / Base_Shutdown by bridging to
 * Vector MICROSAR BSW modules (BswM, EcuM, SchM, Com, Dcm, NvM, …).
 *
 * The MCAL layer is bypassed: instead of real MCAL drivers, the
 * Virtual-MCAL (vmcal/) routes all I/O through vecu_base_context_t.
 *
 * Build with: -DMICROSAR_ROOT=/path/to/delivery -DVECU_MICROSAR=1
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu_microsar_shim.h"
#include "vecu_microsar_mcal_bridge.h"
#include "VMcal_Context.h"

#include <stddef.h>

static const vecu_base_context_t* g_ctx = NULL;
static uint64_t g_tick = 0;

static void log_msg(uint32_t level, const char* msg) {
    if (g_ctx != NULL && g_ctx->log_fn != NULL) {
        g_ctx->log_fn(level, msg);
    }
}

/* ── Extern declarations for MICROSAR BSW entry points ─────────────
 *
 * These functions are provided by the MICROSAR delivery.
 * They will be resolved at link time from the MICROSAR BSW sources.
 *
 * If a function is not available in your delivery, provide a stub
 * or remove the call below.
 */

/* EcuM — ECU State Manager */
extern void EcuM_Init(void);
extern void EcuM_MainFunction(void);
extern void EcuM_GoSleep(void);

/* BswM — BSW Mode Manager */
extern void BswM_Init(void);
extern void BswM_MainFunction(void);
extern void BswM_Deinit(void);

/* SchM — Schedule Manager (BSW MainFunction dispatcher) */
extern void SchM_Init(void);
extern void SchM_Deinit(void);

/* Com — AUTOSAR COM */
extern void Com_Init(void);
extern void Com_MainFunctionRx(void);
extern void Com_MainFunctionTx(void);
extern void Com_DeInit(void);

/* PduR — PDU Router */
extern void PduR_Init(void);

/* CanIf — CAN Interface */
extern void CanIf_Init(void);

/* Dcm — Diagnostic Communication Manager */
extern void Dcm_Init(void);
extern void Dcm_MainFunction(void);

/* Dem — Diagnostic Event Manager */
extern void Dem_Init(void);
extern void Dem_MainFunction(void);

/* NvM — NVRAM Manager */
extern void NvM_Init(void);
extern void NvM_MainFunction(void);

/* CanTp — CAN Transport Protocol */
extern void CanTp_Init(void);
extern void CanTp_MainFunction(void);

/* Csm — Crypto Service Manager */
extern void Csm_Init(void);
extern void Csm_MainFunction(void);

void Base_Init(const vecu_base_context_t* ctx) {
    g_ctx  = ctx;
    g_tick = 0;

    log_msg(2, "[MICROSAR] Base_Init: storing runtime context");

    VMcal_SetCtx(ctx);
    MCALBridge_Init(ctx);

    log_msg(2, "[MICROSAR] Base_Init: initializing MICROSAR BSW stack");

    EcuM_Init();
    SchM_Init();
    BswM_Init();
    PduR_Init();
    CanIf_Init();
    Com_Init();
    CanTp_Init();
    Dcm_Init();
    Dem_Init();
    NvM_Init();
    Csm_Init();

    log_msg(2, "[MICROSAR] Base_Init: BSW stack initialized");
}

void Base_Step(uint64_t tick) {
    if (g_ctx == NULL) { return; }
    g_tick = tick;

    MCALBridge_MainFunction();

    BswM_MainFunction();
    EcuM_MainFunction();

    Com_MainFunctionRx();
    CanTp_MainFunction();
    Dcm_MainFunction();
    Dem_MainFunction();
    NvM_MainFunction();
    Csm_MainFunction();
    Com_MainFunctionTx();
}

void Base_Shutdown(void) {
    if (g_ctx == NULL) { return; }

    log_msg(2, "[MICROSAR] Base_Shutdown: shutting down MICROSAR BSW stack");

    Com_DeInit();
    BswM_Deinit();
    SchM_Deinit();
    EcuM_GoSleep();

    log_msg(2, "[MICROSAR] Base_Shutdown: complete");

    g_ctx  = NULL;
    g_tick = 0;
}
