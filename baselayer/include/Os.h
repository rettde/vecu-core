/* Os.h — Operating System stub (ADR-005 / P3).
 *
 * Single-threaded stub: Os_GetCounterValue returns the current tick,
 * task activation is a no-op.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef OS_H
#define OS_H

#include "Std_Types.h"

typedef uint64_t TickType;

/* Initialise OS stub. */
void Os_Init(void);

/* Get current counter value (tick). */
TickType Os_GetCounterValue(void);

/* Set current tick (called by Base_Step). */
void Os_SetTick(uint64_t tick);

/* Shutdown OS stub. */
void Os_Shutdown(void);

#endif /* OS_H */
