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

#include "Can_GeneralTypes.h"

#define CAN_MAX_CONTROLLERS 8u
#define CAN_MAX_MAILBOXES   24u
#define CAN_RX_QUEUE_SIZE   32u
#define CAN_TX_QUEUE_SIZE   16u

typedef struct {
    uint16 numControllers;
    uint16 numHth;
    uint16 numHrh;
} Can_ConfigType;

void            Can_Init(const Can_ConfigType* Config);
void            Can_DeInit(void);
Std_ReturnType  Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition);
Can_ControllerStateType Can_GetControllerMode(uint8 Controller);
Std_ReturnType  Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
Std_ReturnType  Can_GetControllerErrorState(uint8 Controller,
                                             Can_ErrorStateType* ErrorStatePtr);
void            Can_MainFunction_Read(void);
void            Can_MainFunction_Write(void);
void            Can_MainFunction_Mode(void);
void            Can_MainFunction_BusOff(void);
void            Can_ConfigureRxMailboxes(const uint8* hoh_list, uint8 count);

typedef void (*Can_RxIndicationFnType)(uint16 Hrh, uint32 CanId, uint8 CanDlc,
                                       const uint8* CanSduPtr);
void            Can_SetRxIndicationCallback(Can_RxIndicationFnType fn);

typedef void (*Can_CtrlModeIndFnType)(uint8 ControllerId, uint8 ControllerMode);
void            Can_SetCtrlModeIndicationCallback(Can_CtrlModeIndFnType fn);

#endif /* VMCAL_CAN_H */
