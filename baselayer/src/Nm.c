/* Nm.c — Network Management (simplified stub).
 *
 * Minimal AUTOSAR Nm state machine for Level-3 vECU.
 * Per-channel state: BUS_SLEEP → REPEAT_MESSAGE → NORMAL_OPERATION → READY_SLEEP.
 * Timeout-based transitions driven by Nm_MainFunction.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Nm.h"
#include <string.h>

typedef struct {
    Nm_StateType state;
    boolean      requested;
    uint32       timer;
} Nm_ChannelState;

static boolean          g_initialized = FALSE;
static Nm_ChannelState  g_channels[NM_MAX_CHANNELS];
static uint8            g_num_channels = 0;
static uint32           g_timeout_ticks = 100;
static uint32           g_repeat_msg_ticks = 10;

void Nm_Init(const Nm_ConfigType* ConfigPtr) {
    memset(g_channels, 0, sizeof(g_channels));
    uint8 i;
    for (i = 0; i < NM_MAX_CHANNELS; i++) {
        g_channels[i].state = NM_STATE_BUS_SLEEP;
    }
    if (ConfigPtr != NULL) {
        g_num_channels = (ConfigPtr->numChannels < NM_MAX_CHANNELS) ? ConfigPtr->numChannels : NM_MAX_CHANNELS;
        g_timeout_ticks = ConfigPtr->timeoutTicks;
        g_repeat_msg_ticks = ConfigPtr->repeatMessageTicks;
    } else {
        g_num_channels = 1;
    }
    g_initialized = TRUE;
}

void Nm_DeInit(void) {
    uint8 i;
    for (i = 0; i < NM_MAX_CHANNELS; i++) {
        g_channels[i].state = NM_STATE_UNINIT;
    }
    g_initialized = FALSE;
}

Std_ReturnType Nm_NetworkRequest(uint8 nmChannelHandle) {
    if (!g_initialized || nmChannelHandle >= g_num_channels) { return E_NOT_OK; }
    Nm_ChannelState* ch = &g_channels[nmChannelHandle];
    ch->requested = TRUE;
    if (ch->state == NM_STATE_BUS_SLEEP || ch->state == NM_STATE_PREPARE_BUS_SLEEP) {
        ch->state = NM_STATE_REPEAT_MESSAGE;
        ch->timer = g_repeat_msg_ticks;
    }
    return E_OK;
}

Std_ReturnType Nm_NetworkRelease(uint8 nmChannelHandle) {
    if (!g_initialized || nmChannelHandle >= g_num_channels) { return E_NOT_OK; }
    g_channels[nmChannelHandle].requested = FALSE;
    return E_OK;
}

Std_ReturnType Nm_GetState(uint8 nmChannelHandle, Nm_StateType* nmStatePtr,
                            Nm_ModeType* nmModePtr) {
    if (!g_initialized || nmChannelHandle >= g_num_channels) { return E_NOT_OK; }
    if (nmStatePtr != NULL) { *nmStatePtr = g_channels[nmChannelHandle].state; }
    if (nmModePtr != NULL) {
        *nmModePtr = (g_channels[nmChannelHandle].state == NM_STATE_BUS_SLEEP)
                     ? NM_MODE_BUS_SLEEP : NM_MODE_NETWORK;
    }
    return E_OK;
}

void Nm_NetworkStartIndication(uint8 nmChannelHandle) {
    if (!g_initialized || nmChannelHandle >= g_num_channels) { return; }
    Nm_ChannelState* ch = &g_channels[nmChannelHandle];
    if (ch->state == NM_STATE_BUS_SLEEP) {
        ch->state = NM_STATE_REPEAT_MESSAGE;
        ch->timer = g_repeat_msg_ticks;
    }
}

void Nm_MainFunction(void) {
    if (!g_initialized) { return; }

    uint8 i;
    for (i = 0; i < g_num_channels; i++) {
        Nm_ChannelState* ch = &g_channels[i];
        if (ch->timer > 0) { ch->timer--; }

        switch (ch->state) {
        case NM_STATE_REPEAT_MESSAGE:
            if (ch->timer == 0) {
                ch->state = NM_STATE_NORMAL_OPERATION;
                ch->timer = g_timeout_ticks;
            }
            break;
        case NM_STATE_NORMAL_OPERATION:
            if (!ch->requested) {
                ch->state = NM_STATE_READY_SLEEP;
                ch->timer = g_timeout_ticks;
            } else if (ch->timer == 0) {
                ch->timer = g_timeout_ticks;
            }
            break;
        case NM_STATE_READY_SLEEP:
            if (ch->requested) {
                ch->state = NM_STATE_NORMAL_OPERATION;
                ch->timer = g_timeout_ticks;
            } else if (ch->timer == 0) {
                ch->state = NM_STATE_PREPARE_BUS_SLEEP;
                ch->timer = g_timeout_ticks;
            }
            break;
        case NM_STATE_PREPARE_BUS_SLEEP:
            if (ch->requested) {
                ch->state = NM_STATE_REPEAT_MESSAGE;
                ch->timer = g_repeat_msg_ticks;
            } else if (ch->timer == 0) {
                ch->state = NM_STATE_BUS_SLEEP;
            }
            break;
        default:
            break;
        }
    }
}
