/* EcuM.c — ECU State Manager implementation (ADR-005 / P3).
 *
 * STARTUP -> RUN -> SHUTDOWN state machine.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "EcuM.h"
#include "SchM.h"
#include "Os.h"
#include "Det.h"

static EcuM_StateType g_state = ECUM_STATE_STARTUP;

void EcuM_Init(void) {
    g_state = ECUM_STATE_STARTUP;

    /* Initialise sub-modules in dependency order. */
    Det_Init();
    Os_Init();
    SchM_Init();

    /* Transition to RUN. */
    g_state = ECUM_STATE_RUN;
}

void EcuM_MainFunction(void) {
    if (g_state != ECUM_STATE_RUN) {
        return;
    }
    /* Drive the scheduler which calls all BSW MainFunctions. */
    SchM_MainFunction();
}

void EcuM_GoSleep(void) {
    g_state = ECUM_STATE_SHUTDOWN;

    /* De-init in reverse order. */
    SchM_DeInit();
    Os_Shutdown();
    /* Det stays alive until the very end for error reporting. */
}

EcuM_StateType EcuM_GetState(void) {
    return g_state;
}
