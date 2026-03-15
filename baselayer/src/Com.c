/* Com.c — Communication module implementation (ADR-005 / P4).
 *
 * Signal-level send/receive with configurable signal database.
 * Supports little-endian and big-endian signal packing/unpacking.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Com.h"
#include "PduR.h"
#include "vecu_frame.h"
#include <string.h>

/* ── Internal state ─────────────────────────────────────────────── */

static const Com_ConfigType* g_config = NULL;
static boolean g_initialized = FALSE;

/* PDU shadow buffers — one per configured PDU. */
static uint8_t g_pdu_data[COM_MAX_PDUS][COM_MAX_PDU_LEN];

/* Dirty flags for TX PDUs (set when a signal is written). */
static boolean g_pdu_dirty[COM_MAX_PDUS];

/* ── Helpers: signal pack / unpack ──────────────────────────────── */

/* Find signal config by ID. Returns NULL if not found. */
static const Com_SignalConfigType* find_signal(Com_SignalIdType id) {
    if (g_config == NULL) { return NULL; }
    for (uint16_t i = 0; i < g_config->num_signals; i++) {
        if (g_config->signals[i].signal_id == id) {
            return &g_config->signals[i];
        }
    }
    return NULL;
}

/* Find PDU config by ID. Returns NULL if not found. */
static const Com_PduConfigType* find_pdu(uint16_t pdu_id) {
    if (g_config == NULL) { return NULL; }
    for (uint16_t i = 0; i < g_config->num_pdus; i++) {
        if (g_config->pdus[i].pdu_id == pdu_id) {
            return &g_config->pdus[i];
        }
    }
    return NULL;
}

/* Find PDU index in g_pdu_data array. Returns -1 if not found. */
static int find_pdu_index(uint16_t pdu_id) {
    if (g_config == NULL) { return -1; }
    for (uint16_t i = 0; i < g_config->num_pdus; i++) {
        if (g_config->pdus[i].pdu_id == pdu_id) {
            return (int)i;
        }
    }
    return -1;
}

/* Pack a value into a PDU byte array at the given bit position.
 * Little-endian: LSBit at bit_position, contiguous towards MSBit.
 * Big-endian: MSBit at bit_position, bytes laid out big-endian style. */
static void pack_signal_le(uint8_t* pdu, uint16_t bit_pos,
                           uint16_t bit_len, uint32_t value) {
    for (uint16_t i = 0; i < bit_len; i++) {
        uint16_t bit_index = bit_pos + i;
        uint16_t byte_index = bit_index / 8u;
        uint8_t  bit_in_byte = (uint8_t)(bit_index % 8u);
        if (byte_index >= COM_MAX_PDU_LEN) { break; }
        if ((value >> i) & 1u) {
            pdu[byte_index] |= (uint8_t)(1u << bit_in_byte);
        } else {
            pdu[byte_index] &= (uint8_t)~(1u << bit_in_byte);
        }
    }
}

static uint32_t unpack_signal_le(const uint8_t* pdu, uint16_t bit_pos,
                                 uint16_t bit_len) {
    uint32_t value = 0;
    for (uint16_t i = 0; i < bit_len; i++) {
        uint16_t bit_index = bit_pos + i;
        uint16_t byte_index = bit_index / 8u;
        uint8_t  bit_in_byte = (uint8_t)(bit_index % 8u);
        if (byte_index >= COM_MAX_PDU_LEN) { break; }
        if (pdu[byte_index] & (1u << bit_in_byte)) {
            value |= (1u << i);
        }
    }
    return value;
}

/* Big-endian packing: MSBit of value goes to bit_position,
 * with byte-swapped layout as per AUTOSAR convention. */
static void pack_signal_be(uint8_t* pdu, uint16_t bit_pos,
                           uint16_t bit_len, uint32_t value) {
    /* For big-endian signals, bit_position refers to the MSBit.
     * We pack from MSBit down to LSBit. */
    for (uint16_t i = 0; i < bit_len; i++) {
        /* bit index of the (bit_len-1-i)-th bit of value */
        uint16_t val_bit = bit_len - 1u - i;
        /* Physical bit position: start at bit_pos, increment.
         * AUTOSAR big-endian bit numbering within a byte is MSB=7, LSB=0,
         * but across bytes the next bit wraps to the next byte.
         * Simplified: we lay out bits starting at bit_pos going forward. */
        uint16_t bit_index = bit_pos + i;
        uint16_t byte_index = bit_index / 8u;
        uint8_t  bit_in_byte = (uint8_t)(bit_index % 8u);
        if (byte_index >= COM_MAX_PDU_LEN) { break; }
        if ((value >> val_bit) & 1u) {
            pdu[byte_index] |= (uint8_t)(1u << bit_in_byte);
        } else {
            pdu[byte_index] &= (uint8_t)~(1u << bit_in_byte);
        }
    }
}

