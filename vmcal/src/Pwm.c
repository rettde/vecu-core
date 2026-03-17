/* Pwm.c — Virtual PWM Driver (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed PWM duty cycle and period values.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Pwm.h"
#include <string.h>

static Pwm_DutyType   g_duty[PWM_MAX_CHANNELS];
static Pwm_PeriodType g_period[PWM_MAX_CHANNELS];
static boolean        g_initialized = FALSE;

void Pwm_Init(const Pwm_ConfigType* ConfigPtr) {
    (void)ConfigPtr;
    memset(g_duty, 0, sizeof(g_duty));
    memset(g_period, 0, sizeof(g_period));
    g_initialized = TRUE;
}

void Pwm_DeInit(void) {
    g_initialized = FALSE;
}

void Pwm_SetDutyCycle(Pwm_ChannelType ChannelNumber, Pwm_DutyType DutyCycle) {
    if (!g_initialized || ChannelNumber >= PWM_MAX_CHANNELS) { return; }
    g_duty[ChannelNumber] = DutyCycle;
}

void Pwm_SetPeriodAndDuty(Pwm_ChannelType ChannelNumber,
                           Pwm_PeriodType Period, Pwm_DutyType DutyCycle) {
    if (!g_initialized || ChannelNumber >= PWM_MAX_CHANNELS) { return; }
    g_period[ChannelNumber] = Period;
    g_duty[ChannelNumber] = DutyCycle;
}

void Pwm_SetOutputToIdle(Pwm_ChannelType ChannelNumber) {
    if (!g_initialized || ChannelNumber >= PWM_MAX_CHANNELS) { return; }
    g_duty[ChannelNumber] = 0;
}

Pwm_DutyType Pwm_GetDutyCycle(Pwm_ChannelType ChannelNumber) {
    if (!g_initialized || ChannelNumber >= PWM_MAX_CHANNELS) { return 0; }
    return g_duty[ChannelNumber];
}

Pwm_PeriodType Pwm_GetPeriod(Pwm_ChannelType ChannelNumber) {
    if (!g_initialized || ChannelNumber >= PWM_MAX_CHANNELS) { return 0; }
    return g_period[ChannelNumber];
}
