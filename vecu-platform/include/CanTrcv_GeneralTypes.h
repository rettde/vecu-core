/* CanTrcv_GeneralTypes.h -- AUTOSAR CAN Transceiver General Types.
 *
 * Provides CanTrcv_TrcvModeType and related types used by
 * CanSM and CanTrcv modules.
 * Based on AUTOSAR SWS_CanTrcvGeneralTypes (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CANTRCV_GENERALTYPES_H
#define CANTRCV_GENERALTYPES_H

#include "Std_Types.h"

typedef enum {
    CANTRCV_TRCVMODE_NORMAL  = 0x00u,
    CANTRCV_TRCVMODE_STANDBY = 0x01u,
    CANTRCV_TRCVMODE_SLEEP   = 0x02u
} CanTrcv_TrcvModeType;

typedef enum {
    CANTRCV_WUMODE_ENABLE  = 0x00u,
    CANTRCV_WUMODE_DISABLE = 0x01u,
    CANTRCV_WUMODE_CLEAR   = 0x02u
} CanTrcv_TrcvWakeupModeType;

typedef enum {
    CANTRCV_WU_ERROR          = 0x00u,
    CANTRCV_WU_NOT_SUPPORTED  = 0x01u,
    CANTRCV_WU_BY_BUS         = 0x02u,
    CANTRCV_WU_INTERNALLY     = 0x03u,
    CANTRCV_WU_RESET          = 0x04u,
    CANTRCV_WU_POWER_ON       = 0x05u,
    CANTRCV_WU_BY_PIN         = 0x06u,
    CANTRCV_WU_BY_SYSERR      = 0x07u
} CanTrcv_TrcvWakeupReasonType;

#endif /* CANTRCV_GENERALTYPES_H */
