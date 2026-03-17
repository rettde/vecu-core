/* Pwm.h — Virtual PWM Driver API (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed PWM duty cycle and period. No real hardware.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_PWM_H
#define VMCAL_PWM_H

#include "Std_Types.h"

#define PWM_MAX_CHANNELS 16u

typedef uint8  Pwm_ChannelType;
typedef uint16 Pwm_PeriodType;
typedef uint16 Pwm_DutyType;

typedef struct {
    uint8 numChannels;
} Pwm_ConfigType;

void Pwm_Init(const Pwm_ConfigType* ConfigPtr);
void Pwm_DeInit(void);
void Pwm_SetDutyCycle(Pwm_ChannelType ChannelNumber, Pwm_DutyType DutyCycle);
void Pwm_SetPeriodAndDuty(Pwm_ChannelType ChannelNumber,
                           Pwm_PeriodType Period, Pwm_DutyType DutyCycle);
void Pwm_SetOutputToIdle(Pwm_ChannelType ChannelNumber);
Pwm_DutyType  Pwm_GetDutyCycle(Pwm_ChannelType ChannelNumber);
Pwm_PeriodType Pwm_GetPeriod(Pwm_ChannelType ChannelNumber);

#endif /* VMCAL_PWM_H */
