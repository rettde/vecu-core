/* SchM.h — Schedule Manager (minimal stub, ADR-005 / P3).
 *
 * Deterministic scheduler: iterates BSW MainFunction list in fixed order.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef SCHM_H
#define SCHM_H

#include "Std_Types.h"

/* Initialise the scheduler. */
void SchM_Init(void);

/* Execute all registered BSW MainFunctions in order. */
void SchM_MainFunction(void);

/* De-initialise the scheduler. */
void SchM_DeInit(void);

#endif /* SCHM_H */
