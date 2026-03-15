/* CanTp.c — CAN Transport Protocol implementation (ADR-005 / P7).
 *
 * ISO 15765-2 segmentation (TX) and reassembly (RX).
 * Single-channel simplex: one TX and one RX in progress at a time.
 *
 * TX flow: CanTp_Transmit → SF or FF+CF sequence.
 * RX flow: CanTp_RxIndication → reassemble → CanTp_RxComplete callback.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "CanTp.h"
#include "CanIf.h"
#include <string.h>
#include <stddef.h>

/* ── Internal state ─────────────────────────────────────────────── */

static boolean g_initialized = FALSE;
static const CanTp_ConfigType* g_config = NULL;

/* RX reassembly buffer (single channel) */
static uint8  g_rx_buf[CANTP_MAX_PAYLOAD];
static uint16 g_rx_total = 0;
static uint16 g_rx_pos   = 0;
static uint8  g_rx_sn    = 0;  /* expected sequence number */
static boolean g_rx_active = FALSE;

/* TX segmentation state (single channel) */
static uint8  g_tx_buf[CANTP_MAX_PAYLOAD];
static uint16 g_tx_total = 0;
static uint16 g_tx_pos   = 0;
static uint8  g_tx_sn    = 0;
static uint16 g_tx_channel = 0;
static boolean g_tx_active = FALSE;
static uint8  g_tx_bs_remaining = 0;  /* blocks remaining before FC needed */
static boolean g_tx_fc_wait = FALSE;

/* ── Helpers ────────────────────────────────────────────────────── */

static const CanTp_ChannelConfigType* get_channel(uint16 id) {
    if (g_config == NULL || id >= g_config->numChannels) { return NULL; }
    return &g_config->channels[id];
}

static const CanTp_ChannelConfigType* find_channel_by_rx_id(uint32 canId) {
    if (g_config == NULL) { return NULL; }
    for (uint16 i = 0; i < g_config->numChannels; i++) {
        if (g_config->channels[i].rxId == canId) {
            return &g_config->channels[i];
        }
    }
    return NULL;
}

static void send_can_frame(uint32 canId, const uint8* data, uint8 dlc) {
    CanIf_Transmit(canId, data, dlc);
}

static void send_fc(uint32 fcId, uint8 fs, uint8 bs, uint8 stMin) {
    uint8 fc[8];
    memset(fc, 0xCC, sizeof(fc));  /* padding */
    fc[0] = (uint8)(CANTP_N_PCI_FC | (fs & 0x0Fu));
    fc[1] = bs;
    fc[2] = stMin;
    send_can_frame(fcId, fc, 8);
}

/* ── Lifecycle ──────────────────────────────────────────────────── */

void CanTp_Init(const CanTp_ConfigType* config) {
    g_config = config;
    g_rx_active = FALSE;
    g_tx_active = FALSE;
    g_initialized = TRUE;
}

void CanTp_DeInit(void) {
    g_initialized = FALSE;
    g_config = NULL;
}

void CanTp_MainFunction(void) {
    if (!g_initialized || !g_tx_active || g_tx_fc_wait) { return; }

    /* Send next CF if TX is in progress */
    const CanTp_ChannelConfigType* ch = get_channel(g_tx_channel);
    if (ch == NULL) { g_tx_active = FALSE; return; }

    uint8 frame[8];
    memset(frame, 0xCC, sizeof(frame));
    frame[0] = (uint8)(CANTP_N_PCI_CF | (g_tx_sn & 0x0Fu));

    uint16 remaining = g_tx_total - g_tx_pos;
    uint8 copyLen = (remaining > CANTP_CF_DATA) ? CANTP_CF_DATA : (uint8)remaining;
    memcpy(&frame[1], &g_tx_buf[g_tx_pos], copyLen);
    g_tx_pos += copyLen;
    g_tx_sn = (uint8)((g_tx_sn + 1u) & 0x0Fu);

    send_can_frame(ch->txId, frame, 8);

    if (g_tx_pos >= g_tx_total) {
        g_tx_active = FALSE;
        return;
    }

    /* Block size handling */
    if (ch->blockSize != 0) {
        g_tx_bs_remaining--;
        if (g_tx_bs_remaining == 0) {
            g_tx_fc_wait = TRUE;  /* wait for FC */
        }
    }
}

/* ── Transmit API ───────────────────────────────────────────────── */

