/* Eth.h — Virtual Ethernet Driver API (ADR-002 / Virtual-MCAL).
 *
 * Drop-in replacement for Eth_* MCAL driver.
 * Routes Ethernet frames through vecu_base_context_t callbacks.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_ETH_H
#define VMCAL_ETH_H

#include "Std_Types.h"

typedef uint8  Eth_BufIdxType;
typedef uint16 Eth_FrameType;

typedef struct {
    uint8 numCtrl;
} Eth_ConfigType;

void            Eth_Init(const Eth_ConfigType* CfgPtr);
void            Eth_DeInit(void);
Std_ReturnType  Eth_ProvideTxBuffer(uint8 CtrlIdx, Eth_BufIdxType* BufIdxPtr,
                                    uint8** BufPtr, uint16* LenBytePtr);
Std_ReturnType  Eth_Transmit(uint8 CtrlIdx, Eth_BufIdxType BufIdx,
                             Eth_FrameType FrameType, boolean TxConfirmation,
                             uint16 LenByte, const uint8* PhysAddrPtr);
void            Eth_Receive(uint8 CtrlIdx);
void            Eth_MainFunction(void);

#endif /* VMCAL_ETH_H */
