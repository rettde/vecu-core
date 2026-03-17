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

typedef struct {
    uint8 numCtrl;
} Fr_ConfigType;

void            Fr_Init(const Fr_ConfigType* Fr_ConfigPtr);
void            Fr_DeInit(void);
Std_ReturnType  Fr_TransmitTxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                  const uint8* Fr_LSduPtr, uint8 Fr_LSduLength);
Std_ReturnType  Fr_ReceiveRxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                 uint8* Fr_LSduPtr, uint8* Fr_LSduLengthPtr);
void            Fr_MainFunction(void);

#endif /* VMCAL_FR_H */
