/* Nm.h — Network Management (simplified stub).
 *
 * Minimal AUTOSAR Nm state machine for Level-3 vECU.
 * States: BUS_SLEEP → PREPARE_BUS_SLEEP → REPEAT_MESSAGE → NORMAL_OPERATION.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef NM_H
#define NM_H

#include "Std_Types.h"

typedef uint8 Nm_StateType;

#define NM_STATE_UNINIT              0u
#define NM_STATE_BUS_SLEEP           1u
#define NM_STATE_PREPARE_BUS_SLEEP   2u
#define NM_STATE_REPEAT_MESSAGE      3u
#define NM_STATE_NORMAL_OPERATION    4u
#define NM_STATE_READY_SLEEP         5u

typedef uint8 Nm_ModeType;

#define NM_MODE_BUS_SLEEP   0u
#define NM_MODE_NETWORK     1u

#define NM_MAX_CHANNELS 4u

typedef struct {
    uint8 numChannels;
    uint32 timeoutTicks;
    uint32 repeatMessageTicks;
} Nm_ConfigType;

void           Nm_Init(const Nm_ConfigType* ConfigPtr);
void           Nm_DeInit(void);
void           Nm_MainFunction(void);
Std_ReturnType Nm_NetworkRequest(uint8 nmChannelHandle);
Std_ReturnType Nm_NetworkRelease(uint8 nmChannelHandle);
Std_ReturnType Nm_GetState(uint8 nmChannelHandle, Nm_StateType* nmStatePtr,
                            Nm_ModeType* nmModePtr);
void           Nm_NetworkStartIndication(uint8 nmChannelHandle);

#endif /* NM_H */