static uint32_t unpack_signal_be(const uint8_t* pdu, uint16_t bit_pos,
                                 uint16_t bit_len) {
    uint32_t value = 0;
    for (uint16_t i = 0; i < bit_len; i++) {
        uint16_t val_bit = bit_len - 1u - i;
        uint16_t bit_index = bit_pos + i;
        uint16_t byte_index = bit_index / 8u;
        uint8_t  bit_in_byte = (uint8_t)(bit_index % 8u);
        if (byte_index >= COM_MAX_PDU_LEN) { break; }
        if (pdu[byte_index] & (1u << bit_in_byte)) {
            value |= (1u << val_bit);
        }
    }
    return value;
}

/* ── API ────────────────────────────────────────────────────────── */

void Com_Init(const Com_ConfigType* config) {
    g_config = config;
    memset(g_pdu_data, 0, sizeof(g_pdu_data));
    memset(g_pdu_dirty, 0, sizeof(g_pdu_dirty));

    /* Write initial values for all signals into their PDU buffers. */
    if (config != NULL) {
        for (uint16_t i = 0; i < config->num_signals; i++) {
            const Com_SignalConfigType* sig = &config->signals[i];
            int idx = find_pdu_index(sig->pdu_id);
            if (idx < 0) { continue; }
            if (sig->endianness == COM_BIG_ENDIAN) {
                pack_signal_be(g_pdu_data[idx], sig->bit_position,
                               sig->bit_length, sig->init_value);
            } else {
                pack_signal_le(g_pdu_data[idx], sig->bit_position,
                               sig->bit_length, sig->init_value);
            }
        }
    }

    g_initialized = TRUE;
}

void Com_DeInit(void) {
    g_initialized = FALSE;
    g_config = NULL;
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr) {
    if (!g_initialized || SignalDataPtr == NULL) { return E_NOT_OK; }

    const Com_SignalConfigType* sig = find_signal(SignalId);
    if (sig == NULL || sig->direction != COM_DIRECTION_TX) { return E_NOT_OK; }

    int idx = find_pdu_index(sig->pdu_id);
    if (idx < 0) { return E_NOT_OK; }

    /* Read the value from the caller.  We support up to 32 bits. */
    uint32_t value = 0;
    uint8_t byte_len = (uint8_t)((sig->bit_length + 7u) / 8u);
    if (byte_len > 4u) { byte_len = 4u; }
    memcpy(&value, SignalDataPtr, byte_len);

    if (sig->endianness == COM_BIG_ENDIAN) {
        pack_signal_be(g_pdu_data[idx], sig->bit_position,
                       sig->bit_length, value);
    } else {
        pack_signal_le(g_pdu_data[idx], sig->bit_position,
                       sig->bit_length, value);
    }

    g_pdu_dirty[idx] = TRUE;
    return E_OK;
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr) {
    if (!g_initialized || SignalDataPtr == NULL) { return E_NOT_OK; }

    const Com_SignalConfigType* sig = find_signal(SignalId);
    if (sig == NULL) { return E_NOT_OK; }

    int idx = find_pdu_index(sig->pdu_id);
    if (idx < 0) { return E_NOT_OK; }

    uint32_t value;
    if (sig->endianness == COM_BIG_ENDIAN) {
        value = unpack_signal_be(g_pdu_data[idx], sig->bit_position,
                                 sig->bit_length);
    } else {
        value = unpack_signal_le(g_pdu_data[idx], sig->bit_position,
                                 sig->bit_length);
    }

    uint8_t byte_len = (uint8_t)((sig->bit_length + 7u) / 8u);
    if (byte_len > 4u) { byte_len = 4u; }
    memcpy(SignalDataPtr, &value, byte_len);

    return E_OK;
}

void Com_RxIndication(uint16_t PduId, const uint8_t* PduData, uint8_t Length) {
    if (!g_initialized || PduData == NULL) { return; }

    int idx = find_pdu_index(PduId);
    if (idx < 0) { return; }

    uint8_t copy_len = Length;
    if (copy_len > COM_MAX_PDU_LEN) { copy_len = COM_MAX_PDU_LEN; }
    memcpy(g_pdu_data[idx], PduData, copy_len);
}

Std_ReturnType Com_TriggerTransmit(uint16_t PduId, uint8_t* PduData, uint8_t* Length) {
    if (!g_initialized || PduData == NULL || Length == NULL) { return E_NOT_OK; }

    int idx = find_pdu_index(PduId);
    if (idx < 0) { return E_NOT_OK; }

    const Com_PduConfigType* pdu = find_pdu(PduId);
    if (pdu == NULL) { return E_NOT_OK; }

    *Length = pdu->dlc;
    memcpy(PduData, g_pdu_data[idx], pdu->dlc);
    return E_OK;
}

void Com_MainFunction(void) {
    if (!g_initialized || g_config == NULL) { return; }

    /* TX: for each dirty PDU, trigger transmit via PduR. */
    for (uint16_t i = 0; i < g_config->num_pdus; i++) {
        if (i >= COM_MAX_PDUS) { break; }
        if (!g_pdu_dirty[i]) { continue; }

        const Com_PduConfigType* pdu = &g_config->pdus[i];
        if (pdu->direction != COM_DIRECTION_TX) { continue; }

        PduR_ComTransmit(pdu->pdu_id, g_pdu_data[i], pdu->dlc, pdu->bus_type);
        g_pdu_dirty[i] = FALSE;
    }
}
