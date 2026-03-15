/* Fee.h — Flash Emulation (ADR-005 / P6).
 *
 * RAM-backed emulation layer beneath NvM.  In a real ECU this would
 * abstract flash memory; here it simply provides a read/write API
 * that NvM can call (via MemIf) for its backing store.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef FEE_H
#define FEE_H

#include "Std_Types.h"

void Fee_Init(void);
void Fee_DeInit(void);

Std_ReturnType Fee_Read(uint16 blockNumber, uint16 offset,
                        uint8* dataBufferPtr, uint16 length);
Std_ReturnType Fee_Write(uint16 blockNumber,
                         const uint8* dataBufferPtr, uint16 length);

#endif /* FEE_H */
