/* Icu.h — Virtual ICU (Input Capture Unit) Driver API (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed edge/timestamp/signal measurement. No real hardware.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_ICU_H
#define VMCAL_ICU_H

#include "Std_Types.h"

#define ICU_MAX_CHANNELS 32u

typedef uint8  Icu_ChannelType;
typedef uint32 Icu_ValueType;
typedef uint16 Icu_IndexType;
typedef uint32 Icu_EdgeNumberType;

typedef enum {
    ICU_MODE_NORMAL = 0u,
    ICU_MODE_SLEEP  = 1u
} Icu_ModeType;

typedef enum {
    ICU_ACTIVE = 0u,
    ICU_IDLE   = 1u
} Icu_InputStateType;

typedef enum {
    ICU_LOW  = 0u,
    ICU_HIGH = 1u
} Icu_LevelType;

typedef enum {
    ICU_RISING_EDGE  = 0u,
    ICU_FALLING_EDGE = 1u,
    ICU_BOTH_EDGES   = 2u
} Icu_ActivationType;

typedef enum {
    ICU_MODE_SIGNAL_EDGE_DETECT = 0u,
    ICU_MODE_SIGNAL_MEASUREMENT = 1u,
    ICU_MODE_TIMESTAMP          = 2u,
    ICU_MODE_EDGE_COUNTER       = 3u
} Icu_MeasurementModeType;

typedef enum {
    ICU_LOW_TIME    = 0u,
    ICU_HIGH_TIME   = 1u,
    ICU_PERIOD_TIME = 2u,
    ICU_DUTY_CYCLE  = 3u
} Icu_SignalMeasurementPropertyType;

typedef enum {
    ICU_LINEAR_BUFFER   = 0u,
    ICU_CIRCULAR_BUFFER = 1u
} Icu_TimestampBufferType;

typedef struct {
    Icu_ValueType ActiveTime;
    Icu_ValueType PeriodTime;
} Icu_DutyCycleType;

typedef struct {
    uint8 dummy;
} Icu_ConfigType;

void               Icu_Init(const Icu_ConfigType* ConfigPtr);
void               Icu_DeInit(void);
void               Icu_SetMode(Icu_ModeType Mode);
void               Icu_SetActivationCondition(Icu_ChannelType Channel,
                                              Icu_ActivationType Activation);
void               Icu_DisableNotification(Icu_ChannelType Channel);
void               Icu_EnableNotification(Icu_ChannelType Channel);
Icu_InputStateType Icu_GetInputState(Icu_ChannelType Channel);
Icu_LevelType      Icu_GetInputLevel(Icu_ChannelType Channel);
void               Icu_StartTimestamp(Icu_ChannelType Channel,
                                      Icu_ValueType* BufferPtr,
                                      uint16 BufferSize,
                                      uint16 NotifyInterval);
void               Icu_StopTimestamp(Icu_ChannelType Channel);
Icu_IndexType      Icu_GetTimestampIndex(Icu_ChannelType Channel);
void               Icu_ResetEdgeCount(Icu_ChannelType Channel);
void               Icu_EnableEdgeCount(Icu_ChannelType Channel);
void               Icu_DisableEdgeCount(Icu_ChannelType Channel);
Icu_EdgeNumberType Icu_GetEdgeNumbers(Icu_ChannelType Channel);
void               Icu_EnableEdgeDetection(Icu_ChannelType Channel);
void               Icu_DisableEdgeDetection(Icu_ChannelType Channel);
void               Icu_StartSignalMeasurement(Icu_ChannelType Channel);
void               Icu_StopSignalMeasurement(Icu_ChannelType Channel);
Icu_ValueType      Icu_GetTimeElapsed(Icu_ChannelType Channel);
void               Icu_GetDutyCycleValues(Icu_ChannelType Channel,
                                          Icu_DutyCycleType* DutyCycleValues);
void               Icu_SetInputLevel(Icu_ChannelType Channel, Icu_LevelType Level);
void               Icu_SimulateEdge(Icu_ChannelType Channel);

#endif /* VMCAL_ICU_H */
