/* Rte_SwcSensor.h — RTE interface for the Sensor SWC.
 *
 * Reads VehicleSpeed (signal 0) and BrakeActive (signal 2) from Com.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef RTE_SWCSENSOR_H
#define RTE_SWCSENSOR_H

#include "Std_Types.h"
#include "Com.h"

static inline Std_ReturnType Rte_Read_VehicleSpeed(uint32* value) {
    return Com_ReceiveSignal(0, value);
}

static inline Std_ReturnType Rte_Read_BrakeActive(uint32* value) {
    return Com_ReceiveSignal(2, value);
}

#endif /* RTE_SWCSENSOR_H */
