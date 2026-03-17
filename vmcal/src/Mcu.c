/* Mcu.c — Virtual MCU Driver (ADR-002 / Virtual-MCAL).
 *
 * Clock / Reset / Init-semantics. No real hardware interaction.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Mcu.h"

static boolean       g_initialized = FALSE;
static Mcu_ResetType g_reset_reason = MCU_POWER_ON_RESET;

void Mcu_Init(const Mcu_ConfigType* ConfigPtr) {
    (void)ConfigPtr;
    g_initialized  = TRUE;
    g_reset_reason = MCU_POWER_ON_RESET;
}

void Mcu_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType Mcu_InitClock(Mcu_ClockType ClockSetting) {
    (void)ClockSetting;
    if (!g_initialized) { return E_NOT_OK; }
    return E_OK;
}

Mcu_PllStatusType Mcu_GetPllStatus(void) {
    return g_initialized ? MCU_PLL_LOCKED : MCU_PLL_STATUS_UNDEFINED;
}

void Mcu_PerformReset(void) {
    g_reset_reason = MCU_SW_RESET;
}

Mcu_ResetType Mcu_GetResetReason(void) {
    return g_reset_reason;
}
