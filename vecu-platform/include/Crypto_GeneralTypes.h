/* Crypto_GeneralTypes.h -- AUTOSAR Crypto General Types for host compilation.
 *
 * Provides Crypto_AlgorithmFamilyType, Crypto_JobType and related types
 * used by Csm, CryIf, and Crypto driver modules.
 *
 * Layout matches MICROSAR SIP 34.7.3 (Csm, ASR R20-11 / R21-11).
 * Types are uint8 with #define constants (not enums) per SIP convention.
 * Struct layouts match the common MICROSAR configuration.
 *
 * Based on AUTOSAR SWS_CryptoGeneralTypes (R4.x / R20-11).
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#if !defined(CRYPTO_GENERALTYPES_H)
#define CRYPTO_GENERALTYPES_H

#include "Std_Types.h"

/* ---- Error codes [SWS_Csm_01069] ----------------------------------- */
#if !defined(CRYPTO_E_BUSY)
# define CRYPTO_E_BUSY                    2u
#endif
#if !defined(CRYPTO_E_SMALL_BUFFER)
# define CRYPTO_E_SMALL_BUFFER            3u
#endif
#if !defined(CRYPTO_E_ENTROPY_EXHAUSTED)
# define CRYPTO_E_ENTROPY_EXHAUSTED       4u
#endif
#if !defined(CRYPTO_E_QUEUE_FULL)
# define CRYPTO_E_QUEUE_FULL              5u
#endif
#if !defined(CRYPTO_E_KEY_READ_FAIL)
# define CRYPTO_E_KEY_READ_FAIL           6u
#endif
#if !defined(CRYPTO_E_KEY_WRITE_FAIL)
# define CRYPTO_E_KEY_WRITE_FAIL          7u
#endif
#if !defined(CRYPTO_E_KEY_NOT_AVAILABLE)
# define CRYPTO_E_KEY_NOT_AVAILABLE       8u
#endif
#if !defined(CRYPTO_E_KEY_NOT_VALID)
# define CRYPTO_E_KEY_NOT_VALID           9u
#endif
#if !defined(CRYPTO_E_KEY_SIZE_MISMATCH)
# define CRYPTO_E_KEY_SIZE_MISMATCH       10u
#endif
#if !defined(CRYPTO_E_JOB_CANCELED)
# define CRYPTO_E_JOB_CANCELED            12u
#endif
#if !defined(CRYPTO_E_KEY_EMPTY)
# define CRYPTO_E_KEY_EMPTY               13u
#endif

