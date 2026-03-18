/* Eth.c — Virtual Ethernet Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes Ethernet frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 * Implements AUTOSAR controller mode and EthIf callback chain:
 *   Eth_MainFunction → EthIf_RxIndication()
 *   Eth_Transmit     → EthIf_TxConfirmation() (when requested)
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Eth.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>

#define ETH_TX_BUF_SIZE  VECU_MAX_FRAME_DATA

extern void EthIf_RxIndication(uint8 CtrlIdx, Eth_FrameType FrameType,
                                boolean IsBroadcast, const uint8* PhysAddrPtr,
                                const uint8* DataPtr, uint16 LenByte);
extern void EthIf_TxConfirmation(uint8 CtrlIdx, Eth_BufIdxType BufIdx);
extern void EthIf_CtrlModeIndication(uint8 CtrlIdx, Eth_ModeType CtrlMode);

static Eth_ModeType g_mode = ETH_MODE_DOWN;
static uint8   g_phys_addr[ETH_PHYS_ADDR_LEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
static uint8   g_tx_bufs[ETH_TX_BUF_COUNT][ETH_TX_BUF_SIZE];
static boolean g_tx_buf_used[ETH_TX_BUF_COUNT];
static boolean g_tx_conf_pending[ETH_TX_BUF_COUNT];

void Eth_Init(const Eth_ConfigType* CfgPtr) {
    (void)CfgPtr;
    memset(g_tx_bufs, 0, sizeof(g_tx_bufs));
    memset(g_tx_buf_used, FALSE, sizeof(g_tx_buf_used));
    memset(g_tx_conf_pending, FALSE, sizeof(g_tx_conf_pending));
    g_mode = ETH_MODE_DOWN;
}

void Eth_DeInit(void) {
    g_mode = ETH_MODE_DOWN;
}

Std_ReturnType Eth_SetControllerMode(uint8 CtrlIdx, Eth_ModeType CtrlMode) {
    (void)CtrlIdx;
    g_mode = CtrlMode;
    EthIf_CtrlModeIndication(0, CtrlMode);
    return E_OK;
}

Eth_ModeType Eth_GetControllerMode(uint8 CtrlIdx) {
    (void)CtrlIdx;
    return g_mode;
}

void Eth_GetPhysAddr(uint8 CtrlIdx, uint8* PhysAddrPtr) {
    (void)CtrlIdx;
    if (PhysAddrPtr != NULL) {
        memcpy(PhysAddrPtr, g_phys_addr, ETH_PHYS_ADDR_LEN);
    }
}

Std_ReturnType Eth_ProvideTxBuffer(uint8 CtrlIdx, Eth_BufIdxType* BufIdxPtr,
                                   uint8** BufPtr, uint16* LenBytePtr) {
    (void)CtrlIdx;
    if (g_mode != ETH_MODE_ACTIVE || BufIdxPtr == NULL || BufPtr == NULL || LenBytePtr == NULL) {
        return E_NOT_OK;
    }
    uint8 i;
    for (i = 0; i < ETH_TX_BUF_COUNT; i++) {
        if (!g_tx_buf_used[i]) {
            g_tx_buf_used[i] = TRUE;
            *BufIdxPtr = i;
            *BufPtr    = g_tx_bufs[i];
            *LenBytePtr = ETH_TX_BUF_SIZE;
            return E_OK;
        }
    }
    return E_NOT_OK;
}

Std_ReturnType Eth_Transmit(uint8 CtrlIdx, Eth_BufIdxType BufIdx,
                            Eth_FrameType FrameType, boolean TxConfirmation,
                            uint16 LenByte, const uint8* PhysAddrPtr) {
    (void)CtrlIdx;
    (void)FrameType;
    (void)PhysAddrPtr;
    if (g_mode != ETH_MODE_ACTIVE || BufIdx >= ETH_TX_BUF_COUNT) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    if (LenByte > VECU_MAX_FRAME_DATA) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.bus_type = VECU_BUS_ETH;
    frame.len      = LenByte;
    memcpy(frame.data, g_tx_bufs[BufIdx], LenByte);

    int rc = ctx->push_tx_frame(&frame);
    g_tx_buf_used[BufIdx] = FALSE;

    if (rc == VECU_OK) {
        if (TxConfirmation) {
            g_tx_conf_pending[BufIdx] = TRUE;
        }
        return E_OK;
    }
    return E_NOT_OK;
}

void Eth_Receive(uint8 CtrlIdx) {
    (void)CtrlIdx;
    if (g_mode != ETH_MODE_ACTIVE) { return; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL) { return; }

    vecu_frame_t frame;
    while (1) {
        memset(&frame, 0, sizeof(frame));
        int rc = ctx->pop_rx_frame(&frame);
        if (rc != VECU_OK) { break; }

        if (frame.bus_type == VECU_BUS_ETH) {
            uint16 len = (frame.len <= VECU_MAX_FRAME_DATA) ? (uint16)frame.len : VECU_MAX_FRAME_DATA;
            EthIf_RxIndication(0, 0x0800, FALSE, NULL_PTR, frame.data, len);
        }
    }
}

void Eth_MainFunction(void) {
    if (g_mode != ETH_MODE_ACTIVE) { return; }

    Eth_Receive(0);

    uint8 i;
    for (i = 0; i < ETH_TX_BUF_COUNT; i++) {
        if (g_tx_conf_pending[i]) {
            g_tx_conf_pending[i] = FALSE;
            EthIf_TxConfirmation(0, i);
        }
    }
}
