/* Can.c — Virtual CAN Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes CAN frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 * Implements AUTOSAR controller state machine and callback chain:
 *   Can_MainFunction_Read  → CanIf_RxIndication()
 *   Can_MainFunction_Write → CanIf_TxConfirmation()
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Can.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>

extern void CanIf_RxIndication(Can_HwHandleType Hrh, uint32 CanId,
                                uint8 CanDlc, const uint8* CanSduPtr);
extern void CanIf_TxConfirmation(Can_HwHandleType Hth);
extern void CanIf_ControllerModeIndication(uint8 ControllerId,
                                            Can_ControllerStateType ControllerMode);

typedef struct {
    uint32 id;
    uint8  dlc;
    uint8  data[64];
} Can_RxEntry;

typedef struct {
    Can_HwHandleType hth;
} Can_TxConfEntry;

static Can_ControllerStateType g_ctrl_mode = CAN_CS_UNINIT;

static Can_RxEntry      g_rx_queue[CAN_RX_QUEUE_SIZE];
static uint16           g_rx_head = 0;
static uint16           g_rx_count = 0;

static Can_TxConfEntry  g_tx_conf_queue[CAN_TX_QUEUE_SIZE];
static uint16           g_tx_conf_head = 0;
static uint16           g_tx_conf_count = 0;

void Can_Init(const Can_ConfigType* Config) {
    (void)Config;
    g_ctrl_mode = CAN_CS_STOPPED;
    g_rx_head = 0;
    g_rx_count = 0;
    g_tx_conf_head = 0;
    g_tx_conf_count = 0;
    memset(g_rx_queue, 0, sizeof(g_rx_queue));
    memset(g_tx_conf_queue, 0, sizeof(g_tx_conf_queue));
}

void Can_DeInit(void) {
    g_ctrl_mode = CAN_CS_UNINIT;
    g_rx_count = 0;
    g_tx_conf_count = 0;
}

Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition) {
    (void)Controller;
    switch (Transition) {
        case CAN_T_START:
            if (g_ctrl_mode == CAN_CS_STOPPED) {
                g_ctrl_mode = CAN_CS_STARTED;
                CanIf_ControllerModeIndication(0, CAN_CS_STARTED);
                return E_OK;
            }
            break;
        case CAN_T_STOP:
            if (g_ctrl_mode == CAN_CS_STARTED || g_ctrl_mode == CAN_CS_SLEEP) {
                g_ctrl_mode = CAN_CS_STOPPED;
                CanIf_ControllerModeIndication(0, CAN_CS_STOPPED);
                return E_OK;
            }
            break;
        case CAN_T_SLEEP:
            if (g_ctrl_mode == CAN_CS_STOPPED) {
                g_ctrl_mode = CAN_CS_SLEEP;
                CanIf_ControllerModeIndication(0, CAN_CS_SLEEP);
                return E_OK;
            }
            break;
        case CAN_T_WAKEUP:
            if (g_ctrl_mode == CAN_CS_SLEEP) {
                g_ctrl_mode = CAN_CS_STOPPED;
                CanIf_ControllerModeIndication(0, CAN_CS_STOPPED);
                return E_OK;
            }
            break;
        default:
            break;
    }
    return E_NOT_OK;
}

Can_ControllerStateType Can_GetControllerMode(uint8 Controller) {
    (void)Controller;
    return g_ctrl_mode;
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo) {
    if (g_ctrl_mode != CAN_CS_STARTED || PduInfo == NULL || PduInfo->sdu == NULL) {
        return E_NOT_OK;
    }
    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = PduInfo->id;
    frame.bus_type = VECU_BUS_CAN;
    frame.len      = PduInfo->length;
    if (PduInfo->length > 0) {
        memcpy(frame.data, PduInfo->sdu, PduInfo->length);
    }

    int rc = ctx->push_tx_frame(&frame);
    if (rc == VECU_OK) {
        if (g_tx_conf_count < CAN_TX_QUEUE_SIZE) {
            uint16 idx = (uint16)((g_tx_conf_head + g_tx_conf_count) % CAN_TX_QUEUE_SIZE);
            g_tx_conf_queue[idx].hth = Hth;
            g_tx_conf_count++;
        }
        return E_OK;
    }
    return E_NOT_OK;
}

void Can_MainFunction_Read(void) {
    if (g_ctrl_mode != CAN_CS_STARTED) { return; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL) { return; }

    vecu_frame_t frame;
    while (1) {
        memset(&frame, 0, sizeof(frame));
        int rc = ctx->pop_rx_frame(&frame);
        if (rc != VECU_OK) { break; }

        if (frame.bus_type == VECU_BUS_CAN) {
            uint8 dlc = (frame.len <= 64u) ? (uint8)frame.len : 64u;
            CanIf_RxIndication(0, frame.id, dlc, frame.data);
        }
    }
}

void Can_MainFunction_Write(void) {
    if (g_ctrl_mode != CAN_CS_STARTED) { return; }

    while (g_tx_conf_count > 0) {
        Can_HwHandleType hth = g_tx_conf_queue[g_tx_conf_head].hth;
        g_tx_conf_head = (uint16)((g_tx_conf_head + 1u) % CAN_TX_QUEUE_SIZE);
        g_tx_conf_count--;
        CanIf_TxConfirmation(hth);
    }
}
