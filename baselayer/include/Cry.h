/* Cry.h — Crypto Driver interface (ADR-005 / P5).
 *
 * Lowest layer of the AUTOSAR crypto stack.  Wraps the HSM callback
 * pointers from vecu_base_context_t into a C-callable API that CryIf
 * invokes.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CRY_H
#define CRY_H

#include "Std_Types.h"

/* ── Lifecycle ─────────────────────────────────────────────────────── */

void Cry_Init(void);
void Cry_DeInit(void);

/* ── Crypto primitives (delegates to ctx.hsm_*) ───────────────────── */

/** AES-128 encrypt (ECB / CBC).
 *  mode: 0 = ECB, 1 = CBC.
 *  iv may be NULL for ECB. */
Std_ReturnType Cry_Encrypt(uint32 keySlot, uint32 mode,
                           const uint8* data, uint32 dataLen,
                           const uint8* iv,
                           uint8* result, uint32* resultLen);

/** AES-128 decrypt (ECB / CBC). */
Std_ReturnType Cry_Decrypt(uint32 keySlot, uint32 mode,
                           const uint8* data, uint32 dataLen,
                           const uint8* iv,
                           uint8* result, uint32* resultLen);

/** Generate AES-128-CMAC. */
Std_ReturnType Cry_MacGenerate(uint32 keySlot,
                               const uint8* data, uint32 dataLen,
                               uint8* macPtr, uint32* macLenPtr);

/** Verify AES-128-CMAC. */
Std_ReturnType Cry_MacVerify(uint32 keySlot,
                             const uint8* data, uint32 dataLen,
                             const uint8* macPtr, uint32 macLen);

/** Generate a SecurityAccess seed. */
Std_ReturnType Cry_SeedGenerate(uint8* outSeed, uint32* outLen);

/** Validate a SecurityAccess key against the last seed. */
Std_ReturnType Cry_KeyValidate(const uint8* keyBuf, uint32 keyLen);

/** Generate cryptographically secure random bytes. */
Std_ReturnType Cry_RandomGenerate(uint8* outBuf, uint32 bufLen);

#endif /* CRY_H */
