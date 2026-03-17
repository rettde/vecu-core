/* Adc.c — Virtual ADC Driver (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed ADC channel values. Conversion completes immediately
 * in Adc_MainFunction (single-tick latency).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Adc.h"
#include <string.h>

static Adc_ValueGroupType g_channel_values[ADC_MAX_CHANNELS];
static Adc_StatusType     g_group_status[ADC_MAX_GROUPS];
static boolean            g_initialized = FALSE;

void Adc_Init(const Adc_ConfigType* ConfigPtr) {
    (void)ConfigPtr;
    memset(g_channel_values, 0, sizeof(g_channel_values));
    memset(g_group_status, ADC_IDLE, sizeof(g_group_status));
    g_initialized = TRUE;
}

void Adc_DeInit(void) {
    g_initialized = FALSE;
}

void Adc_StartGroupConversion(Adc_GroupType Group) {
    if (!g_initialized || Group >= ADC_MAX_GROUPS) { return; }
    g_group_status[Group] = ADC_BUSY;
}

void Adc_StopGroupConversion(Adc_GroupType Group) {
    if (!g_initialized || Group >= ADC_MAX_GROUPS) { return; }
    g_group_status[Group] = ADC_IDLE;
}

Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group) {
    if (!g_initialized || Group >= ADC_MAX_GROUPS) { return ADC_IDLE; }
    return g_group_status[Group];
}

Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType* DataBufferPtr) {
    if (!g_initialized || Group >= ADC_MAX_GROUPS || DataBufferPtr == NULL) {
        return E_NOT_OK;
    }
    if (g_group_status[Group] != ADC_COMPLETED) { return E_NOT_OK; }
    *DataBufferPtr = g_channel_values[Group < ADC_MAX_CHANNELS ? Group : 0];
    g_group_status[Group] = ADC_IDLE;
    return E_OK;
}

void Adc_SetChannelValue(Adc_ChannelType Channel, Adc_ValueGroupType Value) {
    if (Channel < ADC_MAX_CHANNELS) {
        g_channel_values[Channel] = Value;
    }
}

void Adc_MainFunction(void) {
    if (!g_initialized) { return; }
    uint8 i;
    for (i = 0; i < ADC_MAX_GROUPS; i++) {
        if (g_group_status[i] == ADC_BUSY) {
            g_group_status[i] = ADC_COMPLETED;
        }
    }
}
