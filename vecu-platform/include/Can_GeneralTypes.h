/* Can_GeneralTypes.h -- AUTOSAR CAN General Types for host compilation.
 *
 * Provides Can_HwType, Can_IdType and related types used by
 * Can driver, CanIf, and CanTp modules.
 * Based on AUTOSAR SWS_CanGeneralTypes (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H
#ifndef CAN_GENERAL_TYPES
#define CAN_GENERAL_TYPES
#endif

#include "ComStack_Types.h"

typedef uint32 Can_IdType;
typedef uint16 Can_HwHandleType;

typedef enum {
    CAN_CS_UNINIT  = 0x00u,
    CAN_CS_STOPPED = 0x01u,
    CAN_CS_STARTED = 0x02u,
    CAN_CS_SLEEP   = 0x03u
} Can_ControllerStateType;

typedef enum {
    CAN_T_START  = 0x00u,
    CAN_T_STOP   = 0x01u,
    CAN_T_SLEEP  = 0x02u,
    CAN_T_WAKEUP = 0x03u
} Can_StateTransitionType;

typedef enum {
    CAN_ERRORSTATE_ACTIVE  = 0x00u,
    CAN_ERRORSTATE_PASSIVE = 0x01u,
    CAN_ERRORSTATE_BUSOFF  = 0x02u
} Can_ErrorStateType;

typedef struct {
    Can_IdType       CanId;
    Can_HwHandleType Hoh;
    uint8            ControllerId;
} Can_HwType;

typedef struct {
    PduIdType   swPduHandle;
    uint8       length;
    Can_IdType  id;
    uint8*      sdu;
} Can_PduType;

#ifndef Can_ReturnType
typedef Std_ReturnType Can_ReturnType;
# define CAN_OK      E_OK
# define CAN_NOT_OK  E_NOT_OK
# define CAN_BUSY    ((Std_ReturnType)2u)
#endif

#endif /* CAN_GENERAL_TYPES_H */
