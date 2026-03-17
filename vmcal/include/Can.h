/* Can.h — Virtual CAN Driver API (ADR-002 / Virtual-MCAL).
 *
 * Drop-in replacement for Can_30_* MCAL driver.
 * Routes CAN frames through vecu_base_context_t callbacks.
 * Implements AUTOSAR controller state machine and CanIf callback chain.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_CAN_H
#define VMCAL_CAN_H

#include "Std_Types.h"

#define CAN_RX_QUEUE_SIZE 32u
#define CAN_TX_QUEUE_SIZE 16u

typedef uint16 Can_HwHandleType;

typedef enum {
    CAN_CS_UNINIT  = 0u,
    CAN_CS_STOPPED = 1u,
    CAN_CS_STARTED = 2u,
    CAN_CS_SLEEP   = 3u
} Can_ControllerStateType;

typedef enum {
    CAN_T_START = 0u,
    CAN_T_STOP  = 1u,
    CAN_T_SLEEP = 2u,
    CAN_T_WAKEUP = 3u
} Can_StateTransitionType;

typedef struct {
    uint32 id;
    uint8  length;
    uint8  _pad[3];
    const uint8* sdu;
} Can_PduType;

typedef struct {
    uint16 numHth;
    uint16 numHrh;
} Can_ConfigType;

void            Can_Init(const Can_ConfigType* Config);
void            Can_DeInit(void);
Std_ReturnType  Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition);
Can_ControllerStateType Can_GetControllerMode(uint8 Controller);
Std_ReturnType  Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void            Can_MainFunction_Read(void);
void            Can_MainFunction_Write(void);

#endif /* VMCAL_CAN_H */
