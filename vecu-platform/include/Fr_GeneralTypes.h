/* Fr_GeneralTypes.h -- AUTOSAR FlexRay General Types for host compilation.
 *
 * Provides Fr_ChannelType, Fr_SlotModeType and related types used by
 * Fr driver and FrIf modules.
 * Based on AUTOSAR SWS_FrGeneralTypes (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef FR_GENERAL_TYPES_H
#define FR_GENERAL_TYPES_H

#include "Std_Types.h"

typedef enum {
    FR_CHANNEL_A  = 0x00u,
    FR_CHANNEL_B  = 0x01u,
    FR_CHANNEL_AB = 0x02u
} Fr_ChannelType;

typedef enum {
    FR_SLOTMODE_KEYSLOT = 0x00u,
    FR_SLOTMODE_ALL     = 0x01u
} Fr_SlotModeType;

typedef enum {
    FR_POCSTATE_CONFIG          = 0x00u,
    FR_POCSTATE_DEFAULT_CONFIG  = 0x01u,
    FR_POCSTATE_HALT            = 0x02u,
    FR_POCSTATE_NORMAL_ACTIVE   = 0x03u,
    FR_POCSTATE_NORMAL_PASSIVE  = 0x04u,
    FR_POCSTATE_READY           = 0x05u,
    FR_POCSTATE_STARTUP         = 0x06u,
    FR_POCSTATE_WAKEUP          = 0x07u
} Fr_POCStateType;

typedef enum {
    FR_SLOTMODE_SINGLE      = 0x00u,
    FR_SLOTMODE_ALL_PENDING = 0x01u
} Fr_SlotAssignmentType;

typedef struct {
    boolean CHIHaltRequest;
    boolean ColdstartNoise;
    Fr_POCStateType State;
    boolean Freeze;
    boolean CHIReadyRequest;
    uint8   ErrorMode;
    uint8   SlotMode;
    uint8   WakeupStatus;
    uint8   StartupState;
} Fr_POCStatusType;

typedef uint8 Fr_TxLPduStatusType;
typedef uint8 Fr_RxLPduStatusType;

#define FR_TRANSMITTED       ((Fr_TxLPduStatusType)0x00u)
#define FR_NOT_TRANSMITTED   ((Fr_TxLPduStatusType)0x01u)

#define FR_RECEIVED          ((Fr_RxLPduStatusType)0x00u)
#define FR_NOT_RECEIVED      ((Fr_RxLPduStatusType)0x01u)
#define FR_RECEIVED_MORE_DATA_AVAILABLE ((Fr_RxLPduStatusType)0x02u)

typedef struct {
    uint8 numCtrl;
} Fr_ConfigType;

#endif /* FR_GENERAL_TYPES_H */
