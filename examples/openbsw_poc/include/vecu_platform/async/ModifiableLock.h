/* async/ModifiableLock.h — vecu single-threaded stub.
 *
 * No-op modifiable lock for single-threaded vECU execution.
 * Replaces the FreeRTOS interrupt-suspension based ModifiableLock.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#pragma once

namespace async
{

class ModifiableLock final
{
public:
    ModifiableLock() : _isLocked(true) {}
    ~ModifiableLock() = default;

    void unlock() { _isLocked = false; }
    void lock() { _isLocked = true; }

private:
    bool _isLocked;
};

} // namespace async
