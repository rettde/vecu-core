/* CryIf.c — Crypto Interface implementation (ADR-005 / P5).
 *
 * Thin 1:1 routing layer.  With a single Cry driver all calls pass
 * straight through.  In a multi-driver setup this layer would select
 * the correct driver based on a configuration table.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "CryIf.h"
#include "Cry.h"

static boolean g_initialized = FALSE;

/* ── Lifecycle ──────────────────────────────────────────────────── */

void CryIf_Init(void) {
    g_initialized = TRUE;
}

void CryIf_DeInit(void) {
    g_initialized = FALSE;
}

/* ── Pass-through to Cry driver ─────────────────────────────────── */

Std_ReturnType CryIf_Encrypt(uint32 keySlot, uint32 mode,
                             const uint8* data, uint32 dataLen,
                             const uint8* iv,
                             uint8* result, uint32* resultLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Cry_Encrypt(keySlot, mode, data, dataLen, iv, result, resultLen);
}

Std_ReturnType CryIf_Decrypt(uint32 keySlot, uint32 mode,
                             const uint8* data, uint32 dataLen,
                             const uint8* iv,
                             uint8* result, uint32* resultLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Cry_Decrypt(keySlot, mode, data, dataLen, iv, result, resultLen);
}

Std_ReturnType CryIf_MacGenerate(uint32 keySlot,
                                 const uint8* data, uint32 dataLen,
                                 uint8* macPtr, uint32* macLenPtr)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Cry_MacGenerate(keySlot, data, dataLen, macPtr, macLenPtr);
}

Std_ReturnType CryIf_MacVerify(uint32 keySlot,
                               const uint8* data, uint32 dataLen,
                               const uint8* macPtr, uint32 macLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Cry_MacVerify(keySlot, data, dataLen, macPtr, macLen);
}

Std_ReturnType CryIf_SeedGenerate(uint8* outSeed, uint32* outLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Cry_SeedGenerate(outSeed, outLen);
}

Std_ReturnType CryIf_KeyValidate(const uint8* keyBuf, uint32 keyLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Cry_KeyValidate(keyBuf, keyLen);
}

Std_ReturnType CryIf_RandomGenerate(uint8* outBuf, uint32 bufLen)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Cry_RandomGenerate(outBuf, bufLen);
}
