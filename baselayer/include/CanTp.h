/* CanTp.h — CAN Transport Protocol (ADR-005 / P7).
 *
 * ISO 15765-2 segmentation and reassembly:
 *   SF (Single Frame), FF (First Frame), CF (Consecutive Frame),
 *   FC (Flow Control).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef CANTP_H
#define CANTP_H

#include "Std_Types.h"

/* ── Constants ─────────────────────────────────────────────────────── */

#define CANTP_MAX_PAYLOAD       4095u  /* max ISO-TP message length */
#define CANTP_SF_MAX_DATA       7u     /* max SF data bytes (classic CAN) */
#define CANTP_FF_FIRST_DATA     6u     /* data bytes in FF */
#define CANTP_CF_DATA           7u     /* data bytes per CF */

/* Frame type nibbles (upper nibble of byte 0) */
#define CANTP_N_PCI_SF          0x00u
#define CANTP_N_PCI_FF          0x10u
#define CANTP_N_PCI_CF          0x20u
#define CANTP_N_PCI_FC          0x30u

/* Flow Control status */
#define CANTP_FC_CTS            0x00u  /* Continue To Send */
#define CANTP_FC_WAIT           0x01u
#define CANTP_FC_OVERFLOW       0x02u

/* ── Configuration ─────────────────────────────────────────────────── */

typedef struct {
    uint32 rxId;       /* CAN ID for RX (physical addressing) */
    uint32 txId;       /* CAN ID for TX */
    uint32 fcId;       /* CAN ID for outgoing FC frames */
    uint8  blockSize;  /* BS parameter in FC (0 = no limit) */
    uint8  stMin;      /* STmin in FC (ms) */
    uint16 _pad;
} CanTp_ChannelConfigType;

typedef struct {
    const CanTp_ChannelConfigType* channels;
    uint16 numChannels;
    uint16 _pad;
} CanTp_ConfigType;

/* ── Lifecycle ─────────────────────────────────────────────────────── */

void CanTp_Init(const CanTp_ConfigType* config);
void CanTp_DeInit(void);
void CanTp_MainFunction(void);

/* ── Transmit API ──────────────────────────────────────────────────── */

/** Request transmission of a message (may be segmented). */
Std_ReturnType CanTp_Transmit(uint16 channelId,
                              const uint8* data, uint16 length);

/* ── Reception indication (called by CanIf / PduR) ─────────────── */

/** Indicate reception of a CAN frame on the TP channel. */
void CanTp_RxIndication(uint32 canId, const uint8* data, uint8 dlc);

/* ── Upper-layer callback (weak — override in application) ───────── */

/** Called when a complete message has been reassembled.
 *  Default implementation forwards to Dcm_ProcessRequest if available. */
void CanTp_RxComplete(uint16 channelId,
                      const uint8* data, uint16 length);

#endif /* CANTP_H */
