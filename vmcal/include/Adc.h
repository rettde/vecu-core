/* Adc.h — Virtual ADC Driver API (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed ADC channel values. No real hardware.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_ADC_H
#define VMCAL_ADC_H

#include "Std_Types.h"

#define ADC_MAX_CHANNELS 32u
#define ADC_MAX_GROUPS    8u

typedef uint8  Adc_ChannelType;
typedef uint8  Adc_GroupType;
typedef uint16 Adc_ValueGroupType;

typedef enum {
    ADC_IDLE     = 0u,
    ADC_BUSY     = 1u,
    ADC_COMPLETED = 2u
} Adc_StatusType;

typedef struct {
    const Adc_ChannelType* channels;
    uint8                  numChannels;
    Adc_GroupType          groupId;
} Adc_GroupDefType;

typedef struct {
    const Adc_GroupDefType* groups;
    uint8                   numGroups;
} Adc_ConfigType;

void            Adc_Init(const Adc_ConfigType* ConfigPtr);
void            Adc_DeInit(void);
void            Adc_StartGroupConversion(Adc_GroupType Group);
void            Adc_StopGroupConversion(Adc_GroupType Group);
Adc_StatusType  Adc_GetGroupStatus(Adc_GroupType Group);
Std_ReturnType  Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType* DataBufferPtr);
void            Adc_SetChannelValue(Adc_ChannelType Channel, Adc_ValueGroupType Value);
void            Adc_MainFunction(void);

#endif /* VMCAL_ADC_H */
