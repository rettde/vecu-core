/* MemIf.h — Memory Abstraction Interface (ADR-005 / P6).
 *
 * Stub routing layer between NvM and Fee.  In a multi-device setup
 * this would select the correct underlying driver.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef MEMIF_H
#define MEMIF_H

#include "Std_Types.h"

void MemIf_Init(void);
void MemIf_DeInit(void);

Std_ReturnType MemIf_Read(uint16 blockNumber, uint16 offset,
                          uint8* dataBufferPtr, uint16 length);
Std_ReturnType MemIf_Write(uint16 blockNumber,
                           const uint8* dataBufferPtr, uint16 length);

#endif /* MEMIF_H */
