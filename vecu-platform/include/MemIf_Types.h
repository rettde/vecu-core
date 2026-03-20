/* MemIf_Types.h -- AUTOSAR Memory Interface Types for host compilation.
 *
 * Provides MemIf_StatusType, MemIf_JobResultType and related types
 * used by NvM, Fee, MemIf modules.
 * Based on AUTOSAR SWS_MemIfTypes (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef MEMIF_TYPES_H
#define MEMIF_TYPES_H

#include "Std_Types.h"

typedef enum {
    MEMIF_UNINIT     = 0x00u,
    MEMIF_IDLE       = 0x01u,
    MEMIF_BUSY       = 0x02u,
    MEMIF_BUSY_INTERNAL = 0x03u
} MemIf_StatusType;

typedef enum {
    MEMIF_JOB_OK                = 0x00u,
    MEMIF_JOB_FAILED            = 0x01u,
    MEMIF_JOB_PENDING           = 0x02u,
    MEMIF_JOB_CANCELED          = 0x03u,
    MEMIF_BLOCK_INCONSISTENT    = 0x04u,
    MEMIF_BLOCK_INVALID         = 0x05u
} MemIf_JobResultType;

typedef enum {
    MEMIF_MODE_SLOW = 0x00u,
    MEMIF_MODE_FAST = 0x01u
} MemIf_ModeType;

typedef uint8* MemIf_DataPtr_pu8;

#endif /* MEMIF_TYPES_H */
