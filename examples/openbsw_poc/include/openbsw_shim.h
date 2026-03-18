/* openbsw_shim.h -- Lifecycle bridge between vecu-appl and OpenBSW.
 *
 * Maps the vecu-core Base_Init/Step/Shutdown API to the OpenBSW
 * EcuM lifecycle.  This header is included by the shim implementation
 * only; OpenBSW BSW modules do not need to know about this layer.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef OPENBSW_SHIM_H
#define OPENBSW_SHIM_H

#include "vecu_base_context.h"

#ifdef __cplusplus
extern "C" {
#endif

void OpenBsw_EcuM_Init(void);
void OpenBsw_EcuM_MainFunction(void);
void OpenBsw_SchM_MainFunction(void);
void OpenBsw_EcuM_GoSleep(void);
void OpenBsw_EcuM_GoOff(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENBSW_SHIM_H */