Std_ReturnType CanTp_Transmit(uint16 channelId,
                              const uint8* data, uint16 length)
{
    if (!g_initialized || data == NULL || length == 0u) { return E_NOT_OK; }
    if (length > CANTP_MAX_PAYLOAD) { return E_NOT_OK; }
    if (g_tx_active) { return E_NOT_OK; }  /* busy */

    const CanTp_ChannelConfigType* ch = get_channel(channelId);
    if (ch == NULL) { return E_NOT_OK; }

    if (length <= CANTP_SF_MAX_DATA) {
        /* Single Frame */
        uint8 frame[8];
        memset(frame, 0xCC, sizeof(frame));
        frame[0] = (uint8)(CANTP_N_PCI_SF | (length & 0x0Fu));
        memcpy(&frame[1], data, length);
        send_can_frame(ch->txId, frame, 8);
        return E_OK;
    }

    /* First Frame + Consecutive Frames */
    memcpy(g_tx_buf, data, length);
    g_tx_total = length;
    g_tx_channel = channelId;
    g_tx_sn = 1;
    g_tx_active = TRUE;
    g_tx_fc_wait = FALSE;
    g_tx_bs_remaining = ch->blockSize;

    uint8 ff[8];
    memset(ff, 0xCC, sizeof(ff));
    ff[0] = (uint8)(CANTP_N_PCI_FF | ((length >> 8u) & 0x0Fu));
    ff[1] = (uint8)(length & 0xFFu);
    uint8 copyLen = (length - 0u > CANTP_FF_FIRST_DATA) ? CANTP_FF_FIRST_DATA : (uint8)length;
    memcpy(&ff[2], data, copyLen);
    g_tx_pos = copyLen;

    send_can_frame(ch->txId, ff, 8);

    /* For BS=0 (no flow control needed), MainFunction will send CFs immediately */
    if (ch->blockSize != 0) {
        g_tx_fc_wait = TRUE;  /* wait for FC before sending CFs */
    }

    return E_OK;
}

/* ── RX indication ──────────────────────────────────────────────── */

void CanTp_RxIndication(uint32 canId, const uint8* data, uint8 dlc) {
    if (!g_initialized || data == NULL || dlc == 0u) { return; }

    uint8 pciType = data[0] & 0xF0u;

    /* Check for FC on TX channel */
    if (pciType == CANTP_N_PCI_FC && g_tx_active) {
        uint8 fs = data[0] & 0x0Fu;
        if (fs == CANTP_FC_CTS) {
            uint8 bs = data[1];
            /* uint8 stMin = data[2]; — ignored in synchronous mode */
            g_tx_bs_remaining = bs;
            g_tx_fc_wait = FALSE;
        } else if (fs == CANTP_FC_OVERFLOW) {
            g_tx_active = FALSE;
        }
        /* FC_WAIT: keep waiting */
        return;
    }

    const CanTp_ChannelConfigType* ch = find_channel_by_rx_id(canId);

    switch (pciType) {
        case CANTP_N_PCI_SF: {
            uint8 sfLen = data[0] & 0x0Fu;
            if (sfLen == 0u || sfLen > CANTP_SF_MAX_DATA || sfLen + 1u > dlc) { return; }
            CanTp_RxComplete(ch ? (uint16)(ch - g_config->channels) : 0u,
                             &data[1], sfLen);
            break;
        }

        case CANTP_N_PCI_FF: {
            uint16 totalLen = (uint16)(((uint16)(data[0] & 0x0Fu) << 8u) | data[1]);
            if (totalLen == 0u || totalLen > CANTP_MAX_PAYLOAD) { return; }
            g_rx_total = totalLen;
            uint8 copyLen = (dlc - 2u > CANTP_FF_FIRST_DATA) ? CANTP_FF_FIRST_DATA : (uint8)(dlc - 2u);
            if (copyLen > totalLen) { copyLen = (uint8)totalLen; }
            memcpy(g_rx_buf, &data[2], copyLen);
            g_rx_pos = copyLen;
            g_rx_sn = 1;
            g_rx_active = TRUE;

            /* Send FC (CTS) */
            if (ch != NULL) {
                send_fc(ch->fcId, CANTP_FC_CTS, ch->blockSize, ch->stMin);
            }
            break;
        }

        case CANTP_N_PCI_CF: {
            if (!g_rx_active) { return; }
            uint8 sn = data[0] & 0x0Fu;
            if (sn != g_rx_sn) {
                /* Sequence error — abort */
                g_rx_active = FALSE;
                return;
            }
            uint16 remaining = g_rx_total - g_rx_pos;
            uint8 copyLen = (remaining > CANTP_CF_DATA) ? CANTP_CF_DATA : (uint8)remaining;
            if (copyLen + 1u > dlc) { copyLen = (uint8)(dlc - 1u); }
            memcpy(&g_rx_buf[g_rx_pos], &data[1], copyLen);
            g_rx_pos += copyLen;
            g_rx_sn = (uint8)((g_rx_sn + 1u) & 0x0Fu);

            if (g_rx_pos >= g_rx_total) {
                g_rx_active = FALSE;
                CanTp_RxComplete(ch ? (uint16)(ch - g_config->channels) : 0u,
                                 g_rx_buf, g_rx_total);
            }
            break;
        }

        default:
            break;
    }
}

/* ── Default RxComplete (weak) ──────────────────────────────────── */

__attribute__((weak))
void CanTp_RxComplete(uint16 channelId,
                      const uint8* data, uint16 length)
{
    (void)channelId;
    (void)data;
    (void)length;
    /* Default: no-op.  Application or BaseLayer overrides this. */
}
