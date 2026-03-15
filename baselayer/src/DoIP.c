/* DoIP.c — Diagnostics over IP implementation (ADR-005 / P7).
 *
 * ISO 13400 stub.  Parses DoIP headers, extracts UDS payloads from
 * diagnostic messages, and forwards them to Dcm_ProcessRequest.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "DoIP.h"
#include "Dcm.h"
#include <string.h>
#include <stddef.h>

static boolean g_initialized = FALSE;
static boolean g_routing_active = FALSE;

/* ── Helpers ────────────────────────────────────────────────────── */

static uint16 read_u16_be(const uint8* p) {
    return (uint16)(((uint16)p[0] << 8u) | p[1]);
}

static uint32 read_u32_be(const uint8* p) {
    return ((uint32)p[0] << 24u) | ((uint32)p[1] << 16u) |
           ((uint32)p[2] << 8u)  | (uint32)p[3];
}

static void write_u16_be(uint8* p, uint16 val) {
    p[0] = (uint8)(val >> 8u);
    p[1] = (uint8)(val & 0xFFu);
}

static void write_u32_be(uint8* p, uint32 val) {
    p[0] = (uint8)(val >> 24u);
    p[1] = (uint8)(val >> 16u);
    p[2] = (uint8)(val >> 8u);
    p[3] = (uint8)(val & 0xFFu);
}

static uint16 build_header(uint8* buf, uint16 payloadType, uint32 payloadLen) {
    buf[0] = DOIP_PROTOCOL_VERSION;
    buf[1] = (uint8)~DOIP_PROTOCOL_VERSION;
    write_u16_be(&buf[2], payloadType);
    write_u32_be(&buf[4], payloadLen);
    return DOIP_HEADER_SIZE;
}

/* ── Lifecycle ──────────────────────────────────────────────────── */

void DoIP_Init(void) {
    g_routing_active = FALSE;
    g_initialized = TRUE;
}

void DoIP_DeInit(void) {
    g_initialized = FALSE;
    g_routing_active = FALSE;
}

void DoIP_MainFunction(void) {
    (void)0;
}

/* ── Processing ─────────────────────────────────────────────────── */

uint16 DoIP_ProcessPacket(const uint8* packet, uint16 packetLen,
                          uint8* respBuf, uint16 respBufSize)
{
    if (!g_initialized || packet == NULL || respBuf == NULL) { return 0; }
    if (packetLen < DOIP_HEADER_SIZE) { return 0; }

    /* Parse header */
    uint8 ver = packet[0];
    if (ver != DOIP_PROTOCOL_VERSION) { return 0; }

    uint16 payloadType = read_u16_be(&packet[2]);
    uint32 payloadLen  = read_u32_be(&packet[4]);
    if (DOIP_HEADER_SIZE + payloadLen > packetLen) { return 0; }

    const uint8* payload = &packet[DOIP_HEADER_SIZE];

    switch (payloadType) {
        case DOIP_TYPE_ROUTING_ACTIVATION_REQ: {
            /* Minimal routing activation: always accept */
            g_routing_active = TRUE;
            /* Response: header + source addr (2) + tester addr (2) + activation code (1) */
            uint16 respPayloadLen = 5;
            if (DOIP_HEADER_SIZE + respPayloadLen > respBufSize) { return 0; }
            uint16 pos = build_header(respBuf, DOIP_TYPE_ROUTING_ACTIVATION_RESP, respPayloadLen);
            /* tester logical address (echo from request, first 2 bytes) */
            if (payloadLen >= 2u) {
                respBuf[pos++] = payload[0];
                respBuf[pos++] = payload[1];
            } else {
                respBuf[pos++] = 0x00;
                respBuf[pos++] = 0x00;
            }
            /* entity logical address */
            respBuf[pos++] = 0x00;
            respBuf[pos++] = 0x01;
            /* routing activation response code: success */
            respBuf[pos++] = 0x10;
            return pos;
        }

        case DOIP_TYPE_DIAG_MESSAGE: {
            if (!g_routing_active) { return 0; }
            /* Payload: source addr (2) + target addr (2) + UDS data */
            if (payloadLen < 5u) { return 0; }
            uint16 srcAddr = read_u16_be(&payload[0]);
            /* uint16 tgtAddr = read_u16_be(&payload[2]); */
            const uint8* udsData = &payload[4];
            uint16 udsLen = (uint16)(payloadLen - 4u);

            /* Forward to Dcm */
            uint8 udsResp[DCM_MAX_BUFFER];
            uint16 udsRespLen = Dcm_ProcessRequest(udsData, udsLen, udsResp, DCM_MAX_BUFFER);
            if (udsRespLen == 0u) { return 0; }

            /* Build DoIP diagnostic message response */
            uint32 diagPayloadLen = 4u + (uint32)udsRespLen;
            if (DOIP_HEADER_SIZE + diagPayloadLen > respBufSize) { return 0; }
            uint16 pos = build_header(respBuf, DOIP_TYPE_DIAG_MESSAGE, diagPayloadLen);
            /* target addr becomes source in response */
            respBuf[pos++] = 0x00;
            respBuf[pos++] = 0x01;
            /* source addr becomes target */
            write_u16_be(&respBuf[pos], srcAddr);
            pos += 2u;
            memcpy(&respBuf[pos], udsResp, udsRespLen);
            pos += udsRespLen;
            return pos;
        }

        default:
            return 0;
    }
}
