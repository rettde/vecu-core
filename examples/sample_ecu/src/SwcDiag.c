/* SwcDiag.c — Diagnostic SWC: handles RoutineControl requests.
 *
 * Provides a sample routine (0x0201) that returns the current
 * vehicle speed as a 2-byte status record.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Rte_SwcDiag.h"
#include <string.h>

extern uint32 SwcSensor_GetVehicleSpeed(void);

/* Routine 0x0201: read vehicle speed */
Std_ReturnType SwcDiag_Routine0201_Start(const uint8* optionRecord, uint16 optLen,
                                          uint8* statusRecord, uint16* statusLen)
{
    (void)optionRecord;
    (void)optLen;

    uint32 speed = SwcSensor_GetVehicleSpeed();
    statusRecord[0] = (uint8)(speed >> 8u);
    statusRecord[1] = (uint8)(speed & 0xFFu);
    *statusLen = 2;
    return E_OK;
}

void SwcDiag_Init(void) {
    /* Nothing to initialise */
}

void SwcDiag_MainFunction(void) {
    /* Could periodically report DTCs based on conditions */
    uint32 speed = SwcSensor_GetVehicleSpeed();
    if (speed > 250u) {
        /* Over-speed DTC */
        Rte_ReportDtc(0x00C1_0100u, 1u);
    }
}
