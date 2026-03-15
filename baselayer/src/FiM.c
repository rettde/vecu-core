/* FiM.c — Function Inhibition Manager implementation (ADR-005 / P6).
 *
 * Stub: always permits all functions.  A real implementation would
 * query Dem for DTC status and inhibit functions accordingly.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "FiM.h"

static boolean g_initialized = FALSE;

void FiM_Init(void) {
    g_initialized = TRUE;
}

void FiM_DeInit(void) {
    g_initialized = FALSE;
}

boolean FiM_GetFunctionPermission(uint16 functionId) {
    (void)functionId;
    if (!g_initialized) { return FALSE; }
    return TRUE;  /* always permitted in stub */
}
