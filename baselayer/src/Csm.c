/* Csm.c — Crypto Service Manager implementation (ADR-005 / P5).
 *
 * Top-level AUTOSAR crypto API.  Application and BSW modules call these
 * functions; they route through CryIf → Cry → HSM callbacks.
 *
 * jobId is accepted for API compatibility but currently ignored
 * (single-job synchronous processing).
 *
 * Key slot convention: Csm uses key slot 0 (master key) by default.
 * The jobId could be used to select different key slots in a future
 * multi-job implementation.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Csm.h"
#include "CryIf.h"
#include <stddef.h>

/* Default key slot used for Csm operations. */
#define CSM_DEFAULT_KEY_SLOT  0u

static boolean g_initialized = FALSE;

/* ── Lifecycle ──────────────────────────────────────────────────── */

void Csm_Init(void) {
    g_initialized = TRUE;
}

void Csm_DeInit(void) {
    g_initialized = FALSE;
}

void Csm_MainFunction(void) {
    /* Synchronous processing — nothing to poll.
     * In a full AUTOSAR stack this would process the async job queue. */
    (void)0;
}

/* ── Crypto services ───────────────────────────────────────────── */

Std_ReturnType Csm_Encrypt(uint32 jobId, uint32 mode,
                           const uint8* dataPtr, uint32 dataLen,
                           uint8* resultPtr, uint32* resultLenPtr)
{
    (void)jobId;
    if (!g_initialized) { return E_NOT_OK; }
    if (dataPtr == NULL || resultPtr == NULL || resultLenPtr == NULL) {
        return E_NOT_OK;
    }
    return CryIf_Encrypt(CSM_DEFAULT_KEY_SLOT, mode,
                         dataPtr, dataLen, NULL,
                         resultPtr, resultLenPtr);
}

Std_ReturnType Csm_Decrypt(uint32 jobId, uint32 mode,
                           const uint8* dataPtr, uint32 dataLen,
                           uint8* resultPtr, uint32* resultLenPtr)
{
    (void)jobId;
    if (!g_initialized) { return E_NOT_OK; }
    if (dataPtr == NULL || resultPtr == NULL || resultLenPtr == NULL) {
        return E_NOT_OK;
    }
    return CryIf_Decrypt(CSM_DEFAULT_KEY_SLOT, mode,
                         dataPtr, dataLen, NULL,
                         resultPtr, resultLenPtr);
}

Std_ReturnType Csm_MacGenerate(uint32 jobId,
                               const uint8* dataPtr, uint32 dataLen,
                               uint8* macPtr, uint32* macLenPtr)
{
    (void)jobId;
    if (!g_initialized) { return E_NOT_OK; }
    if (dataPtr == NULL || macPtr == NULL || macLenPtr == NULL) {
        return E_NOT_OK;
    }
    return CryIf_MacGenerate(CSM_DEFAULT_KEY_SLOT,
                             dataPtr, dataLen, macPtr, macLenPtr);
}

Std_ReturnType Csm_MacVerify(uint32 jobId,
                             const uint8* dataPtr, uint32 dataLen,
                             const uint8* macPtr, uint32 macLen,
                             boolean* verifyPtr)
{
    (void)jobId;
    if (!g_initialized) { return E_NOT_OK; }
    if (dataPtr == NULL || macPtr == NULL || verifyPtr == NULL) {
        return E_NOT_OK;
    }
    Std_ReturnType rc = CryIf_MacVerify(CSM_DEFAULT_KEY_SLOT,
                                        dataPtr, dataLen, macPtr, macLen);
    *verifyPtr = (rc == E_OK) ? TRUE : FALSE;
    return E_OK;  /* MacVerify always returns E_OK; result is in verifyPtr */
}

Std_ReturnType Csm_RandomGenerate(uint32 jobId,
                                  uint8* resultPtr, uint32* resultLenPtr)
{
    (void)jobId;
    if (!g_initialized) { return E_NOT_OK; }
    if (resultPtr == NULL || resultLenPtr == NULL) {
        return E_NOT_OK;
    }
    return CryIf_RandomGenerate(resultPtr, *resultLenPtr);
}

/* ── SecurityAccess helpers ────────────────────────────────────── */

Std_ReturnType Csm_SeedGenerate(uint8* outSeed, uint32* outLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    if (outSeed == NULL || outLen == NULL) { return E_NOT_OK; }
    return CryIf_SeedGenerate(outSeed, outLen);
}

Std_ReturnType Csm_KeyValidate(const uint8* keyBuf, uint32 keyLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    if (keyBuf == NULL) { return E_NOT_OK; }
    return CryIf_KeyValidate(keyBuf, keyLen);
}