/* ---- Key element IDs [SWS_Csm_01022] ------------------------------- */
#if !defined(CRYPTO_KE_MAC_KEY)
# define CRYPTO_KE_MAC_KEY                1u
#endif
#if !defined(CRYPTO_KE_MAC_PROOF)
# define CRYPTO_KE_MAC_PROOF              2u
#endif
#if !defined(CRYPTO_KE_CIPHER_KEY)
# define CRYPTO_KE_CIPHER_KEY             1u
#endif
#if !defined(CRYPTO_KE_CIPHER_IV)
# define CRYPTO_KE_CIPHER_IV              5u
#endif
#if !defined(CRYPTO_KE_CIPHER_PROOF)
# define CRYPTO_KE_CIPHER_PROOF           6u
#endif
#if !defined(CRYPTO_KE_CIPHER_2NDKEY)
# define CRYPTO_KE_CIPHER_2NDKEY          7u
#endif
#if !defined(CRYPTO_KE_SIGNATURE_KEY)
# define CRYPTO_KE_SIGNATURE_KEY          1u
#endif
#if !defined(CRYPTO_KE_RANDOM_SEED_STATE)
# define CRYPTO_KE_RANDOM_SEED_STATE      3u
#endif
#if !defined(CRYPTO_KE_RANDOM_ALGORITHM)
# define CRYPTO_KE_RANDOM_ALGORITHM       4u
#endif
#if !defined(CRYPTO_KE_KEYEXCHANGE_BASE)
# define CRYPTO_KE_KEYEXCHANGE_BASE       8u
#endif
#if !defined(CRYPTO_KE_KEYEXCHANGE_PRIVKEY)
# define CRYPTO_KE_KEYEXCHANGE_PRIVKEY    9u
#endif
#if !defined(CRYPTO_KE_KEYEXCHANGE_OWNPUBKEY)
# define CRYPTO_KE_KEYEXCHANGE_OWNPUBKEY  10u
#endif
#if !defined(CRYPTO_KE_KEYEXCHANGE_SHAREDVALUE)
# define CRYPTO_KE_KEYEXCHANGE_SHAREDVALUE 1u
#endif
#if !defined(CRYPTO_KE_KEYDERIVATION_PASSWORD)
# define CRYPTO_KE_KEYDERIVATION_PASSWORD 1u
#endif
#if !defined(CRYPTO_KE_KEYDERIVATION_SALT)
# define CRYPTO_KE_KEYDERIVATION_SALT     13u
#endif
#if !defined(CRYPTO_KE_KEYDERIVATION_ITERATIONS)
# define CRYPTO_KE_KEYDERIVATION_ITERATIONS 14u
#endif
#if !defined(CRYPTO_KE_KEYDERIVATION_ALGORITHM)
# define CRYPTO_KE_KEYDERIVATION_ALGORITHM 15u
#endif
#if !defined(CRYPTO_KE_KEYGENERATE_KEY)
# define CRYPTO_KE_KEYGENERATE_KEY        1u
#endif
#if !defined(CRYPTO_KE_KEYGENERATE_SEED)
# define CRYPTO_KE_KEYGENERATE_SEED       16u
#endif
#if !defined(CRYPTO_KE_KEYGENERATE_ALGORITHM)
# define CRYPTO_KE_KEYGENERATE_ALGORITHM  17u
#endif

/* ---- Crypto_JobStateType [SWS_Csm_01028] --------------------------- */
typedef uint8 Crypto_JobStateType;
#define CRYPTO_JOBSTATE_IDLE              0u
#define CRYPTO_JOBSTATE_ACTIVE            1u
#define CRYPTO_JOBSTATE_PROGRESSING       2u

/* ---- Crypto_ServiceInfoType [SWS_Csm_01028] ------------------------ */
typedef uint8 Crypto_ServiceInfoType;
#if !defined(CRYPTO_HASH)
# define CRYPTO_HASH                      0x00u
#endif
#if !defined(CRYPTO_MACGENERATE)
# define CRYPTO_MACGENERATE               0x01u
#endif
#if !defined(CRYPTO_MACVERIFY)
# define CRYPTO_MACVERIFY                 0x02u
#endif
#if !defined(CRYPTO_ENCRYPT)
# define CRYPTO_ENCRYPT                   0x03u
#endif
#if !defined(CRYPTO_DECRYPT)
# define CRYPTO_DECRYPT                   0x04u
#endif
#if !defined(CRYPTO_AEADENCRYPT)
# define CRYPTO_AEADENCRYPT               0x05u
#endif
#if !defined(CRYPTO_AEADDECRYPT)
# define CRYPTO_AEADDECRYPT               0x06u
#endif
#if !defined(CRYPTO_SIGNATUREGENERATE)
# define CRYPTO_SIGNATUREGENERATE         0x07u
#endif
#if !defined(CRYPTO_SIGNATUREVERIFY)
# define CRYPTO_SIGNATUREVERIFY           0x08u
#endif
#if !defined(CRYPTO_RANDOMGENERATE)
# define CRYPTO_RANDOMGENERATE            0x0Bu
#endif
#if !defined(CRYPTO_KEYSETVALID)
# define CRYPTO_KEYSETVALID               0x13u
#endif

