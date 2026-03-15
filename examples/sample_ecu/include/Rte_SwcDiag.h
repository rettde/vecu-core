/* Rte_SwcDiag.h — RTE interface for the Diagnostic SWC.
 *
 * Provides access to Dcm and Dem for diagnostic handling.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef RTE_SWCDIAG_H
#define RTE_SWCDIAG_H

#include "Std_Types.h"
#include "Dcm.h"
#include "Dem.h"

/* Diagnostic SWC can report DTCs and check session state. */

static inline uint8 Rte_GetDcmSession(void) {
    return Dcm_GetActiveSession();
}

static inline Std_ReturnType Rte_ReportDtc(uint32 dtcNumber, uint8 status) {
    return Dem_ReportErrorStatus(dtcNumber, status);
}

#endif /* RTE_SWCDIAG_H */
