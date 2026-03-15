/* SwcActuator.c — Actuator SWC: writes actuator command to CAN.
 *
 * Simple logic: if brake is active, command = 0; else command = speed / 10.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Rte_SwcActuator.h"

/* Import sensor accessors */
extern uint32 SwcSensor_GetVehicleSpeed(void);
extern uint32 SwcSensor_GetBrakeActive(void);

void SwcActuator_Init(void) {
    Rte_Write_ActuatorCommand(0);
}

void SwcActuator_MainFunction(void) {
    uint32 speed = SwcSensor_GetVehicleSpeed();
    uint32 brake = SwcSensor_GetBrakeActive();

    uint32 command;
    if (brake != 0u) {
        command = 0u;
    } else {
        command = speed / 10u;
    }

    Rte_Write_ActuatorCommand(command);
}
