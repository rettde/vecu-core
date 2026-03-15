/* FrIf.h — FlexRay Interface stub (ADR-005 / P4).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef FRIF_H
#define FRIF_H

#include "Std_Types.h"

void FrIf_Init(void);
void FrIf_DeInit(void);
Std_ReturnType FrIf_Transmit(uint32_t FrameId, const uint8_t* Data, uint8_t Length);
void FrIf_RxMainFunction(void);

#endif /* FRIF_H */
