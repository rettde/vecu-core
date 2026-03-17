/* Crypto_30_vHsm.c — Virtual HSM Adapter (ADR-003).
 *
 * Delegates cryptographic operations to vecu-core HSM via
 * vecu_base_context_t callbacks. Provides Crypto_30_vHsm-compatible
 * API for the AUTOSAR Crypto Stack (Csm -> CryIf -> Crypto_30_vHsm).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Crypto_30_vHsm.h"
#include "VMcal_Context.h"
#include <string.h>
#include <stddef.h>

static boolean g_initialized = FALSE;

void Crypto_30_vHsm_Init(const Crypto_30_vHsm_ConfigType* ConfigPtr) {
    (void)ConfigPtr;
    g_initialized = TRUE;
}

void Crypto_30_vHsm_DeInit(void) {
    g_initialized = FALSE;
}

static Std_ReturnType process_encrypt(const vecu_base_context_t* ctx,
                                      Crypto_JobType* job) {
    if (ctx->hsm_encrypt == NULL) { return E_NOT_OK; }

    Crypto_JobPrimitiveInputOutputType* io = &job->jobPrimitiveInputOutput;
    if (io->inputPtr == NULL || io->outputPtr == NULL || io->outputLengthPtr == NULL) {
        return E_NOT_OK;
    }

    uint32_t mode = 0;
    if (job->jobPrimitiveInfo != NULL &&
        job->jobPrimitiveInfo->primitiveInfo != NULL) {
        mode = (uint32_t)job->jobPrimitiveInfo->primitiveInfo->algorithm.mode;
    }

    const uint8_t* iv = io->secondaryInputPtr;
    uint32_t out_len = *io->outputLengthPtr;

    int rc = ctx->hsm_encrypt(job->cryptoKeyId, mode,
                              io->inputPtr, io->inputLength,
                              iv, io->outputPtr, &out_len);
    *io->outputLengthPtr = out_len;
    return (rc == 0) ? E_OK : E_NOT_OK;
}

static Std_ReturnType process_decrypt(const vecu_base_context_t* ctx,
                                      Crypto_JobType* job) {
    if (ctx->hsm_decrypt == NULL) { return E_NOT_OK; }

    Crypto_JobPrimitiveInputOutputType* io = &job->jobPrimitiveInputOutput;
    if (io->inputPtr == NULL || io->outputPtr == NULL || io->outputLengthPtr == NULL) {
        return E_NOT_OK;
    }

    uint32_t mode = 0;
    if (job->jobPrimitiveInfo != NULL &&
        job->jobPrimitiveInfo->primitiveInfo != NULL) {
        mode = (uint32_t)job->jobPrimitiveInfo->primitiveInfo->algorithm.mode;
    }

    const uint8_t* iv = io->secondaryInputPtr;
    uint32_t out_len = *io->outputLengthPtr;

    int rc = ctx->hsm_decrypt(job->cryptoKeyId, mode,
                              io->inputPtr, io->inputLength,
                              iv, io->outputPtr, &out_len);
    *io->outputLengthPtr = out_len;
    return (rc == 0) ? E_OK : E_NOT_OK;
}

static Std_ReturnType process_mac_generate(const vecu_base_context_t* ctx,
                                           Crypto_JobType* job) {
    if (ctx->hsm_generate_mac == NULL) { return E_NOT_OK; }

    Crypto_JobPrimitiveInputOutputType* io = &job->jobPrimitiveInputOutput;
    if (io->inputPtr == NULL || io->outputPtr == NULL || io->outputLengthPtr == NULL) {
        return E_NOT_OK;
    }

    uint32_t mac_len = *io->outputLengthPtr;
    int rc = ctx->hsm_generate_mac(job->cryptoKeyId,
                                   io->inputPtr, io->inputLength,
                                   io->outputPtr, &mac_len);
    *io->outputLengthPtr = mac_len;
    return (rc == 0) ? E_OK : E_NOT_OK;
}

static Std_ReturnType process_mac_verify(const vecu_base_context_t* ctx,
                                         Crypto_JobType* job) {
    if (ctx->hsm_verify_mac == NULL) { return E_NOT_OK; }

    Crypto_JobPrimitiveInputOutputType* io = &job->jobPrimitiveInputOutput;
    if (io->inputPtr == NULL || io->secondaryInputPtr == NULL) { return E_NOT_OK; }

    int rc = ctx->hsm_verify_mac(job->cryptoKeyId,
                                 io->inputPtr, io->inputLength,
                                 io->secondaryInputPtr, io->secondaryInputLength);
    if (io->verifyPtr != NULL) {
        *io->verifyPtr = (rc == 0) ? CRYPTO_E_VER_OK : CRYPTO_E_VER_NOT_OK;
    }
    return (rc == 0) ? E_OK : E_NOT_OK;
}

Std_ReturnType Crypto_30_vHsm_ProcessJob(uint32 objectId, Crypto_JobType* job) {
    (void)objectId;
    if (!g_initialized || job == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL) { return E_NOT_OK; }

    if (job->jobPrimitiveInfo == NULL ||
        job->jobPrimitiveInfo->primitiveInfo == NULL) {
        return E_NOT_OK;
    }

    Crypto_AlgorithmFamilyType fam =
        job->jobPrimitiveInfo->primitiveInfo->algorithm.family;

    job->jobState = CRYPTO_JOBSTATE_ACTIVE;

    Std_ReturnType ret;
    switch (fam) {
    case CRYPTO_ALGOFAM_AES:
        if (job->jobPrimitiveInputOutput.outputPtr != NULL) {
            ret = process_encrypt(ctx, job);
        } else {
            ret = process_decrypt(ctx, job);
        }
        break;
    case CRYPTO_ALGOFAM_CMAC:
        if (job->jobPrimitiveInputOutput.verifyPtr != NULL) {
            ret = process_mac_verify(ctx, job);
        } else {
            ret = process_mac_generate(ctx, job);
        }
        break;
    default:
        ret = E_NOT_OK;
        break;
    }

    job->jobState = CRYPTO_JOBSTATE_IDLE;
    return ret;
}

Std_ReturnType Crypto_30_vHsm_KeySetValid(uint32 cryptoKeyId) {
    (void)cryptoKeyId;
    if (!g_initialized) { return E_NOT_OK; }
    return E_OK;
}

Std_ReturnType Crypto_30_vHsm_KeyElementSet(uint32 cryptoKeyId, uint32 keyElementId,
                                            const uint8* keyPtr, uint32 keyLength) {
    (void)cryptoKeyId;
    (void)keyElementId;
    (void)keyPtr;
    (void)keyLength;
    if (!g_initialized) { return E_NOT_OK; }
    return E_OK;
}

Std_ReturnType Crypto_30_vHsm_RandomSeed(uint32 cryptoKeyId,
                                         const uint8* seedPtr, uint32 seedLength) {
    (void)cryptoKeyId;
    (void)seedPtr;
    (void)seedLength;
    if (!g_initialized) { return E_NOT_OK; }
    return E_OK;
}

Std_ReturnType Crypto_30_vHsm_RandomGenerate(uint32 cryptoKeyId,
                                             uint8* resultPtr, uint32* resultLengthPtr) {
    (void)cryptoKeyId;
    if (!g_initialized || resultPtr == NULL || resultLengthPtr == NULL) {
        return E_NOT_OK;
    }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->hsm_rng == NULL) { return E_NOT_OK; }

    int rc = ctx->hsm_rng(resultPtr, *resultLengthPtr);
    return (rc == 0) ? E_OK : E_NOT_OK;
}

void Crypto_30_vHsm_MainFunction(void) {
    if (!g_initialized) { return; }
}
