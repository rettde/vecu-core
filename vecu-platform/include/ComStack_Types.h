/* ComStack_Types.h -- AUTOSAR Communication Stack Types for host compilation.
 *
 * Provides PduInfoType, PduIdType, PduLengthType and related types
 * required by all AUTOSAR communication BSW modules.
 * Based on AUTOSAR SWS_ComStackTypes (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"

typedef uint16 PduIdType;
typedef uint32 PduLengthType;

typedef struct {
    uint8*        SduDataPtr;
    uint8*        MetaDataPtr;
    PduLengthType SduLength;
} PduInfoType;

typedef enum {
    TP_DATACONF    = 0x00u,
    TP_DATARETRY   = 0x01u,
    TP_CONFPENDING = 0x02u
} TpDataStateType;

typedef struct {
    TpDataStateType TpDataState;
    PduLengthType   TxTpDataCnt;
} RetryInfoType;

typedef uint8 NetworkHandleType;

typedef enum {
    BUFREQ_OK       = 0x00u,
    BUFREQ_E_NOT_OK = 0x01u,
    BUFREQ_E_BUSY   = 0x02u,
    BUFREQ_E_OVFL   = 0x03u
} BufReq_ReturnType;

typedef enum {
    TP_STMIN  = 0x00u,
    TP_BS     = 0x01u,
    TP_BC     = 0x02u
} TPParameterType;

typedef uint8 IcomConfigIdType;

typedef enum {
    ICOM_SWITCH_E_OK     = 0x00u,
    ICOM_SWITCH_E_FAILED = 0x01u
} IcomSwitch_ErrorType;

#endif /* COMSTACK_TYPES_H */
