/* Fls.c — Virtual Flash Driver (ADR-002 / Virtual-MCAL).
 *
 * Shared-Memory-backed implementation. Uses vecu_base_context_t.shm_vars
 * as the backing store for flash read/write/erase operations.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Fls.h"
#include "VMcal_Context.h"
#include <string.h>
#include <stddef.h>

static MemIf_StatusType    g_status     = MEMIF_UNINIT;
static MemIf_JobResultType g_job_result = MEMIF_JOB_OK;

void Fls_Init(const Fls_ConfigType* ConfigPtr) {
    (void)ConfigPtr;
    g_status     = MEMIF_IDLE;
    g_job_result = MEMIF_JOB_OK;
}

void Fls_DeInit(void) {
    g_status = MEMIF_UNINIT;
}

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress,
                        uint8* TargetAddressPtr, Fls_LengthType Length) {
    if (g_status == MEMIF_UNINIT || TargetAddressPtr == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->shm_vars == NULL) { return E_NOT_OK; }
    if (SourceAddress + Length > ctx->shm_vars_size) {
        g_job_result = MEMIF_JOB_FAILED;
        return E_NOT_OK;
    }

    const uint8* base = (const uint8*)ctx->shm_vars;
    memcpy(TargetAddressPtr, base + SourceAddress, Length);
    g_job_result = MEMIF_JOB_OK;
    return E_OK;
}

Std_ReturnType Fls_Write(Fls_AddressType TargetAddress,
                         const uint8* SourceAddressPtr, Fls_LengthType Length) {
    if (g_status == MEMIF_UNINIT || SourceAddressPtr == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->shm_vars == NULL) { return E_NOT_OK; }
    if (TargetAddress + Length > ctx->shm_vars_size) {
        g_job_result = MEMIF_JOB_FAILED;
        return E_NOT_OK;
    }

    uint8* base = (uint8*)ctx->shm_vars;
    memcpy(base + TargetAddress, SourceAddressPtr, Length);
    g_job_result = MEMIF_JOB_OK;
    return E_OK;
}

Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length) {
    if (g_status == MEMIF_UNINIT) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->shm_vars == NULL) { return E_NOT_OK; }
    if (TargetAddress + Length > ctx->shm_vars_size) {
        g_job_result = MEMIF_JOB_FAILED;
        return E_NOT_OK;
    }

    uint8* base = (uint8*)ctx->shm_vars;
    memset(base + TargetAddress, 0xFF, Length);
    g_job_result = MEMIF_JOB_OK;
    return E_OK;
}

MemIf_StatusType Fls_GetStatus(void) {
    return g_status;
}

MemIf_JobResultType Fls_GetJobResult(void) {
    return g_job_result;
}

void Fls_MainFunction(void) {
    if (g_status == MEMIF_UNINIT) { return; }
}
