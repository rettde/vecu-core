/* CanIf.c — CAN Interface implementation (ADR-005 / P4).
 *
 * TX: packs PDU data into a vecu_frame_t and calls ctx.push_tx_frame().
 * RX: calls ctx.pop_rx_frame() and forwards CAN frames to PduR.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "CanIf.h"
#include "PduR.h"
#include "vecu_base_context.h"
#include "vecu_frame.h"
#include <string.h>

/* Forward declaration — defined in Base_Entry.c. */
extern const vecu_base_context_t* Base_GetCtx(void);

static boolean g_initialized = FALSE;

void CanIf_Init(void) {
    g_initialized = TRUE;
}

void CanIf_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType CanIf_Transmit(uint32_t CanId, const uint8_t* Data, uint8_t Length) {
    if (!g_initialized || Data == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = CanId;
    frame.bus_type = VECU_BUS_CAN;
    frame.len      = Length;
    memcpy(frame.data, Data, Length);

    int rc = ctx->push_tx_frame(&frame);
    return (rc == VECU_OK) ? E_OK : E_NOT_OK;
}

void CanIf_RxMainFunction(void) {
    if (!g_initialized) { return; }

    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL) { return; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    /* Pop all available CAN frames. */
    while (ctx->pop_rx_frame(&frame) == VECU_OK) {
        if (frame.bus_type == VECU_BUS_CAN) {
            uint8_t len = (frame.len <= 255u) ? (uint8_t)frame.len : 255u;
            /* Use frame.id as the PduId for now.
             * A real implementation would have a mapping table. */
            PduR_RxIndication((uint16_t)frame.id, frame.data, len);
        }
        memset(&frame, 0, sizeof(frame));
    }
}
