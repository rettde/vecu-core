/* Appl_Entry.c — Sample ECU application entry points.
 *
 * Implements the three mandatory application exports:
 *   Appl_Init()          — initialise all SWCs
 *   Appl_MainFunction()  — call SWC runnables each tick
 *   Appl_Shutdown()      — de-init SWCs
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu_base_context.h"

/* SWC forward declarations */
extern void SwcSensor_Init(void);
extern void SwcSensor_MainFunction(void);
extern void SwcActuator_Init(void);
extern void SwcActuator_MainFunction(void);
extern void SwcDiag_Init(void);
extern void SwcDiag_MainFunction(void);

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Appl_Init(void) {
    SwcSensor_Init();
    SwcActuator_Init();
    SwcDiag_Init();
}

EXPORT void Appl_MainFunction(void) {
    SwcSensor_MainFunction();
    SwcActuator_MainFunction();
    SwcDiag_MainFunction();
}

EXPORT void Appl_Shutdown(void) {
    /* Nothing to clean up in this sample. */
}
