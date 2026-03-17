/* Eth.c — Virtual Ethernet Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes Ethernet frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Eth.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>

#define ETH_TX_BUF_SIZE  VECU_MAX_FRAME_DATA
#define ETH_TX_BUF_COUNT 4u

static boolean g_initialized = FALSE;
static uint8   g_tx_bufs[ETH_TX_BUF_COUNT][ETH_TX_BUF_SIZE];
static uint16  g_tx_lens[ETH_TX_BUF_COUNT];

void Eth_Init(const Eth_ConfigType* CfgPtr) {
    (void)CfgPtr;
    memset(g_tx_bufs, 0, sizeof(g_tx_bufs));
    memset(g_tx_lens, 0, sizeof(g_tx_lens));
    g_initialized = TRUE;
}

void Eth_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType Eth_ProvideTxBuffer(uint8 CtrlIdx, Eth_BufIdxType* BufIdxPtr,
                                   uint8** BufPtr, uint16* LenBytePtr) {
    (void)CtrlIdx;
    if (!g_initialized || BufIdxPtr == NULL || BufPtr == NULL || LenBytePtr == NULL) {
        return E_NOT_OK;
    }
    uint8 idx = 0;
    *BufIdxPtr = idx;
    *BufPtr    = g_tx_bufs[idx];
    *LenBytePtr = ETH_TX_BUF_SIZE;
    return E_OK;
}

Std_ReturnType Eth_Transmit(uint8 CtrlIdx, Eth_BufIdxType BufIdx,
                            Eth_FrameType FrameType, boolean TxConfirmation,
                            uint16 LenByte, const uint8* PhysAddrPtr) {
    (void)CtrlIdx;
    (void)FrameType;
    (void)TxConfirmation;
    (void)PhysAddrPtr;
    if (!g_initialized || BufIdx >= ETH_TX_BUF_COUNT) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.bus_type = VECU_BUS_ETH;
    frame.len      = LenByte;
    if (LenByte > VECU_MAX_FRAME_DATA) { return E_NOT_OK; }
    memcpy(frame.data, g_tx_bufs[BufIdx], LenByte);

    int rc = ctx->push_tx_frame(&frame);
    return (rc == VECU_OK) ? E_OK : E_NOT_OK;
}

void Eth_Receive(uint8 CtrlIdx) {
    (void)CtrlIdx;
}

void Eth_MainFunction(void) {
    if (!g_initialized) { return; }
}
