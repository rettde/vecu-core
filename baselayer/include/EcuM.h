/* EcuM.h — ECU State Manager (minimal stub, ADR-005 / P3).
 *
 * Implements STARTUP -> RUN -> SHUTDOWN state machine.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef ECUM_H
#define ECUM_H

#include "Std_Types.h"

/* EcuM states. */
typedef enum {
    ECUM_STATE_STARTUP  = 0,
    ECUM_STATE_RUN      = 1,
    ECUM_STATE_SHUTDOWN = 2
} EcuM_StateType;

/* Initialise EcuM — transitions to STARTUP, then to RUN. */
void EcuM_Init(void);

/* Main function — called each tick in RUN state. */
void EcuM_MainFunction(void);

/* Trigger shutdown sequence. */
void EcuM_GoSleep(void);

/* Query current state. */
EcuM_StateType EcuM_GetState(void);

#endif /* ECUM_H */
