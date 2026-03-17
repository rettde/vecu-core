/* Port.c — Virtual Port Driver (ADR-002 / Virtual-MCAL).
 *
 * Init-semantics-only implementation. Stores pin directions in RAM.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Port.h"
#include <stddef.h>

#define PORT_MAX_PINS 256u

static Port_PinDirectionType g_directions[PORT_MAX_PINS];
static boolean g_initialized = FALSE;

void Port_Init(const Port_ConfigType* ConfigPtr) {
    uint16 i;
    if (ConfigPtr == NULL) {
        g_initialized = TRUE;
        return;
    }
    for (i = 0; i < ConfigPtr->numPins && i < PORT_MAX_PINS; i++) {
        Port_PinType pin = ConfigPtr->pins[i].pin;
        if (pin < PORT_MAX_PINS) {
            g_directions[pin] = ConfigPtr->pins[i].direction;
        }
    }
    g_initialized = TRUE;
}

void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction) {
    if (!g_initialized || Pin >= PORT_MAX_PINS) { return; }
    g_directions[Pin] = Direction;
}

void Port_SetPinMode(Port_PinType Pin, uint8 Mode) {
    (void)Pin;
    (void)Mode;
}
