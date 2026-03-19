/* VecuCanTransceiver.h — Virtual CAN transceiver for OpenBSW.
 *
 * Extends AbstractCANTransceiver to route CAN frames through
 * vecu_base_context_t push_tx_frame / pop_rx_frame callbacks
 * instead of Linux SocketCAN.  This is the link-time MCAL
 * substitution for the CAN bus (ADR-002).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#pragma once

#include <can/transceiver/AbstractCANTransceiver.h>
#include "vecu_base_context.h"

namespace vecu
{

class VecuCanTransceiver final : public ::can::AbstractCANTransceiver
{
public:
    explicit VecuCanTransceiver(uint8_t busId);

    VecuCanTransceiver(VecuCanTransceiver const&)            = delete;
    VecuCanTransceiver& operator=(VecuCanTransceiver const&) = delete;

    ::can::ICanTransceiver::ErrorCode init() final;
    ::can::ICanTransceiver::ErrorCode open() final;
    ::can::ICanTransceiver::ErrorCode open(::can::CANFrame const& frame) final;
    ::can::ICanTransceiver::ErrorCode close() final;
    void shutdown() final;

    ::can::ICanTransceiver::ErrorCode write(::can::CANFrame const& frame) final;
    ::can::ICanTransceiver::ErrorCode write(
        ::can::CANFrame const& frame,
        ::can::ICANFrameSentListener& listener) final;

    ::can::ICanTransceiver::ErrorCode mute() final;
    ::can::ICanTransceiver::ErrorCode unmute() final;

    uint32_t getBaudrate() const final;
    uint16_t getHwQueueTimeout() const final;

    void poll(int maxReceivedPerRun);

private:
    static vecu_frame_t toVecuFrame(::can::CANFrame const& frame, uint8_t busId);
    static ::can::CANFrame fromVecuFrame(vecu_frame_t const& vf);

    bool _muted;
};

} // namespace vecu
