/* CanIf.h — CAN Interface (ADR-005 / P4).
 *
 * Delegates TX to ctx.push_tx_frame(), processes RX from ctx.pop_rx_frame().
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CANIF_H
#define CANIF_H

#include "Std_Types.h"

void CanIf_Init(void);
void CanIf_DeInit(void);

/* Transmit a CAN frame via ctx.push_tx_frame. */
Std_ReturnType CanIf_Transmit(uint32_t CanId, const uint8_t* Data, uint8_t Length);

/* Process inbound CAN frames via ctx.pop_rx_frame.
 * Called from Com_MainFunction cycle. */
void CanIf_RxMainFunction(void);

#endif /* CANIF_H */
