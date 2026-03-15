/* LinIf.h — LIN Interface stub (ADR-005 / P4).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef LINIF_H
#define LINIF_H

#include "Std_Types.h"

void LinIf_Init(void);
void LinIf_DeInit(void);
Std_ReturnType LinIf_Transmit(uint32_t FrameId, const uint8_t* Data, uint8_t Length);
void LinIf_RxMainFunction(void);

#endif /* LINIF_H */
