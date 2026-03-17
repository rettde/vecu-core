/* Os_Mapping.c — OS-Semantics Mapping (ADR-004).
 *
 * Deterministic tick-based dispatch of AUTOSAR OS tasks, alarms,
 * counters, and events. Single-threaded, fully serialized execution.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Os_Mapping.h"
#include <string.h>
#include <stddef.h>

typedef struct {
    Os_TaskFuncType function;
    Os_TickType     cycleTicks;
    Os_TickType     offsetTicks;
    uint8           priority;
    boolean         active;
    Os_EventMaskType events;
} Os_TaskState;

typedef struct {
    Os_CounterIdType     counterId;
    Os_AlarmModeType     mode;
    Os_TaskIdType        taskId;
    Os_AlarmCallbackType callback;
    Os_TickType          start;
    Os_TickType          cycle;
    boolean              armed;
} Os_AlarmState;

typedef struct {
    Os_TickType value;
    Os_TickType maxAllowedValue;
    Os_TickType ticksPerBase;
} Os_CounterState;

static Os_TaskState    g_tasks[OS_MAX_TASKS];
static Os_AlarmState   g_alarms[OS_MAX_ALARMS];
static Os_CounterState g_counters[OS_MAX_COUNTERS];
static uint16          g_num_tasks    = 0;
static uint16          g_num_alarms   = 0;
static uint16          g_num_counters = 0;
static Os_PhaseType    g_phase        = OS_PHASE_UNINIT;

void Os_Init(const Os_ConfigType* config) {
    uint16 i;
    memset(g_tasks, 0, sizeof(g_tasks));
    memset(g_alarms, 0, sizeof(g_alarms));
    memset(g_counters, 0, sizeof(g_counters));
    g_num_tasks    = 0;
    g_num_alarms   = 0;
    g_num_counters = 0;

    if (config == NULL) {
        g_phase = OS_PHASE_STARTUP;
        return;
    }

    if (config->tasks != NULL) {
        g_num_tasks = (config->numTasks < OS_MAX_TASKS) ? config->numTasks : OS_MAX_TASKS;
        for (i = 0; i < g_num_tasks; i++) {
            const Os_TaskConfigType* tc = &config->tasks[i];
            Os_TaskState* ts = &g_tasks[i];
            ts->function    = tc->function;
            ts->cycleTicks  = tc->cycleTicks;
            ts->offsetTicks = tc->offsetTicks;
            ts->priority    = tc->priority;
            ts->active      = tc->autostart;
            ts->events      = 0;
        }
    }

    if (config->alarms != NULL) {
        g_num_alarms = (config->numAlarms < OS_MAX_ALARMS) ? config->numAlarms : OS_MAX_ALARMS;
        for (i = 0; i < g_num_alarms; i++) {
            const Os_AlarmConfigType* ac = &config->alarms[i];
            Os_AlarmState* as = &g_alarms[i];
            as->counterId = ac->counterId;
            as->mode      = ac->mode;
            as->taskId    = ac->taskId;
            as->callback  = ac->callback;
            as->armed     = FALSE;
        }
    }

    if (config->counters != NULL) {
        g_num_counters = (config->numCounters < OS_MAX_COUNTERS) ? config->numCounters : OS_MAX_COUNTERS;
        for (i = 0; i < g_num_counters; i++) {
            const Os_CounterConfigType* cc = &config->counters[i];
            Os_CounterState* cs = &g_counters[i];
            cs->value           = 0;
            cs->maxAllowedValue = cc->maxAllowedValue;
            cs->ticksPerBase    = cc->ticksPerBase;
        }
    }

    g_phase = OS_PHASE_STARTUP;
}

void Os_Shutdown(void) {
    g_phase = OS_PHASE_SHUTDOWN;
    memset(g_tasks, 0, sizeof(g_tasks));
    memset(g_alarms, 0, sizeof(g_alarms));
    g_num_tasks  = 0;
    g_num_alarms = 0;
    g_phase = OS_PHASE_UNINIT;
}

static void advance_counters(void) {
    uint16 i;
    for (i = 0; i < g_num_counters; i++) {
        Os_CounterState* cs = &g_counters[i];
        cs->value++;
        if (cs->value > cs->maxAllowedValue) {
            cs->value = 0;
        }
    }
}

static void process_alarms(void) {
    uint16 i;
    for (i = 0; i < g_num_alarms; i++) {
        Os_AlarmState* as = &g_alarms[i];
        if (!as->armed) { continue; }

        if (as->start > 0) {
            as->start--;
            continue;
        }

        switch (as->mode) {
        case OS_ALARM_MODE_ONESHOT:
            if (as->taskId < g_num_tasks) {
                g_tasks[as->taskId].active = TRUE;
            }
            as->armed = FALSE;
            break;
        case OS_ALARM_MODE_CYCLIC:
            if (as->taskId < g_num_tasks) {
                g_tasks[as->taskId].active = TRUE;
            }
            as->start = as->cycle;
            break;
        case OS_ALARM_MODE_CALLBACK:
            if (as->callback != NULL) { as->callback(); }
            if (as->cycle > 0) {
                as->start = as->cycle;
            } else {
                as->armed = FALSE;
            }
            break;
        }
    }
}

static boolean task_is_due(const Os_TaskState* ts, Os_TickType tick) {
    if (!ts->active || ts->function == NULL) { return FALSE; }
    if (ts->cycleTicks == 0) { return TRUE; }
    return (tick >= ts->offsetTicks &&
            ((tick - ts->offsetTicks) % ts->cycleTicks) == 0);
}

static void dispatch_tasks(Os_TickType tick) {
    uint16 i;
    uint16 ready[OS_MAX_TASKS];
    uint16 ready_count = 0;
    uint16 j;

    for (i = 0; i < g_num_tasks; i++) {
        if (task_is_due(&g_tasks[i], tick)) {
            ready[ready_count++] = i;
        }
    }

    for (i = 1; i < ready_count; i++) {
        uint16 key = ready[i];
        uint8 key_prio = g_tasks[key].priority;
        j = i;
        while (j > 0 && g_tasks[ready[j - 1u]].priority > key_prio) {
            ready[j] = ready[j - 1u];
            j--;
        }
        ready[j] = key;
    }

    for (i = 0; i < ready_count; i++) {
        Os_TaskState* ts = &g_tasks[ready[i]];
        ts->function();
        if (ts->cycleTicks == 0) {
            ts->active = FALSE;
        }
    }
}

void Os_Tick(Os_TickType tick) {
    if (g_phase != OS_PHASE_RUN) { return; }
    advance_counters();
    process_alarms();
    dispatch_tasks(tick);
}

Os_PhaseType Os_GetPhase(void) {
    return g_phase;
}

void Os_SetPhase(Os_PhaseType phase) {
    g_phase = phase;
}

void Os_ActivateTask(Os_TaskIdType taskId) {
    if (taskId < g_num_tasks) {
        g_tasks[taskId].active = TRUE;
    }
}

Std_ReturnType Os_SetRelAlarm(Os_AlarmIdType alarmId,
                              Os_TickType increment, Os_TickType cycle) {
    if (alarmId >= g_num_alarms) { return E_NOT_OK; }
    Os_AlarmState* as = &g_alarms[alarmId];
    as->start = increment;
    as->cycle = cycle;
    as->armed = TRUE;
    return E_OK;
}

Std_ReturnType Os_CancelAlarm(Os_AlarmIdType alarmId) {
    if (alarmId >= g_num_alarms) { return E_NOT_OK; }
    g_alarms[alarmId].armed = FALSE;
    return E_OK;
}

Os_TickType Os_GetCounterValue(Os_CounterIdType counterId) {
    if (counterId >= g_num_counters) { return 0; }
    return g_counters[counterId].value;
}

void Os_SetEvent(Os_TaskIdType taskId, Os_EventMaskType mask) {
    if (taskId < g_num_tasks) {
        g_tasks[taskId].events |= mask;
    }
}

void Os_ClearEvent(Os_EventMaskType mask) {
    uint16 i;
    for (i = 0; i < g_num_tasks; i++) {
        g_tasks[i].events &= ~mask;
    }
}

Os_EventMaskType Os_GetEvent(Os_TaskIdType taskId) {
    if (taskId >= g_num_tasks) { return 0; }
    return g_tasks[taskId].events;
}

Std_ReturnType Os_WaitEvent(Os_EventMaskType mask) {
    (void)mask;
    return E_OK;
}
