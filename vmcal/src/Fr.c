/* Fr.c — Virtual FlexRay Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes FlexRay frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Fr.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>

static boolean g_initialized = FALSE;

void Fr_Init(const Fr_ConfigType* Fr_ConfigPtr) {
    (void)Fr_ConfigPtr;
    g_initialized = TRUE;
}

void Fr_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType Fr_TransmitTxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                 const uint8* Fr_LSduPtr, uint8 Fr_LSduLength) {
    (void)Fr_CtrlIdx;
    if (!g_initialized || Fr_LSduPtr == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = (uint32_t)Fr_LPduIdx;
    frame.bus_type = VECU_BUS_FLEXRAY;
    frame.len      = Fr_LSduLength;
    if (Fr_LSduLength > 0) {
        memcpy(frame.data, Fr_LSduPtr, Fr_LSduLength);
    }

    int rc = ctx->push_tx_frame(&frame);
    return (rc == VECU_OK) ? E_OK : E_NOT_OK;
}

Std_ReturnType Fr_ReceiveRxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                uint8* Fr_LSduPtr, uint8* Fr_LSduLengthPtr) {
    (void)Fr_CtrlIdx;
    (void)Fr_LPduIdx;
    if (!g_initialized || Fr_LSduPtr == NULL || Fr_LSduLengthPtr == NULL) {
        return E_NOT_OK;
    }
    *Fr_LSduLengthPtr = 0;
    return E_NOT_OK;
}

void Fr_MainFunction(void) {
    if (!g_initialized) { return; }
}
