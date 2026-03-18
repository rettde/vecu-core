/* openbsw_stubs.c -- Placeholder stubs for OpenBSW lifecycle functions.
 *
 * When OPENBSW_ROOT is NOT set in CMake, these stubs are compiled
 * instead of the real OpenBSW sources.  They allow the PoC to build
 * and link as a self-contained example for CI and testing.
 *
 * Replace this file with real OpenBSW sources by setting:
 *   cmake -DOPENBSW_ROOT=/path/to/openbsw ...
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Std_Types.h"
#include "openbsw_shim.h"

#include <stddef.h>

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

static boolean g_ecum_initialized = FALSE;

void OpenBsw_EcuM_Init(void) {
    g_ecum_initialized = TRUE;
}

void OpenBsw_EcuM_MainFunction(void) {
    if (!g_ecum_initialized) { return; }
}

void OpenBsw_SchM_MainFunction(void) {
    if (!g_ecum_initialized) { return; }
}

void OpenBsw_EcuM_GoSleep(void) {
    if (!g_ecum_initialized) { return; }
}

void OpenBsw_EcuM_GoOff(void) {
    g_ecum_initialized = FALSE;
}

EXPORT void Appl_Init(void) {
}

EXPORT void Appl_MainFunction(void) {
}

EXPORT void Appl_Shutdown(void) {
}
