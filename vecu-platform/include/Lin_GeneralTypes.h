/* Lin_GeneralTypes.h -- AUTOSAR LIN General Types for host compilation.
 *
 * Provides Lin_StatusType, Lin_FramePidType and related types used by
 * Lin driver and LinIf modules.
 * Based on AUTOSAR SWS_LinGeneralTypes (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef LIN_GENERAL_TYPES_H
#define LIN_GENERAL_TYPES_H

#include "Std_Types.h"

typedef uint8 Lin_FramePidType;
typedef uint8 Lin_FrameCsModelType;
typedef uint8 Lin_FrameResponseType;
typedef uint8 Lin_FrameDlType;

typedef enum {
    LIN_NOT_OK        = 0x00u,
    LIN_TX_OK         = 0x01u,
    LIN_TX_BUSY       = 0x02u,
    LIN_TX_HEADER_ERROR = 0x03u,
    LIN_TX_ERROR      = 0x04u,
    LIN_RX_OK         = 0x05u,
    LIN_RX_BUSY       = 0x06u,
    LIN_RX_ERROR      = 0x07u,
    LIN_RX_NO_RESPONSE = 0x08u,
    LIN_OPERATIONAL   = 0x09u,
    LIN_CH_SLEEP      = 0x0Au
} Lin_StatusType;

typedef enum {
    LIN_ENHANCED_CS = 0x00u,
    LIN_CLASSIC_CS  = 0x01u
} Lin_FrameCsModelEnumType;

typedef enum {
    LIN_FRAMERESPONSE_TX      = 0x00u,
    LIN_FRAMERESPONSE_RX      = 0x01u,
    LIN_FRAMERESPONSE_IGNORE  = 0x02u
} Lin_FrameResponseEnumType;

typedef struct {
    Lin_FramePidType         Pid;
    Lin_FrameCsModelEnumType Cs;
    Lin_FrameResponseEnumType Drc;
    Lin_FrameDlType          Dl;
    uint8*                   SduPtr;
} Lin_PduType;

typedef enum {
    LIN_SLAVE_RESP_DATA_UPDATED     = 0x00u,
    LIN_SLAVE_RESP_DATA_NOT_UPDATED = 0x01u,
    LIN_SLAVE_RESP_ERR_RESP         = 0x02u,
    LIN_SLAVE_RESP_NO_RESP          = 0x03u
} Lin_SlaveResponseType;

#endif /* LIN_GENERAL_TYPES_H */
