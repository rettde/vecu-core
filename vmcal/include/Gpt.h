/* Gpt.h — Virtual GPT Timer Driver API (ADR-002 / Virtual-MCAL).
 *
 * Sim-time-based drop-in replacement for Gpt_* MCAL driver.
 * Timer values are derived from vecu-core tick count.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_GPT_H
#define VMCAL_GPT_H

#include "Std_Types.h"

typedef uint8  Gpt_ChannelType;
typedef uint32 Gpt_ValueType;

typedef void (*Gpt_NotificationType)(void);

#define GPT_MAX_CHANNELS 16u

typedef enum {
    GPT_MODE_NORMAL    = 0u,
    GPT_MODE_SLEEP     = 1u
} Gpt_ModeType;

typedef enum {
    GPT_CH_MODE_ONESHOT    = 0u,
    GPT_CH_MODE_CONTINUOUS = 1u
} Gpt_ChannelModeType;

typedef struct {
    Gpt_ChannelType       channel;
    Gpt_ChannelModeType   mode;
    Gpt_NotificationType  notification;
} Gpt_ChannelConfigType;

typedef struct {
    const Gpt_ChannelConfigType* channels;
    uint8                        numChannels;
} Gpt_ConfigType;

void           Gpt_Init(const Gpt_ConfigType* ConfigPtr);
void           Gpt_DeInit(void);
void           Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value);
void           Gpt_StopTimer(Gpt_ChannelType Channel);
Gpt_ValueType  Gpt_GetTimeElapsed(Gpt_ChannelType Channel);
Gpt_ValueType  Gpt_GetTimeRemaining(Gpt_ChannelType Channel);
void           Gpt_SetMode(Gpt_ModeType Mode);
void           Gpt_Tick(void);

#endif /* VMCAL_GPT_H */
