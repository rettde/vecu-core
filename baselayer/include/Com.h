/* Com.h — Communication module (ADR-005 / P4).
 *
 * Signal-level send/receive with configurable signal database.
 * Supports little-endian and big-endian signal packing.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef COM_H
#define COM_H

#include "Std_Types.h"

/* ── Signal ID type ─────────────────────────────────────────────── */

typedef uint16_t Com_SignalIdType;

/* ── Signal endianness ──────────────────────────────────────────── */

#define COM_LITTLE_ENDIAN  0u
#define COM_BIG_ENDIAN     1u

/* ── Signal direction ───────────────────────────────────────────── */

#define COM_DIRECTION_RX   0u
#define COM_DIRECTION_TX   1u

/* ── Configuration limits ───────────────────────────────────────── */

#define COM_MAX_SIGNALS    64u
#define COM_MAX_PDUS       32u
#define COM_MAX_PDU_LEN    64u

/* ── Signal configuration (one entry per signal) ────────────────── */

typedef struct {
    Com_SignalIdType signal_id;
    uint16_t         pdu_id;
    uint16_t         bit_position;
    uint16_t         bit_length;
    uint8_t          endianness;   /* COM_LITTLE_ENDIAN or COM_BIG_ENDIAN */
    uint8_t          direction;    /* COM_DIRECTION_RX or COM_DIRECTION_TX */
    uint32_t         init_value;
} Com_SignalConfigType;

/* ── PDU configuration (one entry per PDU) ──────────────────────── */

typedef struct {
    uint16_t pdu_id;
    uint32_t frame_id;     /* CAN ID, ETH type, etc. */
    uint8_t  dlc;          /* PDU length in bytes */
    uint8_t  direction;    /* COM_DIRECTION_RX or COM_DIRECTION_TX */
    uint8_t  bus_type;     /* VECU_BUS_CAN, VECU_BUS_ETH, etc. */
    uint8_t  _pad;
} Com_PduConfigType;

/* ── Full Com configuration ─────────────────────────────────────── */

typedef struct {
    const Com_SignalConfigType* signals;
    uint16_t                    num_signals;
    const Com_PduConfigType*    pdus;
    uint16_t                    num_pdus;
} Com_ConfigType;

/* ── API ────────────────────────────────────────────────────────── */

void Com_Init(const Com_ConfigType* config);
void Com_DeInit(void);
void Com_MainFunction(void);

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);

/* Called by PduR when an RX PDU arrives. */
void Com_RxIndication(uint16_t PduId, const uint8_t* PduData, uint8_t Length);

/* Called by PduR to get TX data for a PDU. */
Std_ReturnType Com_TriggerTransmit(uint16_t PduId, uint8_t* PduData, uint8_t* Length);

#endif /* COM_H */
