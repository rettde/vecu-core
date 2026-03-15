/* Csm.h — Crypto Service Manager (ADR-005 / P5).
 *
 * Top-level AUTOSAR crypto API used by application / BSW modules.
 * Routes all operations through CryIf → Cry → HSM callbacks.
 *
 * jobId parameters are accepted for API compatibility but currently
 * ignored (single-job synchronous processing).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CSM_H
#define CSM_H

#include "Std_Types.h"

/* ── Lifecycle ─────────────────────────────────────────────────────── */

void Csm_Init(void);
void Csm_DeInit(void);
void Csm_MainFunction(void);

/* ── Crypto services ──────────────────────────────────────────────── */

/** AES-128 encrypt.
 *  mode: 0 = ECB, 1 = CBC.  iv may be NULL for ECB. */
Std_ReturnType Csm_Encrypt(uint32 jobId, uint32 mode,
                           const uint8* dataPtr, uint32 dataLen,
                           uint8* resultPtr, uint32* resultLenPtr);

/** AES-128 decrypt. */
Std_ReturnType Csm_Decrypt(uint32 jobId, uint32 mode,
                           const uint8* dataPtr, uint32 dataLen,
                           uint8* resultPtr, uint32* resultLenPtr);

/** Generate AES-128-CMAC. */
Std_ReturnType Csm_MacGenerate(uint32 jobId,
                               const uint8* dataPtr, uint32 dataLen,
                               uint8* macPtr, uint32* macLenPtr);

/** Verify AES-128-CMAC. */
Std_ReturnType Csm_MacVerify(uint32 jobId,
                             const uint8* dataPtr, uint32 dataLen,
                             const uint8* macPtr, uint32 macLen,
                             boolean* verifyPtr);

/** Generate cryptographically secure random bytes. */
Std_ReturnType Csm_RandomGenerate(uint32 jobId,
                                  uint8* resultPtr, uint32* resultLenPtr);

/* ── SecurityAccess helpers ───────────────────────────────────────── */

/** Generate a SecurityAccess seed (wraps HSM seed generation). */
Std_ReturnType Csm_SeedGenerate(uint8* outSeed, uint32* outLen);

/** Validate a SecurityAccess key against the last seed. */
Std_ReturnType Csm_KeyValidate(const uint8* keyBuf, uint32 keyLen);

#endif /* CSM_H */