/* ---- Crypto_AlgorithmFamilyType [SWS_Csm_01047] -------------------- */
typedef uint8 Crypto_AlgorithmFamilyType;
typedef uint8 Crypto_AlgorithmSecondaryFamilyType;
#if !defined(CRYPTO_ALGOFAM_NOT_SET)
# define CRYPTO_ALGOFAM_NOT_SET           0x00u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA1)
# define CRYPTO_ALGOFAM_SHA1              0x01u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA2_224)
# define CRYPTO_ALGOFAM_SHA2_224          0x02u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA2_256)
# define CRYPTO_ALGOFAM_SHA2_256          0x03u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA2_384)
# define CRYPTO_ALGOFAM_SHA2_384          0x04u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA2_512)
# define CRYPTO_ALGOFAM_SHA2_512          0x05u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA3_224)
# define CRYPTO_ALGOFAM_SHA3_224          0x08u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA3_256)
# define CRYPTO_ALGOFAM_SHA3_256          0x09u
#endif
#if !defined(CRYPTO_ALGOFAM_SHA3_384)
# define CRYPTO_ALGOFAM_SHA3_384          0x0Au
#endif
#if !defined(CRYPTO_ALGOFAM_SHA3_512)
# define CRYPTO_ALGOFAM_SHA3_512          0x0Bu
#endif
#if !defined(CRYPTO_ALGOFAM_BLAKE_1_256)
# define CRYPTO_ALGOFAM_BLAKE_1_256       0x0Fu
#endif
#if !defined(CRYPTO_ALGOFAM_BLAKE_1_512)
# define CRYPTO_ALGOFAM_BLAKE_1_512       0x10u
#endif
#if !defined(CRYPTO_ALGOFAM_BLAKE_2s_256)
# define CRYPTO_ALGOFAM_BLAKE_2s_256      0x11u
#endif
#if !defined(CRYPTO_ALGOFAM_BLAKE_2s_512)
# define CRYPTO_ALGOFAM_BLAKE_2s_512      0x12u
#endif
#if !defined(CRYPTO_ALGOFAM_3DES)
# define CRYPTO_ALGOFAM_3DES              0x13u
#endif
#if !defined(CRYPTO_ALGOFAM_AES)
# define CRYPTO_ALGOFAM_AES               0x14u
#endif
#if !defined(CRYPTO_ALGOFAM_CHACHA)
# define CRYPTO_ALGOFAM_CHACHA            0x15u
#endif
#if !defined(CRYPTO_ALGOFAM_RSA)
# define CRYPTO_ALGOFAM_RSA               0x16u
#endif
#if !defined(CRYPTO_ALGOFAM_ED25519)
# define CRYPTO_ALGOFAM_ED25519           0x17u
#endif
#if !defined(CRYPTO_ALGOFAM_BRAINPOOL)
# define CRYPTO_ALGOFAM_BRAINPOOL         0x18u
#endif
#if !defined(CRYPTO_ALGOFAM_ECCNIST)
# define CRYPTO_ALGOFAM_ECCNIST           0x19u
#endif
#if !defined(CRYPTO_ALGOFAM_SECURECOUNTER)
# define CRYPTO_ALGOFAM_SECURECOUNTER     0x1Au
#endif
#if !defined(CRYPTO_ALGOFAM_RNG)
# define CRYPTO_ALGOFAM_RNG               0x1Bu
#endif
#if !defined(CRYPTO_ALGOFAM_SIPHASH)
# define CRYPTO_ALGOFAM_SIPHASH           0x1Cu
#endif
#if !defined(CRYPTO_ALGOFAM_POLY1305)
# define CRYPTO_ALGOFAM_POLY1305          0x2Du
#endif
#if !defined(CRYPTO_ALGOFAM_CUSTOM)
# define CRYPTO_ALGOFAM_CUSTOM            0xFFu
#endif

