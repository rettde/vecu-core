/* Fr.c — Virtual FlexRay Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes FlexRay frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 * Implements POC state machine and FrIf callback chain:
 *   Fr_MainFunction → FrIf_RxIndication(PduIdType, PduInfoType*)
 *   Fr_MainFunction → FrIf_TxConfirmation(PduIdType)
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Fr.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>

extern void FrIf_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
extern void FrIf_TxConfirmation(PduIdType TxPduId);

typedef struct {
    PduIdType txPduId;
} Fr_TxConfEntry;

static Fr_POCStateType g_poc_state = FR_POCSTATE_DEFAULT_CONFIG;

static Fr_TxConfEntry  g_tx_conf[FR_TX_QUEUE_SIZE];
static uint16          g_tx_conf_head  = 0;
static uint16          g_tx_conf_count = 0;

void Fr_Init(const Fr_ConfigType* Fr_ConfigPtr) {
    (void)Fr_ConfigPtr;
    g_poc_state = FR_POCSTATE_DEFAULT_CONFIG;
    g_tx_conf_head  = 0;
    g_tx_conf_count = 0;
    memset(g_tx_conf, 0, sizeof(g_tx_conf));
}

void Fr_DeInit(void) {
    g_poc_state = FR_POCSTATE_DEFAULT_CONFIG;
    g_tx_conf_count = 0;
}

Std_ReturnType Fr_ControllerInit(uint8 Fr_CtrlIdx) {
    (void)Fr_CtrlIdx;
    if (g_poc_state == FR_POCSTATE_DEFAULT_CONFIG ||
        g_poc_state == FR_POCSTATE_CONFIG) {
        g_poc_state = FR_POCSTATE_READY;
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Fr_StartCommunication(uint8 Fr_CtrlIdx) {
    (void)Fr_CtrlIdx;
    if (g_poc_state == FR_POCSTATE_READY) {
        g_poc_state = FR_POCSTATE_NORMAL_ACTIVE;
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Fr_HaltCommunication(uint8 Fr_CtrlIdx) {
    (void)Fr_CtrlIdx;
    if (g_poc_state == FR_POCSTATE_NORMAL_ACTIVE ||
        g_poc_state == FR_POCSTATE_NORMAL_PASSIVE) {
        g_poc_state = FR_POCSTATE_HALT;
        return E_OK;
    }
    return E_NOT_OK;
}

Fr_POCStateType Fr_GetPOCState(uint8 Fr_CtrlIdx) {
    (void)Fr_CtrlIdx;
    return g_poc_state;
}

Std_ReturnType Fr_TransmitTxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                 const uint8* Fr_LSduPtr, uint8 Fr_LSduLength) {
    (void)Fr_CtrlIdx;
    if (g_poc_state != FR_POCSTATE_NORMAL_ACTIVE) { return E_NOT_OK; }
    if (Fr_LSduPtr == NULL) { return E_NOT_OK; }

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
    if (rc == VECU_OK) {
        if (g_tx_conf_count < FR_TX_QUEUE_SIZE) {
            uint16 idx = (uint16)((g_tx_conf_head + g_tx_conf_count) % FR_TX_QUEUE_SIZE);
            g_tx_conf[idx].txPduId = (PduIdType)Fr_LPduIdx;
            g_tx_conf_count++;
        }
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Fr_ReceiveRxLPdu(uint8 Fr_CtrlIdx, uint16 Fr_LPduIdx,
                                uint8* Fr_LSduPtr, uint8* Fr_LSduLengthPtr) {
    (void)Fr_CtrlIdx;
    (void)Fr_LPduIdx;
    if (g_poc_state != FR_POCSTATE_NORMAL_ACTIVE) { return E_NOT_OK; }
    if (Fr_LSduPtr == NULL || Fr_LSduLengthPtr == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL) {
        *Fr_LSduLengthPtr = 0;
        return E_NOT_OK;
    }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    int rc = ctx->pop_rx_frame(&frame);
    if (rc == VECU_OK && frame.bus_type == VECU_BUS_FLEXRAY) {
        uint8 len = (frame.len <= 254u) ? (uint8)frame.len : 254u;
        memcpy(Fr_LSduPtr, frame.data, len);
        *Fr_LSduLengthPtr = len;
        return E_OK;
    }

    *Fr_LSduLengthPtr = 0;
    return E_NOT_OK;
}

void Fr_MainFunction(void) {
    if (g_poc_state != FR_POCSTATE_NORMAL_ACTIVE) { return; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL) { return; }

    if (ctx->pop_rx_frame != NULL) {
        vecu_frame_t frame;
        while (1) {
            memset(&frame, 0, sizeof(frame));
            int rc = ctx->pop_rx_frame(&frame);
            if (rc != VECU_OK) { break; }

            if (frame.bus_type == VECU_BUS_FLEXRAY) {
                uint8 len = (frame.len <= 254u) ? (uint8)frame.len : 254u;
                PduInfoType pduInfo;
                pduInfo.SduDataPtr  = frame.data;
                pduInfo.MetaDataPtr = NULL_PTR;
                pduInfo.SduLength   = len;
                FrIf_RxIndication((PduIdType)frame.id, &pduInfo);
            }
        }
    }

    while (g_tx_conf_count > 0) {
        PduIdType txPduId = g_tx_conf[g_tx_conf_head].txPduId;
        g_tx_conf_head = (uint16)((g_tx_conf_head + 1u) % FR_TX_QUEUE_SIZE);
        g_tx_conf_count--;
        FrIf_TxConfirmation(txPduId);
    }
}
