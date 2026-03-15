/* WdgM.c — Watchdog Manager implementation (ADR-005 / P6).
 *
 * Stub: accepts checkpoint indications but does not enforce timing.
 * A real implementation would track alive counters and report to Det.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "WdgM.h"

static boolean g_initialized = FALSE;

void WdgM_Init(void) {
    g_initialized = TRUE;
}

void WdgM_DeInit(void) {
    g_initialized = FALSE;
}

void WdgM_MainFunction(void) {
    /* Supervision check would go here. */
    (void)0;
}

void WdgM_CheckpointReached(uint16 entityId, uint16 checkpointId) {
    (void)entityId;
    (void)checkpointId;
    /* Stub: accept and ignore. */
}
