/* vecu_openbsw_shim.h — OpenBSW integration shim for vecu-core.
 *
 * Bridges Eclipse OpenBSW lifecycle, communication, and diagnostics
 * to vecu-core's vecu_base_context_t runtime interface.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VECU_OPENBSW_SHIM_H
#define VECU_OPENBSW_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vecu_base_context.h"

void Base_Init(const vecu_base_context_t* ctx);
void Base_Step(uint64_t tick);
void Base_Shutdown(void);

const vecu_base_context_t* OpenBsw_GetCtx(void);

#ifdef __cplusplus
}
#endif

#endif /* VECU_OPENBSW_SHIM_H */
