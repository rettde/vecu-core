/* Lin.h — Virtual LIN Driver API (ADR-002 / Virtual-MCAL).
 *
 * Routes LIN frames through vecu_base_context_t callbacks.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_LIN_H
#define VMCAL_LIN_H

#include "Std_Types.h"

#ifndef LIN_GENERALTYPES_H
typedef uint8 Lin_FramePidType;
typedef uint8 Lin_FrameDlType;

typedef enum {
    LIN_FRAMERESPONSE_TX  = 0u,
    LIN_FRAMERESPONSE_RX  = 1u,
    LIN_FRAMERESPONSE_IGNORE = 2u
} Lin_FrameResponseType;

typedef struct {
    Lin_FramePidType       Pid;
    Lin_FrameDlType        Dl;
    Lin_FrameResponseType  Drc;
    uint8*                 SduPtr;
} Lin_PduType;

typedef enum {
    LIN_NOT_OK        = 0u,
    LIN_TX_OK         = 1u,
    LIN_TX_BUSY       = 2u,
    LIN_TX_HEADER_ERROR = 3u,
    LIN_TX_ERROR      = 4u,
    LIN_RX_OK         = 5u,
    LIN_RX_BUSY       = 6u,
    LIN_RX_ERROR      = 7u,
    LIN_RX_NO_RESPONSE = 8u,
    LIN_CH_SLEEP      = 9u
} Lin_StatusType;
#endif /* LIN_GENERALTYPES_H */

#if !defined(LIN_CFG_H)
typedef struct {
    uint8 numChannels;
} Lin_ConfigType;
#endif

void            Lin_Init(const Lin_ConfigType* Config);
void            Lin_DeInit(void);
Std_ReturnType  Lin_SendFrame(uint8 Channel, const Lin_PduType* PduInfoPtr);
Lin_StatusType  Lin_GetStatus(uint8 Channel, const uint8** Lin_SduPtr);
Std_ReturnType  Lin_GoToSleep(uint8 Channel);
Std_ReturnType  Lin_Wakeup(uint8 Channel);
void            Lin_MainFunction(void);

#endif /* VMCAL_LIN_H */
