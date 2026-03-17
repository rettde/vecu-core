/* Wdg.c — Virtual Watchdog Driver (ADR-002 / Virtual-MCAL).
 *
 * Software watchdog with configurable timeout. Wdg_MainFunction
 * decrements the counter; Wdg_Trigger resets it.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Wdg.h"
#include <stddef.h>

static Wdg_ModeType g_mode    = WDG_MODE_OFF;
static uint32       g_timeout = 0;
static uint32       g_counter = 0;
static boolean      g_expired = FALSE;

void Wdg_Init(const Wdg_ConfigType* ConfigPtr) {
    if (ConfigPtr != NULL) {
        g_mode    = ConfigPtr->initialMode;
        g_timeout = ConfigPtr->timeoutTicks;
    } else {
        g_mode    = WDG_MODE_OFF;
        g_timeout = 100;
    }
    g_counter = g_timeout;
    g_expired = FALSE;
}

void Wdg_DeInit(void) {
    g_mode    = WDG_MODE_OFF;
    g_expired = FALSE;
}

Std_ReturnType Wdg_SetMode(Wdg_ModeType Mode) {
    g_mode = Mode;
    if (Mode == WDG_MODE_OFF) {
        g_expired = FALSE;
    }
    return E_OK;
}

void Wdg_SetTriggerCondition(uint16 timeout) {
    g_timeout = (uint32)timeout;
    g_counter = g_timeout;
}

void Wdg_Trigger(void) {
    if (g_mode != WDG_MODE_OFF) {
        g_counter = g_timeout;
        g_expired = FALSE;
    }
}

boolean Wdg_IsExpired(void) {
    return g_expired;
}

void Wdg_MainFunction(void) {
    if (g_mode == WDG_MODE_OFF) { return; }
    if (g_counter > 0) {
        g_counter--;
    } else {
        g_expired = TRUE;
    }
}
