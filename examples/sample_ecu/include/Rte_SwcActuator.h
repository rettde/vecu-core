/* Rte_SwcActuator.h — RTE interface for the Actuator SWC.
 *
 * Writes TxSignal (signal 3) to Com for actuator command output.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef RTE_SWCACTUATOR_H
#define RTE_SWCACTUATOR_H

#include "Std_Types.h"
#include "Com.h"

static inline Std_ReturnType Rte_Write_ActuatorCommand(uint32 value) {
    return Com_SendSignal(3, value);
}

#endif /* RTE_SWCACTUATOR_H */
