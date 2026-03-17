/* ComM.h — Communication Manager (simplified stub).
 *
 * Minimal AUTOSAR ComM state machine for Level-3 vECU.
 * Manages communication modes: NO_COM → FULL_COM → SILENT_COM.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef COMM_H
#define COMM_H

#include "Std_Types.h"

typedef uint8 ComM_ModeType;

#define COMM_NO_COMMUNICATION     0u
#define COMM_SILENT_COMMUNICATION 1u
#define COMM_FULL_COMMUNICATION   2u

typedef uint8 ComM_UserHandleType;

#define COMM_MAX_USERS  8u
#define COMM_MAX_CHANNELS 4u

typedef struct {
    uint8 numUsers;
    uint8 numChannels;
} ComM_ConfigType;

void           ComM_Init(const ComM_ConfigType* ConfigPtr);
void           ComM_DeInit(void);
void           ComM_MainFunction(void);
Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode);
Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode);
void           ComM_CommunicationAllowed(uint8 Channel, boolean Allowed);

#endif /* COMM_H */
