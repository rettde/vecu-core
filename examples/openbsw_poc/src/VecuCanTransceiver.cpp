/* VecuCanTransceiver.cpp — Virtual CAN transceiver for OpenBSW.
 *
 * Routes CAN frames through vecu_base_context_t callbacks instead of
 * Linux SocketCAN.  See VecuCanTransceiver.h for details.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu/VecuCanTransceiver.h"
#include "VMcal_Context.h"

#include <cstring>

namespace vecu
{

VecuCanTransceiver::VecuCanTransceiver(uint8_t const busId)
: AbstractCANTransceiver(busId)
, _muted(false)
{}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::init()
{
    setState(State::CLOSED);
    return ErrorCode::CAN_ERR_OK;
}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::open()
{
    setState(State::OPEN);
    return ErrorCode::CAN_ERR_OK;
}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::open(::can::CANFrame const& /* frame */)
{
    return open();
}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::close()
{
    setState(State::CLOSED);
    return ErrorCode::CAN_ERR_OK;
}

void VecuCanTransceiver::shutdown()
{
    close();
}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::write(::can::CANFrame const& frame)
{
    if (!isInState(State::OPEN) || _muted)
    {
        return ErrorCode::CAN_ERR_ILLEGAL_STATE;
    }

    vecu_base_context_t const* ctx = VMcal_GetCtx();
    if ((ctx == nullptr) || (ctx->push_tx_frame == nullptr))
    {
        return ErrorCode::CAN_ERR_ILLEGAL_STATE;
    }

    vecu_frame_t vf = toVecuFrame(frame, getBusId());
    int const rc    = ctx->push_tx_frame(&vf);
    if (rc != 0)
    {
        return ErrorCode::CAN_ERR_TX_HW_QUEUE_FULL;
    }

    notifySentListeners(frame);
    return ErrorCode::CAN_ERR_OK;
}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::write(
    ::can::CANFrame const& frame, ::can::ICANFrameSentListener& /* listener */)
{
    return write(frame);
}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::mute()
{
    _muted = true;
    return ErrorCode::CAN_ERR_OK;
}

::can::ICanTransceiver::ErrorCode VecuCanTransceiver::unmute()
{
    _muted = false;
    return ErrorCode::CAN_ERR_OK;
}

uint32_t VecuCanTransceiver::getBaudrate() const
{
    return 500000U;
}

uint16_t VecuCanTransceiver::getHwQueueTimeout() const
{
    return 100U;
}

void VecuCanTransceiver::poll(int const maxReceivedPerRun)
{
    if (!isInState(State::OPEN))
    {
        return;
    }

    vecu_base_context_t const* ctx = VMcal_GetCtx();
    if ((ctx == nullptr) || (ctx->pop_rx_frame == nullptr))
    {
        return;
    }

    for (int i = 0; i < maxReceivedPerRun; ++i)
    {
        vecu_frame_t vf;
        int const rc = ctx->pop_rx_frame(&vf);
        if (rc != 0)
        {
            break;
        }
        if (vf.bus_type != 0U)
        {
            continue;
        }

        ::can::CANFrame canFrame = fromVecuFrame(vf);
        notifyListeners(canFrame);
    }
}

vecu_frame_t VecuCanTransceiver::toVecuFrame(
    ::can::CANFrame const& frame, uint8_t const busId)
{
    vecu_frame_t vf;
    std::memset(&vf, 0, sizeof(vf));
    vf.id       = frame.getId();
    vf.len      = frame.getPayloadLength();
    vf.bus_type = 0U;
    vf.pad0     = busId;
    if (frame.getPayloadLength() > 0U)
    {
        std::memcpy(vf.data, frame.getPayload(), frame.getPayloadLength());
    }
    return vf;
}

::can::CANFrame VecuCanTransceiver::fromVecuFrame(vecu_frame_t const& vf)
{
    uint8_t len = static_cast<uint8_t>(vf.len);
    if (len > ::can::CANFrame::MAX_FRAME_LENGTH)
    {
        len = ::can::CANFrame::MAX_FRAME_LENGTH;
    }
    return ::can::CANFrame(vf.id, vf.data, len);
}

} // namespace vecu
