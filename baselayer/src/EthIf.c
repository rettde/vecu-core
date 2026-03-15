/* EthIf.c — Ethernet Interface stub (ADR-005 / P4).
 *
 * Same pattern as CanIf but for BusType::Eth.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "EthIf.h"
#include "PduR.h"
#include "vecu_base_context.h"
#include "vecu_frame.h"
#include <string.h>

extern const vecu_base_context_t* Base_GetCtx(void);

static boolean g_initialized = FALSE;

void EthIf_Init(void) {
    g_initialized = TRUE;
}

void EthIf_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType EthIf_Transmit(uint32_t FrameId, const uint8_t* Data, uint8_t Length) {
    if (!g_initialized || Data == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = FrameId;
    frame.bus_type = VECU_BUS_ETH;
    frame.len      = Length;
    memcpy(frame.data, Data, Length);

    int rc = ctx->push_tx_frame(&frame);
    return (rc == VECU_OK) ? E_OK : E_NOT_OK;
}

void EthIf_RxMainFunction(void) {
    /* Stub — same pattern as CanIf, filter for VECU_BUS_ETH. */
    (void)0;
}
