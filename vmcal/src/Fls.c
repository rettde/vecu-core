/* Fls.c — Virtual Flash Driver (ADR-002 / Virtual-MCAL).
 *
 * Shared-Memory-backed implementation. Uses vecu_base_context_t.shm_vars
 * as the backing store for flash read/write/erase operations.
 *
 * Async job model: Read/Write/Erase accept a job (set BUSY + PENDING),
 * Fls_MainFunction processes one pending job per call and fires the
 * jobEndNotification or jobErrorNotification callback (Fee-compatible).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Fls.h"
#include "VMcal_Context.h"
#include <string.h>
#include <stddef.h>

typedef enum {
    FLS_JOB_NONE  = 0u,
    FLS_JOB_READ  = 1u,
    FLS_JOB_WRITE = 2u,
    FLS_JOB_ERASE = 3u
} Fls_JobType;

typedef struct {
    Fls_JobType     type;
    Fls_AddressType address;
    Fls_LengthType  length;
    uint8*          read_dst;
    const uint8*    write_src;
} Fls_PendingJob;

static MemIf_StatusType    g_status     = MEMIF_UNINIT;
static MemIf_JobResultType g_job_result = MEMIF_JOB_OK;
static Fls_PendingJob      g_pending    = {FLS_JOB_NONE, 0, 0, NULL, NULL};

static Fls_JobEndNotificationType   g_job_end_notif   = NULL;
static Fls_JobErrorNotificationType g_job_error_notif = NULL;

void Fls_Init(const Fls_ConfigType* ConfigPtr) {
    g_status     = MEMIF_IDLE;
    g_job_result = MEMIF_JOB_OK;
    g_pending.type = FLS_JOB_NONE;
    if (ConfigPtr != NULL) {
        g_job_end_notif   = ConfigPtr->jobEndNotification;
        g_job_error_notif = ConfigPtr->jobErrorNotification;
    } else {
        g_job_end_notif   = NULL;
        g_job_error_notif = NULL;
    }
}

void Fls_DeInit(void) {
    g_status = MEMIF_UNINIT;
    g_pending.type = FLS_JOB_NONE;
}

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress,
                        uint8* TargetAddressPtr, Fls_LengthType Length) {
    if (g_status != MEMIF_IDLE || TargetAddressPtr == NULL) { return E_NOT_OK; }

    g_pending.type     = FLS_JOB_READ;
    g_pending.address  = SourceAddress;
    g_pending.length   = Length;
    g_pending.read_dst = TargetAddressPtr;
    g_status     = MEMIF_BUSY;
    g_job_result = MEMIF_JOB_PENDING;
    return E_OK;
}

Std_ReturnType Fls_Write(Fls_AddressType TargetAddress,
                         const uint8* SourceAddressPtr, Fls_LengthType Length) {
    if (g_status != MEMIF_IDLE || SourceAddressPtr == NULL) { return E_NOT_OK; }

    g_pending.type      = FLS_JOB_WRITE;
    g_pending.address   = TargetAddress;
    g_pending.length    = Length;
    g_pending.write_src = SourceAddressPtr;
    g_status     = MEMIF_BUSY;
    g_job_result = MEMIF_JOB_PENDING;
    return E_OK;
}

Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length) {
    if (g_status != MEMIF_IDLE) { return E_NOT_OK; }

    g_pending.type    = FLS_JOB_ERASE;
    g_pending.address = TargetAddress;
    g_pending.length  = Length;
    g_status     = MEMIF_BUSY;
    g_job_result = MEMIF_JOB_PENDING;
    return E_OK;
}

void Fls_Cancel(void) {
    if (g_status == MEMIF_BUSY) {
        g_status       = MEMIF_IDLE;
        g_job_result   = MEMIF_JOB_CANCELED;
        g_pending.type = FLS_JOB_NONE;
    }
}

MemIf_StatusType Fls_GetStatus(void) {
    return g_status;
}

MemIf_JobResultType Fls_GetJobResult(void) {
    return g_job_result;
}

void Fls_MainFunction(void) {
    if (g_status != MEMIF_BUSY || g_pending.type == FLS_JOB_NONE) { return; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    boolean success = FALSE;

    if (ctx != NULL && ctx->shm_vars != NULL &&
        g_pending.address + g_pending.length <= ctx->shm_vars_size) {

        switch (g_pending.type) {
            case FLS_JOB_READ: {
                const uint8* base = (const uint8*)ctx->shm_vars;
                memcpy(g_pending.read_dst, base + g_pending.address, g_pending.length);
                success = TRUE;
                break;
            }
            case FLS_JOB_WRITE: {
                uint8* base = (uint8*)ctx->shm_vars;
                memcpy(base + g_pending.address, g_pending.write_src, g_pending.length);
                success = TRUE;
                break;
            }
            case FLS_JOB_ERASE: {
                uint8* base = (uint8*)ctx->shm_vars;
                memset(base + g_pending.address, 0xFFu, g_pending.length);
                success = TRUE;
                break;
            }
            default:
                break;
        }
    }

    g_pending.type = FLS_JOB_NONE;
    g_status = MEMIF_IDLE;

    if (success) {
        g_job_result = MEMIF_JOB_OK;
        if (g_job_end_notif != NULL) {
            g_job_end_notif();
        }
    } else {
        g_job_result = MEMIF_JOB_FAILED;
        if (g_job_error_notif != NULL) {
            g_job_error_notif();
        }
    }
}
