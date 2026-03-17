/* Port.h — Virtual Port Driver API (ADR-002 / Virtual-MCAL).
 *
 * Init-semantics-only drop-in replacement for Port_* MCAL driver.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_PORT_H
#define VMCAL_PORT_H

#include "Std_Types.h"

typedef uint16 Port_PinType;

typedef enum {
    PORT_PIN_IN  = 0u,
    PORT_PIN_OUT = 1u
} Port_PinDirectionType;

typedef enum {
    PORT_PIN_LEVEL_LOW  = 0u,
    PORT_PIN_LEVEL_HIGH = 1u
} Port_PinLevelValue;

typedef struct {
    Port_PinType          pin;
    Port_PinDirectionType direction;
    Port_PinLevelValue    initValue;
} Port_PinConfigType;

typedef struct {
    const Port_PinConfigType* pins;
    uint16                    numPins;
} Port_ConfigType;

void Port_Init(const Port_ConfigType* ConfigPtr);
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);
void Port_SetPinMode(Port_PinType Pin, uint8 Mode);

#endif /* VMCAL_PORT_H */
