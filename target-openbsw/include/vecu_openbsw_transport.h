/* vecu_openbsw_transport.h — OpenBSW CAN/ETH transport adapter.
 *
 * Routes OpenBSW cpp2can / cpp2ethernet frames through
 * vecu_base_context_t push_tx_frame / pop_rx_frame.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VECU_OPENBSW_TRANSPORT_H
#define VECU_OPENBSW_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vecu_base_context.h"
#include "vecu_frame.h"

int OpenBsw_TransmitCan(uint32_t id, const uint8_t* data, uint32_t len);
int OpenBsw_TransmitEth(const uint8_t* data, uint32_t len);
int OpenBsw_ReceiveCan(vecu_frame_t* frame);
int OpenBsw_ReceiveEth(vecu_frame_t* frame);
void OpenBsw_PollRxFrames(void);

#ifdef __cplusplus
}
#endif

#endif /* VECU_OPENBSW_TRANSPORT_H */
