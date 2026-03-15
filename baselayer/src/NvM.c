/* NvM.c — Non-Volatile Memory Manager implementation (ADR-005 / P6).
 *
 * Block-based read/write.  Each block has a RAM mirror and a backing
 * region within ctx.shm_vars.  NvM_WriteBlock copies RAM → SHM,
 * NvM_ReadBlock copies SHM → caller buffer.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "NvM.h"
#include "vecu_base_context.h"
#include <string.h>
#include <stddef.h>

extern const vecu_base_context_t* Base_GetCtx(void);

/* ── Internal state ─────────────────────────────────────────────── */

static boolean g_initialized = FALSE;
static const NvM_ConfigType* g_config = NULL;

/* RAM mirrors — dirty flag per block */
static uint8 g_ram[NVM_MAX_BLOCKS][NVM_MAX_BLOCK_SIZE];
static boolean g_dirty[NVM_MAX_BLOCKS];

/* ── Helpers ────────────────────────────────────────────────────── */

static const NvM_BlockDescriptorType* find_block(uint16 blockId) {
    if (g_config == NULL) { return NULL; }
    for (uint16 i = 0; i < g_config->numBlocks; i++) {
        if (g_config->blocks[i].blockId == blockId) {
            return &g_config->blocks[i];
        }
    }
    return NULL;
}

static uint8* shm_ptr(const NvM_BlockDescriptorType* blk) {
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->shm_vars == NULL) { return NULL; }
    if ((uint32)blk->shmOffset + (uint32)blk->length > ctx->shm_vars_size) {
        return NULL;
    }
    return (uint8*)ctx->shm_vars + blk->shmOffset;
}

static uint16 block_index(const NvM_BlockDescriptorType* blk) {
    /* blocks pointer arithmetic gives index */
    return (uint16)(blk - g_config->blocks);
}

/* ── Lifecycle ──────────────────────────────────────────────────── */

void NvM_Init(const NvM_ConfigType* config) {
    g_config = config;
    memset(g_ram, 0, sizeof(g_ram));
    memset(g_dirty, FALSE, sizeof(g_dirty));
    g_initialized = TRUE;
}

void NvM_DeInit(void) {
    g_initialized = FALSE;
    g_config = NULL;
}

void NvM_MainFunction(void) {
    /* Could process async job queue — currently synchronous. */
    (void)0;
}

/* ── Block API ──────────────────────────────────────────────────── */

Std_ReturnType NvM_ReadBlock(uint16 blockId, uint8* dstPtr) {
    if (!g_initialized || dstPtr == NULL) { return E_NOT_OK; }
    const NvM_BlockDescriptorType* blk = find_block(blockId);
    if (blk == NULL) { return E_NOT_OK; }

    /* Read from SHM backing store into caller buffer */
    uint8* src = shm_ptr(blk);
    if (src != NULL) {
        memcpy(dstPtr, src, blk->length);
    } else {
        /* No SHM — return RAM mirror */
        uint16 idx = block_index(blk);
        memcpy(dstPtr, g_ram[idx], blk->length);
    }
    return E_OK;
}

Std_ReturnType NvM_WriteBlock(uint16 blockId, const uint8* srcPtr) {
    if (!g_initialized || srcPtr == NULL) { return E_NOT_OK; }
    const NvM_BlockDescriptorType* blk = find_block(blockId);
    if (blk == NULL) { return E_NOT_OK; }

    uint16 idx = block_index(blk);
    memcpy(g_ram[idx], srcPtr, blk->length);
    g_dirty[idx] = TRUE;

    /* Write-through to SHM if available */
    uint8* dst = shm_ptr(blk);
    if (dst != NULL) {
        memcpy(dst, srcPtr, blk->length);
    }
    return E_OK;
}

Std_ReturnType NvM_WriteAll(void) {
    if (!g_initialized || g_config == NULL) { return E_NOT_OK; }
    for (uint16 i = 0; i < g_config->numBlocks; i++) {
        if (g_dirty[i]) {
            uint8* dst = shm_ptr(&g_config->blocks[i]);
            if (dst != NULL) {
                memcpy(dst, g_ram[i], g_config->blocks[i].length);
            }
            g_dirty[i] = FALSE;
        }
    }
    return E_OK;
}

Std_ReturnType NvM_ReadAll(void) {
    if (!g_initialized || g_config == NULL) { return E_NOT_OK; }
    for (uint16 i = 0; i < g_config->numBlocks; i++) {
        uint8* src = shm_ptr(&g_config->blocks[i]);
        if (src != NULL) {
            memcpy(g_ram[i], src, g_config->blocks[i].length);
        }
        g_dirty[i] = FALSE;
    }
    return E_OK;
}
