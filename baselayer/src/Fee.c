/* Fee.c — Flash Emulation implementation (ADR-005 / P6).
 *
 * RAM-backed emulation.  Stores blocks in a simple flat array.
 * In a real ECU this would abstract NOR/NAND flash.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Fee.h"
#include <string.h>
#include <stddef.h>

#define FEE_MAX_BLOCKS  32u
#define FEE_BLOCK_SIZE  128u

static boolean g_initialized = FALSE;
static uint8 g_store[FEE_MAX_BLOCKS][FEE_BLOCK_SIZE];

void Fee_Init(void) {
    memset(g_store, 0xFF, sizeof(g_store));  /* erased state */
    g_initialized = TRUE;
}

void Fee_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType Fee_Read(uint16 blockNumber, uint16 offset,
                        uint8* dataBufferPtr, uint16 length)
{
    if (!g_initialized || dataBufferPtr == NULL) { return E_NOT_OK; }
    if (blockNumber >= FEE_MAX_BLOCKS) { return E_NOT_OK; }
    if ((uint32)offset + (uint32)length > FEE_BLOCK_SIZE) { return E_NOT_OK; }
    memcpy(dataBufferPtr, &g_store[blockNumber][offset], length);
    return E_OK;
}

Std_ReturnType Fee_Write(uint16 blockNumber,
                         const uint8* dataBufferPtr, uint16 length)
{
    if (!g_initialized || dataBufferPtr == NULL) { return E_NOT_OK; }
    if (blockNumber >= FEE_MAX_BLOCKS) { return E_NOT_OK; }
    if (length > FEE_BLOCK_SIZE) { return E_NOT_OK; }
    memcpy(g_store[blockNumber], dataBufferPtr, length);
    return E_OK;
}