/* ---- Crypto_AlgorithmModeType [SWS_Csm_01048] ---------------------- */
typedef uint8 Crypto_AlgorithmModeType;
#if !defined(CRYPTO_ALGOMODE_NOT_SET)
# define CRYPTO_ALGOMODE_NOT_SET          0x00u
#endif
#if !defined(CRYPTO_ALGOMODE_ECB)
# define CRYPTO_ALGOMODE_ECB              0x01u
#endif
#if !defined(CRYPTO_ALGOMODE_CBC)
# define CRYPTO_ALGOMODE_CBC              0x02u
#endif
#if !defined(CRYPTO_ALGOMODE_CFB)
# define CRYPTO_ALGOMODE_CFB              0x03u
#endif
#if !defined(CRYPTO_ALGOMODE_OFB)
# define CRYPTO_ALGOMODE_OFB              0x04u
#endif
#if !defined(CRYPTO_ALGOMODE_CTR)
# define CRYPTO_ALGOMODE_CTR              0x05u
#endif
#if !defined(CRYPTO_ALGOMODE_XTS)
# define CRYPTO_ALGOMODE_XTS              0x06u
#endif
#if !defined(CRYPTO_ALGOMODE_GCM)
# define CRYPTO_ALGOMODE_GCM              0x07u
#endif
#if !defined(CRYPTO_ALGOMODE_RSAES_OAEP)
# define CRYPTO_ALGOMODE_RSAES_OAEP       0x08u
#endif
#if !defined(CRYPTO_ALGOMODE_RSAES_PKCS1_v1_5)
# define CRYPTO_ALGOMODE_RSAES_PKCS1_v1_5 0x09u
#endif
#if !defined(CRYPTO_ALGOMODE_RSASSA_PSS)
# define CRYPTO_ALGOMODE_RSASSA_PSS       0x0Au
#endif
#if !defined(CRYPTO_ALGOMODE_RSASSA_PKCS1_v1_5)
# define CRYPTO_ALGOMODE_RSASSA_PKCS1_v1_5 0x0Bu
#endif
#if !defined(CRYPTO_ALGOMODE_8ROUNDS)
# define CRYPTO_ALGOMODE_8ROUNDS          0x0Cu
#endif
#if !defined(CRYPTO_ALGOMODE_12ROUNDS)
# define CRYPTO_ALGOMODE_12ROUNDS         0x0Du
#endif
#if !defined(CRYPTO_ALGOMODE_20ROUNDS)
# define CRYPTO_ALGOMODE_20ROUNDS         0x0Eu
#endif
#if !defined(CRYPTO_ALGOMODE_HMAC)
# define CRYPTO_ALGOMODE_HMAC             0x0Fu
#endif
#if !defined(CRYPTO_ALGOMODE_CMAC)
# define CRYPTO_ALGOMODE_CMAC             0x10u
#endif
#if !defined(CRYPTO_ALGOMODE_GMAC)
# define CRYPTO_ALGOMODE_GMAC             0x11u
#endif
#if !defined(CRYPTO_ALGOMODE_CTRDRBG)
# define CRYPTO_ALGOMODE_CTRDRBG          0x12u
#endif
#if !defined(CRYPTO_ALGOMODE_SIPHASH_2_4)
# define CRYPTO_ALGOMODE_SIPHASH_2_4      0x13u
#endif
#if !defined(CRYPTO_ALGOMODE_SIPHASH_4_8)
# define CRYPTO_ALGOMODE_SIPHASH_4_8      0x14u
#endif
#if !defined(CRYPTO_ALGOMODE_CUSTOM)
# define CRYPTO_ALGOMODE_CUSTOM           0xFFu
#endif

/* ---- Crypto_OperationModeType --------------------------------------- */
typedef uint8 Crypto_OperationModeType;
#if !defined(CRYPTO_OPERATIONMODE_START)
# define CRYPTO_OPERATIONMODE_START       1u
#endif
#if !defined(CRYPTO_OPERATIONMODE_UPDATE)
# define CRYPTO_OPERATIONMODE_UPDATE      2u
#endif
#if !defined(CRYPTO_OPERATIONMODE_FINISH)
# define CRYPTO_OPERATIONMODE_FINISH      4u
#endif
#if !defined(CRYPTO_OPERATIONMODE_STREAMSTART)
# define CRYPTO_OPERATIONMODE_STREAMSTART (CRYPTO_OPERATIONMODE_START | CRYPTO_OPERATIONMODE_UPDATE)
#endif
#if !defined(CRYPTO_OPERATIONMODE_SINGLECALL)
# define CRYPTO_OPERATIONMODE_SINGLECALL  (CRYPTO_OPERATIONMODE_START | CRYPTO_OPERATIONMODE_UPDATE | CRYPTO_OPERATIONMODE_FINISH)
#endif

