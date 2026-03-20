/* Icu.c — Virtual ICU (Input Capture Unit) Driver (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed edge detection, timestamps, edge counting and signal measurement.
 * Edges are injected via Icu_SimulateEdge() from test harness or SIL Kit.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Icu.h"
#include <string.h>

typedef struct {
    Icu_ActivationType activation;
    Icu_LevelType      level;
    Icu_InputStateType state;
    boolean            notification_enabled;
    boolean            edge_detect_enabled;
    boolean            edge_count_enabled;
    Icu_EdgeNumberType edge_count;
    boolean            timestamp_running;
    Icu_ValueType*     ts_buffer;
    uint16             ts_buffer_size;
    uint16             ts_notify_interval;
    Icu_IndexType      ts_index;
    boolean            signal_meas_running;
    Icu_ValueType      elapsed_time;
    Icu_DutyCycleType  duty_cycle;
} Icu_ChannelState;

static Icu_ChannelState g_channels[ICU_MAX_CHANNELS];
static Icu_ModeType     g_mode = ICU_MODE_NORMAL;
static boolean          g_initialized = FALSE;
static uint32           g_tick = 0u;

void Icu_Init(const Icu_ConfigType* ConfigPtr) {
    (void)ConfigPtr;
    memset(g_channels, 0, sizeof(g_channels));
    g_mode = ICU_MODE_NORMAL;
    g_tick = 0u;
    g_initialized = TRUE;
}

void Icu_DeInit(void) {
    g_initialized = FALSE;
}

void Icu_SetMode(Icu_ModeType Mode) {
    if (!g_initialized) { return; }
    g_mode = Mode;
}

void Icu_SetActivationCondition(Icu_ChannelType Channel,
                                Icu_ActivationType Activation) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].activation = Activation;
}

void Icu_DisableNotification(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].notification_enabled = FALSE;
}

void Icu_EnableNotification(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].notification_enabled = TRUE;
}

Icu_InputStateType Icu_GetInputState(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return ICU_IDLE; }
    Icu_InputStateType s = g_channels[Channel].state;
    g_channels[Channel].state = ICU_IDLE;
    return s;
}

Icu_LevelType Icu_GetInputLevel(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return ICU_LOW; }
    return g_channels[Channel].level;
}

void Icu_StartTimestamp(Icu_ChannelType Channel, Icu_ValueType* BufferPtr,
                        uint16 BufferSize, uint16 NotifyInterval) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    if (BufferPtr == NULL || BufferSize == 0u) { return; }
    g_channels[Channel].ts_buffer = BufferPtr;
    g_channels[Channel].ts_buffer_size = BufferSize;
    g_channels[Channel].ts_notify_interval = NotifyInterval;
    g_channels[Channel].ts_index = 0u;
    g_channels[Channel].timestamp_running = TRUE;
}

void Icu_StopTimestamp(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].timestamp_running = FALSE;
}

Icu_IndexType Icu_GetTimestampIndex(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return 0u; }
    return g_channels[Channel].ts_index;
}

void Icu_ResetEdgeCount(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].edge_count = 0u;
}

void Icu_EnableEdgeCount(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].edge_count_enabled = TRUE;
}

void Icu_DisableEdgeCount(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].edge_count_enabled = FALSE;
}

Icu_EdgeNumberType Icu_GetEdgeNumbers(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return 0u; }
    return g_channels[Channel].edge_count;
}

void Icu_EnableEdgeDetection(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].edge_detect_enabled = TRUE;
}

void Icu_DisableEdgeDetection(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].edge_detect_enabled = FALSE;
}

void Icu_StartSignalMeasurement(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].signal_meas_running = TRUE;
    g_channels[Channel].elapsed_time = 0u;
    g_channels[Channel].duty_cycle.ActiveTime = 0u;
    g_channels[Channel].duty_cycle.PeriodTime = 0u;
}

void Icu_StopSignalMeasurement(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].signal_meas_running = FALSE;
}

Icu_ValueType Icu_GetTimeElapsed(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return 0u; }
    return g_channels[Channel].elapsed_time;
}

void Icu_GetDutyCycleValues(Icu_ChannelType Channel,
                            Icu_DutyCycleType* DutyCycleValues) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    if (DutyCycleValues == NULL) { return; }
    *DutyCycleValues = g_channels[Channel].duty_cycle;
}

void Icu_SetInputLevel(Icu_ChannelType Channel, Icu_LevelType Level) {
    if (Channel >= ICU_MAX_CHANNELS) { return; }
    g_channels[Channel].level = Level;
}

void Icu_SimulateEdge(Icu_ChannelType Channel) {
    if (!g_initialized || Channel >= ICU_MAX_CHANNELS) { return; }
    Icu_ChannelState* ch = &g_channels[Channel];

    ch->state = ICU_ACTIVE;

    if (ch->edge_count_enabled) {
        ch->edge_count++;
    }

    if (ch->timestamp_running && ch->ts_buffer != NULL) {
        if (ch->ts_index < ch->ts_buffer_size) {
            ch->ts_buffer[ch->ts_index] = g_tick;
            ch->ts_index++;
        } else if (ch->ts_buffer_size > 0u) {
            ch->ts_index = 0u;
            ch->ts_buffer[ch->ts_index] = g_tick;
            ch->ts_index++;
        }
    }

    if (ch->signal_meas_running) {
        ch->elapsed_time = g_tick;
    }

    g_tick++;
}
