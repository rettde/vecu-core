/* WdgM.h — Watchdog Manager (ADR-005 / P6).
 *
 * Stub: alive supervision with checkpoint monitoring.
 * Reports to Det on timeout.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WDGM_H
#define WDGM_H

#include "Std_Types.h"

void WdgM_Init(void);
void WdgM_DeInit(void);
void WdgM_MainFunction(void);

/** Report a checkpoint alive indication. */
void WdgM_CheckpointReached(uint16 entityId, uint16 checkpointId);

#endif /* WDGM_H */
