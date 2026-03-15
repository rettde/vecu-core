/* Os.c — Operating System stub (ADR-005 / P3).
 *
 * Single-threaded stub.  Tick is set externally by Base_Step.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Os.h"

static TickType g_current_tick = 0;

void Os_Init(void) {
    g_current_tick = 0;
}

TickType Os_GetCounterValue(void) {
    return g_current_tick;
}

void Os_SetTick(uint64_t tick) {
    g_current_tick = tick;
}

void Os_Shutdown(void) {
    g_current_tick = 0;
}
