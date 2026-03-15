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
#include "CanIf.h"
#include "EthIf.h"
#include "LinIf.h"
#include "FrIf.h"
#include "PduR.h"
#include "Com.h"
#include "Cry.h"
#include "CryIf.h"
#include "Csm.h"
#include "Fee.h"
#include "MemIf.h"
#include "NvM.h"
#include "Dem.h"
#include "Dcm.h"
#include "FiM.h"
#include "WdgM.h"

static EcuM_StateType g_state = ECUM_STATE_STARTUP;

void EcuM_Init(void) {
    g_state = ECUM_STATE_STARTUP;

    /* Initialise sub-modules in dependency order. */
    Det_Init();
    Os_Init();
    CanIf_Init();
    EthIf_Init();
    LinIf_Init();
    FrIf_Init();
    PduR_Init();
    Cry_Init();
    CryIf_Init();
    Csm_Init();
    Fee_Init();
    MemIf_Init();
    /* NvM_Init, Dem_Init, Dcm_Init called externally with config — see Base_Init. */
    FiM_Init();
    WdgM_Init();
    /* Com_Init is called externally with config — see Base_Init. */
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
    WdgM_DeInit();
    FiM_DeInit();
    Dcm_DeInit();
    Dem_DeInit();
    NvM_DeInit();
    MemIf_DeInit();
    Fee_DeInit();
    Csm_DeInit();
    CryIf_DeInit();
    Cry_DeInit();
    Com_DeInit();
    PduR_DeInit();
    FrIf_DeInit();
    LinIf_DeInit();
    EthIf_DeInit();
    CanIf_DeInit();
    Os_Shutdown();
    /* Det stays alive until the very end for error reporting. */
}

EcuM_StateType EcuM_GetState(void) {
    return g_state;
}
