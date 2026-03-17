/* Spi.c — Virtual SPI Driver (ADR-002 / Virtual-MCAL).
 *
 * No-Op / loopback implementation. Internal buffers allow WriteIB/ReadIB
 * round-trips without actual hardware.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Spi.h"
#include <string.h>

static Spi_StatusType g_status = SPI_UNINIT;
static uint8 g_ib[SPI_MAX_CHANNEL][SPI_IB_SIZE];

void Spi_Init(const Spi_ConfigType* ConfigPtr) {
    (void)ConfigPtr;
    memset(g_ib, 0, sizeof(g_ib));
    g_status = SPI_IDLE;
}

void Spi_DeInit(void) {
    g_status = SPI_UNINIT;
}

Std_ReturnType Spi_WriteIB(Spi_ChannelType Channel,
                           const Spi_DataBufferType* DataBufferPtr) {
    if (g_status == SPI_UNINIT || Channel >= SPI_MAX_CHANNEL) { return E_NOT_OK; }
    if (DataBufferPtr != NULL) {
        memcpy(g_ib[Channel], DataBufferPtr, SPI_IB_SIZE);
    }
    return E_OK;
}

Std_ReturnType Spi_ReadIB(Spi_ChannelType Channel,
                          Spi_DataBufferType* DataBufferPointer) {
    if (g_status == SPI_UNINIT || Channel >= SPI_MAX_CHANNEL || DataBufferPointer == NULL) {
        return E_NOT_OK;
    }
    memcpy(DataBufferPointer, g_ib[Channel], SPI_IB_SIZE);
    return E_OK;
}

Std_ReturnType Spi_AsyncTransmit(Spi_SequenceType Sequence) {
    (void)Sequence;
    if (g_status == SPI_UNINIT) { return E_NOT_OK; }
    return E_OK;
}

Spi_StatusType Spi_GetStatus(void) {
    return g_status;
}

Spi_SeqResultType Spi_GetSequenceResult(Spi_SequenceType Sequence) {
    (void)Sequence;
    return SPI_SEQ_OK;
}

void Spi_MainFunction(void) {
    if (g_status == SPI_UNINIT) { return; }
}
