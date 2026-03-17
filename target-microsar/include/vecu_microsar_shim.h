/* vecu_microsar_shim.h — Vector MICROSAR integration shim for vecu-core.
 *
 * Bridges Vector MICROSAR BSW (BswM, EcuM, SchM, Com, Dcm, NvM, …)
 * to vecu-core's vecu_base_context_t runtime interface.
 *
 * The MICROSAR BSW sources are NOT part of this repository.
 * They must be provided via MICROSAR_ROOT at build time.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VECU_MICROSAR_SHIM_H
#define VECU_MICROSAR_SHIM_H

#include "vecu_base_context.h"

#ifdef VECU_MICROSAR

void Base_Init(const vecu_base_context_t* ctx);
void Base_Step(uint64_t tick);
void Base_Shutdown(void);

#endif /* VECU_MICROSAR */

#endif /* VECU_MICROSAR_SHIM_H */
