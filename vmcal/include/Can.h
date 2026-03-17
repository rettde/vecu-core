/* Can.h — Virtual CAN Driver API (ADR-002 / Virtual-MCAL).
 *
 * Drop-in replacement for Can_30_* MCAL driver.
 * Routes CAN frames through vecu_base_context_t callbacks.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_CAN_H
#define VMCAL_CAN_H

#include "Std_Types.h"

typedef uint16 Can_HwHandleType;

typedef struct {
    uint32 id;
    uint8  length;
    uint8  _pad[3];
    const uint8* sdu;
} Can_PduType;

typedef struct {
    uint16 numHth;
    uint16 numHrh;
} Can_ConfigType;

void            Can_Init(const Can_ConfigType* Config);
void            Can_DeInit(void);
Std_ReturnType  Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void            Can_MainFunction_Read(void);
void            Can_MainFunction_Write(void);

#endif /* VMCAL_CAN_H */
