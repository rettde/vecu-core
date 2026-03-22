/* Can.c — Virtual CAN Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes CAN frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 * Implements AUTOSAR controller state machine and callback chain:
 *   Can_MainFunction_Read  → CanIf_RxIndication(Can_HwType*, PduInfoType*)
 *   Can_MainFunction_Write → CanIf_TxConfirmation(PduIdType)
 *   Can_MainFunction_Mode  → CanIf_ControllerModeIndication(deferred)
 *
 * Multi-controller support (up to CAN_MAX_CONTROLLERS).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Can.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>
#include <stdio.h>

static void can_log(int level, const char* msg) {
    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx != NULL && ctx->log_fn != NULL) {
        ctx->log_fn(level, msg);
    }
}

extern void CanIf_RxIndication(const Can_HwType* Mailbox,
                                const PduInfoType* PduInfoPtr);
extern void CanIf_TxConfirmation(PduIdType CanTxPduId);
extern void CanIf_ControllerModeIndication(uint8 ControllerId,
                                            Can_ControllerStateType ControllerMode);

typedef struct {
    Can_HwHandleType hth;
    PduIdType        swPduHandle;
} Can_TxConfEntry;

typedef struct {
    Can_ControllerStateType current;
    Can_ControllerStateType pending;
    boolean                 mode_pending;
    Can_ErrorStateType      error_state;
} Can_CtrlState;

static Can_CtrlState    g_ctrl[CAN_MAX_CONTROLLERS];
static uint8            g_num_controllers = 0;

static Can_TxConfEntry  g_tx_conf_queue[CAN_TX_QUEUE_SIZE];
static uint16           g_tx_conf_head = 0;
static uint16           g_tx_conf_count = 0;

static uint8            g_rx_hohs[CAN_MAX_MAILBOXES];
static uint8            g_num_rx_hohs = 0;

static Can_RxIndicationFnType g_rx_indication_fn = NULL;

void Can_SetRxIndicationCallback(Can_RxIndicationFnType fn) {
    g_rx_indication_fn = fn;
}

static Can_CtrlModeIndFnType g_ctrl_mode_ind_fn = NULL;

void Can_SetCtrlModeIndicationCallback(Can_CtrlModeIndFnType fn) {
    g_ctrl_mode_ind_fn = fn;
}

static void can_default_rx_indication(uint16 Hrh, uint32 CanId, uint8 CanDlc,
                                       const uint8* CanSduPtr) {
    Can_HwType mailbox;
    PduInfoType pduInfo;
    mailbox.CanId = CanId;
    mailbox.Hoh = Hrh;
    mailbox.ControllerId = 0;
    pduInfo.SduDataPtr = (uint8*)CanSduPtr;
    pduInfo.SduLength = CanDlc;
    CanIf_RxIndication(&mailbox, &pduInfo);
}

void Can_Init(const Can_ConfigType* Config) {
    (void)Config;
    g_num_controllers = CAN_MAX_CONTROLLERS;
    uint8 i;
    for (i = 0; i < CAN_MAX_CONTROLLERS; i++) {
        g_ctrl[i].current      = CAN_CS_STOPPED;
        g_ctrl[i].pending      = CAN_CS_UNINIT;
        g_ctrl[i].mode_pending = FALSE;
        g_ctrl[i].error_state  = CAN_ERRORSTATE_ACTIVE;
    }
    g_tx_conf_head = 0;
    g_tx_conf_count = 0;
    memset(g_tx_conf_queue, 0, sizeof(g_tx_conf_queue));
}

void Can_DeInit(void) {
    uint8 i;
    for (i = 0; i < CAN_MAX_CONTROLLERS; i++) {
        g_ctrl[i].current = CAN_CS_UNINIT;
        g_ctrl[i].mode_pending = FALSE;
    }
    g_num_controllers = 0;
    g_tx_conf_count = 0;
}

Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition) {
    if (Controller >= g_num_controllers) { return E_NOT_OK; }
    Can_CtrlState* cs = &g_ctrl[Controller];
    Can_ControllerStateType target = CAN_CS_UNINIT;

    switch (Transition) {
        case CAN_T_START:
            if (cs->current != CAN_CS_STOPPED) { return E_NOT_OK; }
            target = CAN_CS_STARTED;
            break;
        case CAN_T_STOP:
            if (cs->current != CAN_CS_STARTED && cs->current != CAN_CS_SLEEP) {
                return E_NOT_OK;
            }
            target = CAN_CS_STOPPED;
            break;
        case CAN_T_SLEEP:
            if (cs->current != CAN_CS_STOPPED) { return E_NOT_OK; }
            target = CAN_CS_SLEEP;
            break;
        case CAN_T_WAKEUP:
            if (cs->current != CAN_CS_SLEEP) { return E_NOT_OK; }
            target = CAN_CS_STOPPED;
            break;
        default:
            return E_NOT_OK;
    }

    cs->pending = target;
    cs->mode_pending = TRUE;
    return E_OK;
}

Can_ControllerStateType Can_GetControllerMode(uint8 Controller) {
    if (Controller >= g_num_controllers) { return CAN_CS_UNINIT; }
    return g_ctrl[Controller].current;
}

Std_ReturnType Can_GetControllerErrorState(uint8 Controller,
                                            Can_ErrorStateType* ErrorStatePtr) {
    if (Controller >= g_num_controllers || ErrorStatePtr == NULL) { return E_NOT_OK; }
    *ErrorStatePtr = g_ctrl[Controller].error_state;
    return E_OK;
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo) {
    if (PduInfo == NULL || PduInfo->sdu == NULL) { return E_NOT_OK; }
    if (g_num_controllers == 0 || g_ctrl[0].current != CAN_CS_STARTED) {
        return E_NOT_OK;
    }
    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }
    {
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "vmcal Can: TX id=0x%08X len=%u hth=%u",
                 (unsigned)PduInfo->id, (unsigned)PduInfo->length, (unsigned)Hth);
        can_log(2, buf);
    }

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
            g_tx_conf_queue[idx].swPduHandle = PduInfo->swPduHandle;
            g_tx_conf_count++;
        }
        return E_OK;
    }
    return E_NOT_OK;
}

void Can_ConfigureRxMailboxes(const uint8* hoh_list, uint8 count) {
    uint8 n = (count <= CAN_MAX_MAILBOXES) ? count : CAN_MAX_MAILBOXES;
    uint8 i;
    for (i = 0; i < n; i++) {
        g_rx_hohs[i] = hoh_list[i];
    }
    g_num_rx_hohs = n;
}

void Can_MainFunction_Read(void) {
    uint8 i;
    boolean any_started = FALSE;
    for (i = 0; i < g_num_controllers; i++) {
        if (g_ctrl[i].current == CAN_CS_STARTED) { any_started = TRUE; break; }
    }
    if (!any_started) { return; }

    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL) { return; }

    vecu_frame_t frame;
    while (1) {
        memset(&frame, 0, sizeof(frame));
        int rc = ctx->pop_rx_frame(&frame);
        if (rc != VECU_OK) { break; }

        if (frame.bus_type == VECU_BUS_CAN) {
            uint8 dlc = (frame.len <= 64u) ? (uint8)frame.len : 64u;

            {
                char buf[96];
                snprintf(buf, sizeof(buf),
                         "vmcal Can: RX id=0x%08X len=%u hohs=%u",
                         (unsigned)frame.id, (unsigned)dlc, (unsigned)g_num_rx_hohs);
                can_log(2, buf);
            }

            Can_RxIndicationFnType rx_fn = g_rx_indication_fn
                                            ? g_rx_indication_fn
                                            : can_default_rx_indication;
            if (g_num_rx_hohs == 0) {
                rx_fn(0u, frame.id, dlc, frame.data);
            } else {
                for (i = 0; i < g_num_rx_hohs; i++) {
                    rx_fn(g_rx_hohs[i], frame.id, dlc, frame.data);
                }
            }
        }
    }
}

void Can_MainFunction_Write(void) {
    boolean any_started = FALSE;
    uint8 i;
    for (i = 0; i < g_num_controllers; i++) {
        if (g_ctrl[i].current == CAN_CS_STARTED) { any_started = TRUE; break; }
    }
    if (!any_started) { return; }

    while (g_tx_conf_count > 0) {
        PduIdType swPduHandle = g_tx_conf_queue[g_tx_conf_head].swPduHandle;
        g_tx_conf_head = (uint16)((g_tx_conf_head + 1u) % CAN_TX_QUEUE_SIZE);
        g_tx_conf_count--;
        CanIf_TxConfirmation(swPduHandle);
    }
}

void Can_MainFunction_Mode(void) {
    uint8 i;
    for (i = 0; i < g_num_controllers; i++) {
        Can_CtrlState* cs = &g_ctrl[i];
        if (cs->mode_pending) {
            cs->current = cs->pending;
            cs->mode_pending = FALSE;
            if (g_ctrl_mode_ind_fn != NULL) {
                g_ctrl_mode_ind_fn(i, (uint8)cs->current);
            } else {
                CanIf_ControllerModeIndication(i, cs->current);
            }
        }
    }
}

void Can_MainFunction_BusOff(void) {
    /* No bus-off in virtual environment — no-op. */
}
