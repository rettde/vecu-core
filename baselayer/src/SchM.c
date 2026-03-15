/* SchM.c — Schedule Manager implementation (ADR-005 / P3).
 *
 * Deterministic single-threaded scheduler.  Iterates BSW MainFunction
 * list in fixed order.  Additional modules can be registered here.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "SchM.h"
#include "CanIf.h"
#include "Com.h"
#include "Csm.h"
#include "Dcm.h"
#include "Dem.h"
#include "NvM.h"
#include "WdgM.h"
#include <stddef.h>

/* ── Internal state ─────────────────────────────────────────────── */

static boolean g_initialized = FALSE;

/* BSW MainFunction table — add new modules here.
 * Currently empty; EcuM_MainFunction drives SchM_MainFunction
 * which in turn would drive registered modules.
 * In P4+ this will include Com_MainFunction, PduR_MainFunction, etc. */

typedef void (*SchM_MainFunctionPtr)(void);

/* BSW MainFunction table — executed in order each tick.
 * A NULL sentinel terminates the list. */
static const SchM_MainFunctionPtr g_main_functions[] = {
    CanIf_RxMainFunction,  /* Process inbound CAN frames first */
    Com_MainFunction,      /* Then handle signal TX/RX */
    Csm_MainFunction,      /* Crypto job processing */
    Dcm_MainFunction,      /* Diagnostic session timeout */
    Dem_MainFunction,      /* DTC aging / debouncing */
    NvM_MainFunction,      /* NvM async job processing */
    WdgM_MainFunction,     /* Alive supervision */
    NULL  /* sentinel */
};

/* ── API ────────────────────────────────────────────────────────── */

void SchM_Init(void) {
    g_initialized = TRUE;
}

void SchM_MainFunction(void) {
    if (!g_initialized) {
        return;
    }
    for (unsigned i = 0; g_main_functions[i] != NULL; i++) {
        g_main_functions[i]();
    }
}

void SchM_DeInit(void) {
    g_initialized = FALSE;
}
