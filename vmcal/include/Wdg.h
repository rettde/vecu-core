/* Wdg.h — Virtual Watchdog Driver API (ADR-002 / Virtual-MCAL).
 *
 * Software watchdog with configurable timeout. Trigger resets the counter.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_WDG_H
#define VMCAL_WDG_H

#include "Std_Types.h"

typedef enum {
    WDG_MODE_OFF  = 0u,
    WDG_MODE_SLOW = 1u,
    WDG_MODE_FAST = 2u
} Wdg_ModeType;

typedef struct {
    Wdg_ModeType initialMode;
    uint32       timeoutTicks;
} Wdg_ConfigType;

void           Wdg_Init(const Wdg_ConfigType* ConfigPtr);
void           Wdg_DeInit(void);
Std_ReturnType Wdg_SetMode(Wdg_ModeType Mode);
void           Wdg_SetTriggerCondition(uint16 timeout);
void           Wdg_Trigger(void);
boolean        Wdg_IsExpired(void);
void           Wdg_MainFunction(void);

#endif /* VMCAL_WDG_H */
