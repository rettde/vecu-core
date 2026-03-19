/* suspendResumeAllInterrupts.h — vecu single-threaded stub.
 *
 * In a single-threaded vECU there are no interrupts to suspend.
 * All operations are no-ops.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#pragma once

#include <platform/estdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint32_t OldIntEnabledStatusValueType;

static inline uint32_t getMachineStateRegisterValueAndSuspendAllInterrupts(void) { return 0U; }

static inline OldIntEnabledStatusValueType getOldIntEnabledStatusValueAndSuspendAllInterrupts(void)
{
    return 0U;
}

static inline void resumeAllInterrupts(uint32_t oldMachineStateRegisterValue)
{
    (void)oldMachineStateRegisterValue;
}

#ifdef __cplusplus
}
#endif
