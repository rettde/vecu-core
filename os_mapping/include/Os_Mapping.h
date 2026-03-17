/* Os_Mapping.h — OS-Semantics Mapping API (ADR-004).
 *
 * Maps AUTOSAR OS semantics (tasks, alarms, counters, events) onto
 * the vecu-core deterministic tick engine.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef OS_MAPPING_H
#define OS_MAPPING_H

#include "Os_Task.h"

typedef enum {
    OS_PHASE_UNINIT   = 0u,
    OS_PHASE_STARTUP  = 1u,
    OS_PHASE_RUN      = 2u,
    OS_PHASE_SHUTDOWN = 3u
} Os_PhaseType;

void         Os_Init(const Os_ConfigType* config);
void         Os_Shutdown(void);
void         Os_Tick(Os_TickType tick);
Os_PhaseType Os_GetPhase(void);
void         Os_SetPhase(Os_PhaseType phase);

void            Os_ActivateTask(Os_TaskIdType taskId);
Std_ReturnType  Os_SetRelAlarm(Os_AlarmIdType alarmId,
                               Os_TickType increment, Os_TickType cycle);
Std_ReturnType  Os_CancelAlarm(Os_AlarmIdType alarmId);
Os_TickType     Os_GetCounterValue(Os_CounterIdType counterId);

void            Os_SetEvent(Os_TaskIdType taskId, Os_EventMaskType mask);
void            Os_ClearEvent(Os_EventMaskType mask);
Os_EventMaskType Os_GetEvent(Os_TaskIdType taskId);
Std_ReturnType  Os_WaitEvent(Os_EventMaskType mask);

#endif /* OS_MAPPING_H */
