/* Mcu.h — Virtual MCU Driver API (ADR-002 / Virtual-MCAL).
 *
 * Clock / Reset / Init-semantics drop-in replacement for Mcu_* MCAL driver.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_MCU_H
#define VMCAL_MCU_H

#include "Std_Types.h"

typedef uint8 Mcu_ClockType;

typedef enum {
    MCU_POWER_ON_RESET  = 0u,
    MCU_SW_RESET        = 1u,
    MCU_WATCHDOG_RESET  = 2u,
    MCU_RESET_UNDEFINED = 3u
} Mcu_ResetType;

typedef enum {
    MCU_PLL_LOCKED      = 0u,
    MCU_PLL_UNLOCKED    = 1u,
    MCU_PLL_STATUS_UNDEFINED = 2u
} Mcu_PllStatusType;

typedef struct {
    Mcu_ClockType defaultClock;
} Mcu_ConfigType;

void               Mcu_Init(const Mcu_ConfigType* ConfigPtr);
void               Mcu_DeInit(void);
Std_ReturnType     Mcu_InitClock(Mcu_ClockType ClockSetting);
Mcu_PllStatusType  Mcu_GetPllStatus(void);
void               Mcu_PerformReset(void);
Mcu_ResetType      Mcu_GetResetReason(void);

#endif /* VMCAL_MCU_H */
