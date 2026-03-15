/* MemIf.c — Memory Abstraction Interface implementation (ADR-005 / P6).
 *
 * Stub: 1:1 pass-through to Fee.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "MemIf.h"
#include "Fee.h"

static boolean g_initialized = FALSE;

void MemIf_Init(void) {
    g_initialized = TRUE;
}

void MemIf_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType MemIf_Read(uint16 blockNumber, uint16 offset,
                          uint8* dataBufferPtr, uint16 length)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Fee_Read(blockNumber, offset, dataBufferPtr, length);
}

Std_ReturnType MemIf_Write(uint16 blockNumber,
                           const uint8* dataBufferPtr, uint16 length)
{
    if (!g_initialized) { return E_NOT_OK; }
    return Fee_Write(blockNumber, dataBufferPtr, length);
}
