/* FiM.h — Function Inhibition Manager (ADR-005 / P6).
 *
 * Stub: queries Dem for DTC status and returns permission/inhibition.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef FIM_H
#define FIM_H

#include "Std_Types.h"

void FiM_Init(void);
void FiM_DeInit(void);

/** Check if function functionId is permitted.
 *  Returns TRUE if permitted, FALSE if inhibited. */
boolean FiM_GetFunctionPermission(uint16 functionId);

#endif /* FIM_H */
