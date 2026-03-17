/* Gpt.c — Virtual GPT Timer Driver (ADR-002 / Virtual-MCAL).
 *
 * Sim-time-based. Timers count down via Gpt_Tick() calls.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Gpt.h"
#include <stddef.h>
#include <string.h>

typedef struct {
    Gpt_ValueType        target;
    Gpt_ValueType        elapsed;
    Gpt_ChannelModeType  mode;
    Gpt_NotificationType notify;
    boolean              running;
} Gpt_ChannelState;

static Gpt_ChannelState g_ch[GPT_MAX_CHANNELS];
static boolean g_initialized = FALSE;

void Gpt_Init(const Gpt_ConfigType* ConfigPtr) {
    memset(g_ch, 0, sizeof(g_ch));
    if (ConfigPtr != NULL && ConfigPtr->channels != NULL) {
        uint8 i;
        for (i = 0; i < ConfigPtr->numChannels && i < GPT_MAX_CHANNELS; i++) {
            const Gpt_ChannelConfigType* cc = &ConfigPtr->channels[i];
            if (cc->channel < GPT_MAX_CHANNELS) {
                g_ch[cc->channel].mode   = cc->mode;
                g_ch[cc->channel].notify = cc->notification;
            }
        }
    }
    g_initialized = TRUE;
}

void Gpt_DeInit(void) {
    memset(g_ch, 0, sizeof(g_ch));
    g_initialized = FALSE;
}

void Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value) {
    if (!g_initialized || Channel >= GPT_MAX_CHANNELS) { return; }
    g_ch[Channel].target  = Value;
    g_ch[Channel].elapsed = 0;
    g_ch[Channel].running = TRUE;
}

void Gpt_StopTimer(Gpt_ChannelType Channel) {
    if (!g_initialized || Channel >= GPT_MAX_CHANNELS) { return; }
    g_ch[Channel].running = FALSE;
}

Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType Channel) {
    if (!g_initialized || Channel >= GPT_MAX_CHANNELS) { return 0; }
    return g_ch[Channel].elapsed;
}

Gpt_ValueType Gpt_GetTimeRemaining(Gpt_ChannelType Channel) {
    if (!g_initialized || Channel >= GPT_MAX_CHANNELS) { return 0; }
    Gpt_ChannelState* ch = &g_ch[Channel];
    if (!ch->running || ch->elapsed >= ch->target) { return 0; }
    return ch->target - ch->elapsed;
}

void Gpt_SetMode(Gpt_ModeType Mode) {
    (void)Mode;
}

void Gpt_Tick(void) {
    uint8 i;
    if (!g_initialized) { return; }
    for (i = 0; i < GPT_MAX_CHANNELS; i++) {
        Gpt_ChannelState* ch = &g_ch[i];
        if (!ch->running) { continue; }
        ch->elapsed++;
        if (ch->elapsed >= ch->target) {
            if (ch->notify != NULL) { ch->notify(); }
            if (ch->mode == GPT_CH_MODE_CONTINUOUS) {
                ch->elapsed = 0;
            } else {
                ch->running = FALSE;
            }
        }
    }
}
