/* Fls.h — Virtual Flash Driver API (ADR-002 / Virtual-MCAL).
 *
 * Shared-Memory-backed drop-in replacement for Fls_* MCAL driver.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_FLS_H
#define VMCAL_FLS_H

#include "Std_Types.h"

typedef uint32 Fls_AddressType;
typedef uint32 Fls_LengthType;

typedef enum {
    MEMIF_UNINIT     = 0u,
    MEMIF_IDLE       = 1u,
    MEMIF_BUSY       = 2u,
    MEMIF_BUSY_INTERNAL = 3u
} MemIf_StatusType;

typedef enum {
    MEMIF_JOB_OK       = 0u,
    MEMIF_JOB_FAILED   = 1u,
    MEMIF_JOB_PENDING  = 2u,
    MEMIF_JOB_CANCELED = 3u,
    MEMIF_BLOCK_INCONSISTENT = 4u,
    MEMIF_BLOCK_INVALID      = 5u
} MemIf_JobResultType;

typedef void (*Fls_JobEndNotificationType)(void);
typedef void (*Fls_JobErrorNotificationType)(void);

typedef struct {
    uint32 flashSize;
    Fls_JobEndNotificationType   jobEndNotification;
    Fls_JobErrorNotificationType jobErrorNotification;
} Fls_ConfigType;

void               Fls_Init(const Fls_ConfigType* ConfigPtr);
void               Fls_DeInit(void);
Std_ReturnType     Fls_Read(Fls_AddressType SourceAddress,
                            uint8* TargetAddressPtr, Fls_LengthType Length);
Std_ReturnType     Fls_Write(Fls_AddressType TargetAddress,
                             const uint8* SourceAddressPtr, Fls_LengthType Length);
Std_ReturnType     Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length);
void               Fls_Cancel(void);
MemIf_StatusType   Fls_GetStatus(void);
MemIf_JobResultType Fls_GetJobResult(void);
void               Fls_MainFunction(void);

#endif /* VMCAL_FLS_H */
