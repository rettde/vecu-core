/* PduR.c — PDU Router implementation (ADR-005 / P4).
 *
 * Routes TX PDUs from Com to the correct bus interface (CanIf, EthIf, etc.)
 * and RX indications from bus interfaces back to Com.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "PduR.h"
#include "Com.h"
#include <stddef.h>
#include "CanIf.h"
#include "EthIf.h"
#include "LinIf.h"
#include "FrIf.h"
#include "vecu_frame.h"

static boolean g_initialized = FALSE;

void PduR_Init(void) {
    g_initialized = TRUE;
}

void PduR_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType PduR_ComTransmit(uint16_t PduId, const uint8_t* Data, uint8_t Length,
                                uint8_t BusType) {
    if (!g_initialized || Data == NULL) { return E_NOT_OK; }

    /* Route to the appropriate bus interface based on bus type. */
    switch (BusType) {
        case VECU_BUS_CAN:
            return CanIf_Transmit((uint32_t)PduId, Data, Length);
        case VECU_BUS_ETH:
            return EthIf_Transmit((uint32_t)PduId, Data, Length);
        case VECU_BUS_LIN:
            return LinIf_Transmit((uint32_t)PduId, Data, Length);
        case VECU_BUS_FLEXRAY:
            return FrIf_Transmit((uint32_t)PduId, Data, Length);
        default:
            return E_NOT_OK;
    }
}

void PduR_RxIndication(uint16_t PduId, const uint8_t* Data, uint8_t Length) {
    if (!g_initialized || Data == NULL) { return; }
    /* Forward to Com for signal unpacking. */
    Com_RxIndication(PduId, Data, Length);
}
