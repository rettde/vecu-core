/* Lin.c — Virtual LIN Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes LIN frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Lin.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>

static boolean        g_initialized = FALSE;
static Lin_StatusType g_status      = LIN_NOT_OK;
static uint8          g_rx_buf[8];

void Lin_Init(const Lin_ConfigType* Config) {
    (void)Config;
    memset(g_rx_buf, 0, sizeof(g_rx_buf));
    g_status      = LIN_CH_SLEEP;
    g_initialized = TRUE;
}

void Lin_DeInit(void) {
    g_initialized = FALSE;
    g_status      = LIN_NOT_OK;
}

Std_ReturnType Lin_SendFrame(uint8 Channel, const Lin_PduType* PduInfoPtr) {
    (void)Channel;
    if (!g_initialized || PduInfoPtr == NULL || PduInfoPtr->SduPtr == NULL) {
        return E_NOT_OK;
    }
    if (g_status == LIN_CH_SLEEP) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = (uint32_t)PduInfoPtr->Pid;
    frame.bus_type = VECU_BUS_LIN;
    frame.len      = PduInfoPtr->Dl;
    if (PduInfoPtr->Dl > 0 && PduInfoPtr->Dl <= 8) {
        memcpy(frame.data, PduInfoPtr->SduPtr, PduInfoPtr->Dl);
    }

    int rc = ctx->push_tx_frame(&frame);
    if (rc == VECU_OK) {
        g_status = LIN_TX_OK;
        return E_OK;
    }
    g_status = LIN_TX_ERROR;
    return E_NOT_OK;
}

Lin_StatusType Lin_GetStatus(uint8 Channel, const uint8** Lin_SduPtr) {
    (void)Channel;
    if (Lin_SduPtr != NULL) {
        *Lin_SduPtr = g_rx_buf;
    }
    return g_status;
}

Std_ReturnType Lin_GoToSleep(uint8 Channel) {
    (void)Channel;
    if (!g_initialized) { return E_NOT_OK; }
    g_status = LIN_CH_SLEEP;
    return E_OK;
}

Std_ReturnType Lin_Wakeup(uint8 Channel) {
    (void)Channel;
    if (!g_initialized) { return E_NOT_OK; }
    if (g_status == LIN_CH_SLEEP) {
        g_status = LIN_NOT_OK;
    }
    return E_OK;
}

void Lin_MainFunction(void) {
    if (!g_initialized || g_status == LIN_CH_SLEEP) { return; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL) { return; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    int rc = ctx->pop_rx_frame(&frame);
    if (rc == VECU_OK && frame.bus_type == VECU_BUS_LIN) {
        uint8 len = (frame.len <= 8u) ? (uint8)frame.len : 8u;
        memcpy(g_rx_buf, frame.data, len);
        g_status = LIN_RX_OK;
    }
}
