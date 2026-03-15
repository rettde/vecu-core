/* Cry.c — Crypto Driver implementation (ADR-005 / P5).
 *
 * Wraps the HSM callback pointers from vecu_base_context_t into a
 * standard AUTOSAR-style API.  CryIf calls these functions; they
 * delegate directly to the ctx.hsm_*() callbacks injected by the
 * Rust bridge.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Cry.h"
#include "vecu_base_context.h"
#include <stddef.h>

/* ── Forward declaration (defined in Base_Entry.c) ────────────────── */
extern const vecu_base_context_t* Base_GetCtx(void);

/* ── Internal state ─────────────────────────────────────────────── */

static boolean g_initialized = FALSE;

/* ── Lifecycle ──────────────────────────────────────────────────── */

void Cry_Init(void) {
    g_initialized = TRUE;
}

void Cry_DeInit(void) {
    g_initialized = FALSE;
}

/* ── Helper: convert VECU_OK / negative to E_OK / E_NOT_OK ─────── */

static Std_ReturnType vecu_to_std(int rc) {
    return (rc == 0) ? E_OK : E_NOT_OK;
}

/* ── Crypto primitives ─────────────────────────────────────────── */

Std_ReturnType Cry_Encrypt(uint32 keySlot, uint32 mode,
                           const uint8* data, uint32 dataLen,
                           const uint8* iv,
                           uint8* result, uint32* resultLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->hsm_encrypt == NULL) { return E_NOT_OK; }
    return vecu_to_std(ctx->hsm_encrypt(keySlot, mode, data, dataLen,
                                        iv, result, resultLen));
}

Std_ReturnType Cry_Decrypt(uint32 keySlot, uint32 mode,
                           const uint8* data, uint32 dataLen,
                           const uint8* iv,
                           uint8* result, uint32* resultLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->hsm_decrypt == NULL) { return E_NOT_OK; }
    return vecu_to_std(ctx->hsm_decrypt(keySlot, mode, data, dataLen,
                                        iv, result, resultLen));
}

Std_ReturnType Cry_MacGenerate(uint32 keySlot,
                               const uint8* data, uint32 dataLen,
                               uint8* macPtr, uint32* macLenPtr)
{
    if (!g_initialized) { return E_NOT_OK; }
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->hsm_generate_mac == NULL) { return E_NOT_OK; }
    return vecu_to_std(ctx->hsm_generate_mac(keySlot, data, dataLen,
                                             macPtr, macLenPtr));
}

Std_ReturnType Cry_MacVerify(uint32 keySlot,
                             const uint8* data, uint32 dataLen,
                             const uint8* macPtr, uint32 macLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->hsm_verify_mac == NULL) { return E_NOT_OK; }
    return vecu_to_std(ctx->hsm_verify_mac(keySlot, data, dataLen,
                                           macPtr, macLen));
}

Std_ReturnType Cry_SeedGenerate(uint8* outSeed, uint32* outLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->hsm_seed == NULL) { return E_NOT_OK; }
    return vecu_to_std(ctx->hsm_seed(outSeed, outLen));
}

Std_ReturnType Cry_KeyValidate(const uint8* keyBuf, uint32 keyLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->hsm_key == NULL) { return E_NOT_OK; }
    return vecu_to_std(ctx->hsm_key(keyBuf, keyLen));
}

Std_ReturnType Cry_RandomGenerate(uint8* outBuf, uint32 bufLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->hsm_rng == NULL) { return E_NOT_OK; }
    return vecu_to_std(ctx->hsm_rng(outBuf, bufLen));
}
