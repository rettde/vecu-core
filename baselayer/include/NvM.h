/* NvM.h — Non-Volatile Memory Manager (ADR-005 / P6).
 *
 * Block-based read/write backed by ctx.shm_vars.  Each block is a
 * fixed-size region within the SHM variable block.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef NVM_H
#define NVM_H

#include "Std_Types.h"

/* ── Configuration ─────────────────────────────────────────────────── */

#define NVM_MAX_BLOCKS      32u
#define NVM_MAX_BLOCK_SIZE  128u

typedef struct {
    uint16 blockId;
    uint16 length;       /* bytes */
    uint16 shmOffset;    /* byte offset within shm_vars */
    uint16 _pad;
} NvM_BlockDescriptorType;

typedef struct {
    const NvM_BlockDescriptorType* blocks;
    uint16 numBlocks;
    uint16 _pad;
} NvM_ConfigType;

/* ── Lifecycle ─────────────────────────────────────────────────────── */

void NvM_Init(const NvM_ConfigType* config);
void NvM_DeInit(void);
void NvM_MainFunction(void);

/* ── Block API ─────────────────────────────────────────────────────── */

Std_ReturnType NvM_ReadBlock(uint16 blockId, uint8* dstPtr);
Std_ReturnType NvM_WriteBlock(uint16 blockId, const uint8* srcPtr);

/** Flush all modified blocks to SHM backing store. */
Std_ReturnType NvM_WriteAll(void);

/** Read all blocks from SHM into RAM mirrors. */
Std_ReturnType NvM_ReadAll(void);

#endif /* NVM_H */
