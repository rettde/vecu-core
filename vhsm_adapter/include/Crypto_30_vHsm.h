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

typedef uint32 Crypto_JobIdType;

typedef enum {
    CRYPTO_ALGOFAM_AES       = 0u,
    CRYPTO_ALGOFAM_SHA2_256  = 1u,
    CRYPTO_ALGOFAM_CMAC      = 2u
} Crypto_AlgorithmFamilyType;

typedef enum {
    CRYPTO_ALGOMODE_ECB = 0u,
    CRYPTO_ALGOMODE_CBC = 1u
} Crypto_AlgorithmModeType;

typedef enum {
    CRYPTO_OPERATIONMODE_START       = 0x01u,
    CRYPTO_OPERATIONMODE_UPDATE      = 0x02u,
    CRYPTO_OPERATIONMODE_STREAMSTART = 0x03u,
    CRYPTO_OPERATIONMODE_FINISH      = 0x04u,
    CRYPTO_OPERATIONMODE_SINGLECALL  = 0x07u
} Crypto_OperationModeType;

typedef enum {
    CRYPTO_E_VER_OK       = 0u,
    CRYPTO_E_VER_NOT_OK   = 1u
} Crypto_VerifyResultType;

typedef struct {
    Crypto_AlgorithmFamilyType family;
    Crypto_AlgorithmModeType   mode;
    uint32                     keyLength;
} Crypto_AlgorithmInfoType;

typedef struct {
    const uint8* inputPtr;
    uint32       inputLength;
    uint8*       outputPtr;
    uint32*      outputLengthPtr;
    const uint8* secondaryInputPtr;
    uint32       secondaryInputLength;
    Crypto_VerifyResultType* verifyPtr;
} Crypto_JobPrimitiveInputOutputType;

typedef struct {
    Crypto_AlgorithmInfoType algorithm;
} Crypto_PrimitiveInfoType;

typedef struct {
    uint32                          callbackId;
    const Crypto_PrimitiveInfoType* primitiveInfo;
    uint32                          cryIfKeyId;
} Crypto_JobPrimitiveInfoType;

typedef struct {
    uint32 jobId;
    uint32 jobPriority;
} Crypto_JobInfoType;

typedef enum {
    CRYPTO_JOBSTATE_IDLE   = 0u,
    CRYPTO_JOBSTATE_ACTIVE = 1u
} Crypto_JobStateType;

typedef struct {
    Crypto_JobIdType                    jobId;
    Crypto_JobStateType                 jobState;
    Crypto_JobPrimitiveInputOutputType  jobPrimitiveInputOutput;
    const Crypto_JobPrimitiveInfoType*  jobPrimitiveInfo;
    const Crypto_JobInfoType*           jobInfo;
    uint32                              cryptoKeyId;
} Crypto_JobType;

typedef struct {
    uint8 numKeys;
} Crypto_30_vHsm_ConfigType;

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
