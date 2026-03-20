/* Crypto_30_vHsm.h — Virtual HSM Adapter API (ADR-003).
 *
 * Crypto_30_vHsm-compatible adapter layer that delegates cryptographic
 * operations to the vecu-core HSM module via vecu_base_context_t callbacks.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CRYPTO_30_VHSM_H
#define CRYPTO_30_VHSM_H

#include "Std_Types.h"
#include "Crypto_GeneralTypes.h"

#if !defined(CRYPTO_30_VHSM_CFG_H)
typedef struct {
    uint8 numKeys;
} Crypto_30_vHsm_ConfigType;
#endif

void           Crypto_30_vHsm_Init(const Crypto_30_vHsm_ConfigType* ConfigPtr);
void           Crypto_30_vHsm_DeInit(void);
Std_ReturnType Crypto_30_vHsm_ProcessJob(uint32 objectId, Crypto_JobType* job);
Std_ReturnType Crypto_30_vHsm_KeySetValid(uint32 cryptoKeyId);
Std_ReturnType Crypto_30_vHsm_KeyElementSet(uint32 cryptoKeyId, uint32 keyElementId,
                                            const uint8* keyPtr, uint32 keyLength);
Std_ReturnType Crypto_30_vHsm_RandomSeed(uint32 cryptoKeyId,
                                         const uint8* seedPtr, uint32 seedLength);
Std_ReturnType Crypto_30_vHsm_RandomGenerate(uint32 cryptoKeyId,
                                             uint8* resultPtr, uint32* resultLengthPtr);
void           Crypto_30_vHsm_MainFunction(void);

#endif /* CRYPTO_30_VHSM_H */
