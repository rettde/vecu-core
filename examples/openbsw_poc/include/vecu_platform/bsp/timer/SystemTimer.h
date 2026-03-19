/* bsp/timer/SystemTimer.h — vecu single-threaded stub.
 *
 * Provides tick-based system timer for deterministic vECU execution.
 * All time functions return 0 or tick-derived values.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

static uint64_t g_vecu_system_ticks = 0;

static inline void sysDelayUs(uint32_t delay) { (void)delay; }

static inline uint64_t getSystemTicks(void) { return g_vecu_system_ticks; }

static inline uint32_t getSystemTicks32Bit(void) { return (uint32_t)g_vecu_system_ticks; }

static inline uint32_t getSystemTimeUs32Bit(void) { return (uint32_t)(g_vecu_system_ticks * 1000U); }

static inline uint32_t getSystemTimeMs32Bit(void) { return (uint32_t)g_vecu_system_ticks; }

static inline uint64_t getSystemTimeNs(void) { return g_vecu_system_ticks * 1000000ULL; }

static inline uint64_t getSystemTimeUs(void) { return g_vecu_system_ticks * 1000ULL; }

static inline uint64_t getSystemTimeMs(void) { return g_vecu_system_ticks; }

static inline uint64_t systemTicksToTimeUs(uint64_t ticks) { return ticks * 1000ULL; }

static inline uint64_t systemTicksToTimeNs(uint64_t ticks) { return ticks * 1000000ULL; }

static inline uint32_t getFastTicks(void) { return (uint32_t)g_vecu_system_ticks; }

static inline uint32_t getFastTicksPerSecond(void) { return 1000U; }

static inline void initSystemTimer(void) { g_vecu_system_ticks = 0; }

#ifdef __cplusplus
}
#endif
