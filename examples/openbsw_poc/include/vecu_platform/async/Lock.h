/* async/Lock.h — vecu single-threaded stub.
 *
 * No-op lock for single-threaded vECU execution.
 * Replaces the FreeRTOS interrupt-suspension based Lock.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#pragma once

namespace async
{

class Lock
{
public:
    Lock()  = default;
    ~Lock() = default;
};

} // namespace async
