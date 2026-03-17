/* Os_Task.h — OS-Semantics Mapping: Task & Alarm types (ADR-004).
 *
 * Defines task, alarm, counter, and event structures for deterministic
 * tick-based scheduling in the Level-3 vECU.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef OS_TASK_H
#define OS_TASK_H

#include "Std_Types.h"

typedef uint16 Os_TaskIdType;
typedef uint16 Os_AlarmIdType;
typedef uint16 Os_CounterIdType;
typedef uint32 Os_EventMaskType;
typedef uint32 Os_TickType;

typedef void (*Os_TaskFuncType)(void);
typedef void (*Os_AlarmCallbackType)(void);

#define OS_MAX_TASKS    64u
#define OS_MAX_ALARMS   32u
#define OS_MAX_COUNTERS  8u
#define OS_MAX_EVENTS   64u

typedef struct {
    Os_TaskIdType   taskId;
    Os_TaskFuncType function;
    Os_TickType     cycleTicks;
    Os_TickType     offsetTicks;
    uint8           priority;
    boolean         autostart;
} Os_TaskConfigType;

typedef enum {
    OS_ALARM_MODE_ONESHOT    = 0u,
    OS_ALARM_MODE_CYCLIC     = 1u,
    OS_ALARM_MODE_CALLBACK   = 2u
} Os_AlarmModeType;

typedef struct {
    Os_AlarmIdType       alarmId;
    Os_CounterIdType     counterId;
    Os_AlarmModeType     mode;
    Os_TaskIdType        taskId;
    Os_AlarmCallbackType callback;
} Os_AlarmConfigType;

typedef struct {
    Os_CounterIdType counterId;
    Os_TickType      maxAllowedValue;
    Os_TickType      ticksPerBase;
} Os_CounterConfigType;

typedef struct {
    const Os_TaskConfigType*    tasks;
    uint16                      numTasks;
    const Os_AlarmConfigType*   alarms;
    uint16                      numAlarms;
    const Os_CounterConfigType* counters;
    uint16                      numCounters;
} Os_ConfigType;

#endif /* OS_TASK_H */
