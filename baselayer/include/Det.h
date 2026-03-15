/* Det.h — Default Error Tracer (ADR-005 / P3).
 *
 * Det_ReportError forwards to ctx.log_fn.
 * Counts errors per module for diagnostic purposes.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef DET_H
#define DET_H

#include "Std_Types.h"

/* Initialise Det. */
void Det_Init(void);

/* Report a development error. */
Std_ReturnType Det_ReportError(uint16_t ModuleId,
                               uint8_t  InstanceId,
                               uint8_t  ApiId,
                               uint8_t  ErrorId);

/* Get total error count (for testing). */
uint32_t Det_GetErrorCount(void);

#endif /* DET_H */
