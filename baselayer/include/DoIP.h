/* DoIP.h — Diagnostics over IP (ADR-005 / P7).
 *
 * ISO 13400 stub.  Provides header parsing and diagnostic message
 * routing for UDS over Ethernet.  In this minimal implementation
 * only the diagnostic message type is handled.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef DOIP_H
#define DOIP_H

#include "Std_Types.h"

/* ── DoIP header constants ─────────────────────────────────────── */

#define DOIP_PROTOCOL_VERSION   0x02u
#define DOIP_HEADER_SIZE        8u

/* Payload types */
#define DOIP_TYPE_ROUTING_ACTIVATION_REQ   0x0005u
#define DOIP_TYPE_ROUTING_ACTIVATION_RESP  0x0006u
#define DOIP_TYPE_DIAG_MESSAGE             0x8001u
#define DOIP_TYPE_DIAG_MESSAGE_ACK         0x8002u
#define DOIP_TYPE_DIAG_MESSAGE_NACK        0x8003u

/* ── Lifecycle ─────────────────────────────────────────────────── */

void DoIP_Init(void);
void DoIP_DeInit(void);
void DoIP_MainFunction(void);

/* ── Processing API ────────────────────────────────────────────── */

/** Process a DoIP packet (header + payload).
 *  Returns response length written to respBuf (0 = no response). */
uint16 DoIP_ProcessPacket(const uint8* packet, uint16 packetLen,
                          uint8* respBuf, uint16 respBufSize);

#endif /* DOIP_H */
