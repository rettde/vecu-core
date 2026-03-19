/* async/Types.h — vecu single-threaded async types.
 *
 * Replaces the FreeRTOS-specific async/Types.h with a minimal
 * single-threaded cooperative model for deterministic vECU execution.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#pragma once

#include "async/IRunnable.h"
#include "async/Lock.h"
#include "async/ModifiableLock.h"

#include <timer/Timeout.h>

#include <platform/estdint.h>

namespace async
{

using RunnableType       = IRunnable;
using ContextType        = uint8_t;
using EventMaskType      = uint32_t;
using LockType           = Lock;
using ModifiableLockType = ModifiableLock;

ContextType const CONTEXT_INVALID = 0xFFU;

struct TimeoutType : public ::timer::Timeout
{
public:
    TimeoutType() : _runnable(nullptr), _context(CONTEXT_INVALID) {}

    void cancel() { _runnable = nullptr; }

    void expired() override
    {
        if (_runnable != nullptr)
        {
            _runnable->execute();
        }
    }

    IRunnable* _runnable;
    ContextType _context;
};

struct TimeUnit
{
    enum Type
    {
        MICROSECONDS = 1,
        MILLISECONDS = 1000,
        SECONDS      = 1000000
    };
};

using TimeUnitType = TimeUnit::Type;

} // namespace async
