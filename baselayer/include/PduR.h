/* PduR.h — PDU Router (ADR-005 / P4).
 *
 * Routes PDUs between Com and bus interface modules (CanIf, EthIf, etc.).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef PDUR_H
#define PDUR_H

#include "Std_Types.h"

void PduR_Init(void);
void PduR_DeInit(void);

/* TX path: Com -> PduR -> appropriate bus interface. */
Std_ReturnType PduR_ComTransmit(uint16_t PduId, const uint8_t* Data, uint8_t Length,
                                uint8_t BusType);

/* RX path: bus interface -> PduR -> Com.
 * Called by CanIf/EthIf/LinIf/FrIf when a frame arrives. */
void PduR_RxIndication(uint16_t PduId, const uint8_t* Data, uint8_t Length);

#endif /* PDUR_H */
