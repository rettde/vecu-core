/* CryIf.h — Crypto Interface (ADR-005 / P5).
 *
 * Thin routing layer between Csm and the Cry driver.  In this minimal
 * implementation there is only one Cry driver, so all calls pass through
 * 1:1.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CRYIF_H
#define CRYIF_H

#include "Std_Types.h"

/* ── Lifecycle ─────────────────────────────────────────────────────── */

void CryIf_Init(void);
void CryIf_DeInit(void);

/* ── Crypto primitives (1:1 pass-through to Cry) ──────────────────── */

Std_ReturnType CryIf_Encrypt(uint32 keySlot, uint32 mode,
                             const uint8* data, uint32 dataLen,
                             const uint8* iv,
                             uint8* result, uint32* resultLen);

Std_ReturnType CryIf_Decrypt(uint32 keySlot, uint32 mode,
                             const uint8* data, uint32 dataLen,
                             const uint8* iv,
                             uint8* result, uint32* resultLen);

Std_ReturnType CryIf_MacGenerate(uint32 keySlot,
                                 const uint8* data, uint32 dataLen,
                                 uint8* macPtr, uint32* macLenPtr);

Std_ReturnType CryIf_MacVerify(uint32 keySlot,
                               const uint8* data, uint32 dataLen,
                               const uint8* macPtr, uint32 macLen);

Std_ReturnType CryIf_SeedGenerate(uint8* outSeed, uint32* outLen);

Std_ReturnType CryIf_KeyValidate(const uint8* keyBuf, uint32 keyLen);

Std_ReturnType CryIf_RandomGenerate(uint8* outBuf, uint32 bufLen);

#endif /* CRYIF_H */
