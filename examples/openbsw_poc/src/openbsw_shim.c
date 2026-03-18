/* openbsw_shim.c -- Lifecycle bridge: vecu-appl -> OpenBSW EcuM.
 *
 * Provides the three mandatory Base_* entry points that vecu-appl
 * calls (see vecu_base_context.h).  Internally delegates to the
 * OpenBSW EcuM / SchM lifecycle functions.
 *
 * Also stores the vecu_base_context_t pointer so that Virtual-MCAL
 * modules (linked into the same shared library) can access frame I/O
 * and crypto callbacks via VMcal_GetCtx().
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Std_Types.h"
#include "openbsw_shim.h"
#include "VMcal_Context.h"

#include <stddef.h>

#ifdef VECU_BUILD
#include "Os_Mapping.h"
#endif

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Base_Init(const vecu_base_context_t* ctx) {
    if (ctx == NULL) { return; }

    VMcal_Init(ctx);

#ifdef VECU_BUILD
    Os_Init(NULL);
#endif

    OpenBsw_EcuM_Init();
}

EXPORT void Base_Step(uint64_t tick) {
#ifdef VECU_BUILD
    Os_Tick((Os_TickType)tick);
#else
    (void)tick;
#endif

    OpenBsw_EcuM_MainFunction();
    OpenBsw_SchM_MainFunction();
}

EXPORT void Base_Shutdown(void) {
    OpenBsw_EcuM_GoSleep();
    OpenBsw_EcuM_GoOff();

#ifdef VECU_BUILD
    Os_Shutdown();
#endif
}