/* ---- Crypto_ProcessingType [SWS_Csm_01049] ------------------------- */
typedef uint8 Crypto_ProcessingType;
#define CRYPTO_PROCESSING_ASYNC           0u
#define CRYPTO_PROCESSING_SYNC            1u

/* ---- Crypto_VerifyResultType [SWS_Csm_01024] ----------------------- */
typedef uint8 Crypto_VerifyResultType;
#if !defined(CRYPTO_E_VER_OK)
# define CRYPTO_E_VER_OK                  0u
#endif
#if !defined(CRYPTO_E_VER_NOT_OK)
# define CRYPTO_E_VER_NOT_OK              1u
#endif

/* ---- Crypto_AlgorithmInfoType [SWS_Csm_01008] ---------------------- */
typedef struct {
    Crypto_AlgorithmFamilyType          family;
    Crypto_AlgorithmSecondaryFamilyType secondaryFamily;
    uint32                              keyLength;
    Crypto_AlgorithmModeType            mode;
} Crypto_AlgorithmInfoType;

/* ---- Crypto_PrimitiveInfoType [SWS_Csm_00930] ---------------------- */
typedef struct {
    Crypto_ServiceInfoType   service;
    Crypto_AlgorithmInfoType algorithm;
} Crypto_PrimitiveInfoType;

/* ---- Crypto_JobPrimitiveInfoType [SWS_Csm_01012] ------------------- */
typedef struct {
    uint32                          callbackId;
    const Crypto_PrimitiveInfoType* primitiveInfo;
    uint32                          cryIfKeyId;
    Crypto_ProcessingType           processingType;
} Crypto_JobPrimitiveInfoType;

/* ---- Crypto_JobPrimitiveInputOutputType [SWS_Csm_01009] ------------ */
typedef struct {
    const uint8*             inputPtr;
    uint32                   inputLength;
    const uint8*             secondaryInputPtr;
    uint32                   secondaryInputLength;
    const uint8*             tertiaryInputPtr;
    uint32                   tertiaryInputLength;
    uint8*                   outputPtr;
    uint32*                  outputLengthPtr;
    uint8*                   secondaryOutputPtr;
    uint32*                  secondaryOutputLengthPtr;
    uint64                   input64;
    Crypto_VerifyResultType* verifyPtr;
    uint64*                  output64Ptr;
    Crypto_OperationModeType mode;
    uint32                   cryIfKeyId;
    uint32                   targetCryIfKeyId;
} Crypto_JobPrimitiveInputOutputType;

/* ---- Crypto_JobInfoType [SWS_Csm_01013] ---------------------------- */
typedef struct {
    uint32 jobId;
    uint32 jobPriority;
} Crypto_JobInfoType;

/* ---- Crypto_JobRedirectionInfoType [SWS_Csm_91026] ----------------- */
typedef struct {
    uint8  redirectionConfig;
    uint32 inputKeyId;
    uint32 inputKeyElementId;
    uint32 secondaryInputKeyId;
    uint32 secondaryInputKeyElementId;
    uint32 tertiaryInputKeyId;
    uint32 tertiaryInputKeyElementId;
    uint32 outputKeyId;
    uint32 outputKeyElementId;
    uint32 secondaryOutputKeyId;
    uint32 secondaryOutputKeyElementId;
} Crypto_JobRedirectionInfoType;

/* ---- Crypto_JobType [SWS_Csm_01013] -------------------------------- */
typedef struct {
    uint32                                  jobId;
    Crypto_JobStateType                     jobState;
    Crypto_JobPrimitiveInputOutputType      jobPrimitiveInputOutput;
    const Crypto_JobPrimitiveInfoType*      jobPrimitiveInfo;
    Crypto_JobRedirectionInfoType*          jobRedirectionInfoRef;
    const Crypto_JobInfoType*               jobInfo;
    uint32                                  cryptoKeyId;
    uint32                                  targetCryptoKeyId;
    uint32                                  jobPriority;
} Crypto_JobType;

#endif /* !defined(CRYPTO_GENERALTYPES_H) */
