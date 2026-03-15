/* EthIf.h — Ethernet Interface stub (ADR-005 / P4).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef ETHIF_H
#define ETHIF_H

#include "Std_Types.h"

void EthIf_Init(void);
void EthIf_DeInit(void);
Std_ReturnType EthIf_Transmit(uint32_t FrameId, const uint8_t* Data, uint8_t Length);
void EthIf_RxMainFunction(void);

#endif /* ETHIF_H */
