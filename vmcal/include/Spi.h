/* Spi.h — Virtual SPI Driver API (ADR-002 / Virtual-MCAL).
 *
 * No-Op / loopback drop-in replacement for Spi_* MCAL driver.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_SPI_H
#define VMCAL_SPI_H

#include "Std_Types.h"

typedef uint8  Spi_ChannelType;
typedef uint8  Spi_SequenceType;
typedef uint8  Spi_DataBufferType;
typedef uint8  Spi_JobType;

typedef enum {
    SPI_UNINIT     = 0u,
    SPI_IDLE       = 1u,
    SPI_BUSY       = 2u
} Spi_StatusType;

typedef enum {
    SPI_SEQ_OK      = 0u,
    SPI_SEQ_PENDING = 1u,
    SPI_SEQ_FAILED  = 2u
} Spi_SeqResultType;

#define SPI_MAX_CHANNEL 16u
#define SPI_IB_SIZE     64u

typedef struct {
    uint8 numChannels;
} Spi_ConfigType;

void              Spi_Init(const Spi_ConfigType* ConfigPtr);
void              Spi_DeInit(void);
Std_ReturnType    Spi_WriteIB(Spi_ChannelType Channel,
                              const Spi_DataBufferType* DataBufferPtr);
Std_ReturnType    Spi_ReadIB(Spi_ChannelType Channel,
                             Spi_DataBufferType* DataBufferPointer);
Std_ReturnType    Spi_AsyncTransmit(Spi_SequenceType Sequence);
Spi_StatusType    Spi_GetStatus(void);
Spi_SeqResultType Spi_GetSequenceResult(Spi_SequenceType Sequence);
void              Spi_MainFunction(void);

#endif /* VMCAL_SPI_H */
