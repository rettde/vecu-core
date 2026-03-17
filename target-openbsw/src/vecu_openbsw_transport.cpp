/* vecu_openbsw_transport.cpp — OpenBSW CAN/ETH transport adapter.
 *
 * Routes frames between OpenBSW cpp2can/cpp2ethernet and
 * vecu_base_context_t push_tx_frame / pop_rx_frame.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu_openbsw_transport.h"
#include "vecu_openbsw_shim.h"

#include <string.h>

extern "C" int OpenBsw_TransmitCan(uint32_t id, const uint8_t* data, uint32_t len) {
    const vecu_base_context_t* ctx = OpenBsw_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return -1; }
    if (data == NULL && len > 0) { return -1; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = id;
    frame.bus_type = VECU_BUS_CAN;
    frame.len      = len;
    if (len > 0 && len <= VECU_MAX_FRAME_DATA) {
        memcpy(frame.data, data, len);
    }

    return ctx->push_tx_frame(&frame);
}

extern "C" int OpenBsw_TransmitEth(const uint8_t* data, uint32_t len) {
    const vecu_base_context_t* ctx = OpenBsw_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return -1; }
    if (data == NULL || len == 0 || len > VECU_MAX_FRAME_DATA) { return -1; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.bus_type = VECU_BUS_ETH;
    frame.len      = len;
    memcpy(frame.data, data, len);

    return ctx->push_tx_frame(&frame);
}

extern "C" int OpenBsw_ReceiveCan(vecu_frame_t* frame) {
    const vecu_base_context_t* ctx = OpenBsw_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL || frame == NULL) { return -1; }

    vecu_frame_t rx;
    memset(&rx, 0, sizeof(rx));
    int rc = ctx->pop_rx_frame(&rx);
    if (rc != VECU_OK) { return rc; }

    if (rx.bus_type == VECU_BUS_CAN) {
        memcpy(frame, &rx, sizeof(vecu_frame_t));
        return VECU_OK;
    }
    return VECU_NOT_SUPPORTED;
}

extern "C" int OpenBsw_ReceiveEth(vecu_frame_t* frame) {
    const vecu_base_context_t* ctx = OpenBsw_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL || frame == NULL) { return -1; }

    vecu_frame_t rx;
    memset(&rx, 0, sizeof(rx));
    int rc = ctx->pop_rx_frame(&rx);
    if (rc != VECU_OK) { return rc; }

    if (rx.bus_type == VECU_BUS_ETH) {
        memcpy(frame, &rx, sizeof(vecu_frame_t));
        return VECU_OK;
    }
    return VECU_NOT_SUPPORTED;
}

extern "C" void OpenBsw_PollRxFrames(void) {
    const vecu_base_context_t* ctx = OpenBsw_GetCtx();
    if (ctx == NULL || ctx->pop_rx_frame == NULL) { return; }

    vecu_frame_t rx;
    while (1) {
        memset(&rx, 0, sizeof(rx));
        int rc = ctx->pop_rx_frame(&rx);
        if (rc != VECU_OK) { break; }

        /* Route frame to appropriate OpenBSW handler based on bus type.
         *
         * In a full integration:
         *   VECU_BUS_CAN → ::can::CanTransceiver::onReceive(rx)
         *   VECU_BUS_ETH → ::ethernet::EthernetTransceiver::onReceive(rx)
         *
         * The OpenBSW cpp2can/cpp2ethernet layers dispatch to
         * registered listeners (docan, doip, application).
         */
        (void)rx;
    }
}
