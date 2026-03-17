/* VMcal_Context.h — Virtual-MCAL runtime context (ADR-002).
 *
 * Stores the vecu_base_context_t pointer injected by the runtime.
 * All Virtual-MCAL modules access the runtime through this interface.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_CONTEXT_H
#define VMCAL_CONTEXT_H

#include "vecu_base_context.h"

void VMcal_Init(const vecu_base_context_t* ctx);
void VMcal_Shutdown(void);
const vecu_base_context_t* VMcal_GetCtx(void);

#endif /* VMCAL_CONTEXT_H */
