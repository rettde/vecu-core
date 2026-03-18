/* Fr.h — Virtual FlexRay Driver API (ADR-002 / Virtual-MCAL).
 *
 * Drop-in replacement for Fr_* MCAL driver.
 * Routes FlexRay frames through vecu_base_context_t callbacks.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_FR_H
#define VMCAL_FR_H

#include "Std_Types.h"
#include "ComStack_Types.h"

#define FR_RX_QUEUE_SIZE 16u
#define FR_TX_QUEUE_SIZE 8u

typedef enum {
    FR_POCSTATE_DEFAULT_CONFIG = 0x00u,
    FR_POCSTATE_READY          = 0x01u,
    FR_POCSTATE_NORMAL_ACTIVE  = 0x02u,
    FR_POCSTATE_NORMAL_PASSIVE = 0x03u,
    FR_POCSTATE_HALT           = 0x04u,
    FR_POCSTATE_CONFIG         = 0x05u
} Fr_POCStateType;

typedef struct {
    uint8 numCtrl;
} Fr_ConfigType;

void            Fr_Init(const Fr_ConfigType* Fr_ConfigPtr);
void            Fr_DeInit(void);
Std_ReturnType  Fr_ControllerInit(uint8 Fr_CtrlIdx);
Std_ReturnType  Fr_StartCommunication(uint8 Fr_CtrlIdx);
Std_ReturnType  Fr_HaltCommunication(uint8 Fr_CtrlIdx);
Fr_POCStateType Fr_GetPOCState(uint8 Fr_CtrlIdx);
Std_ReturnType  Fr_TransmitTxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                  const uint8* Fr_LSduPtr, uint8 Fr_LSduLength);
Std_ReturnType  Fr_ReceiveRxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                 uint8* Fr_LSduPtr, uint8* Fr_LSduLengthPtr);
void            Fr_MainFunction(void);

#endif /* VMCAL_FR_H */
