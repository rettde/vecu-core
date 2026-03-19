/* openbsw_bridge.cpp — C++ lifecycle bridge: vecu ABI → OpenBSW.
 *
 * Provides the three mandatory Base_* entry points that vecu-appl
 * calls (see vecu_base_context.h).  Internally drives the OpenBSW
 * LifecycleManager through its runlevels.
 *
 * This file replaces the C-only openbsw_shim.c when building with
 * real OpenBSW sources.  It links against OpenBSW's C++ libraries
 * and provides the platform-specific BSP implementation using
 * vecu_base_context_t callbacks.
 *
 * Build: only compiled when OPENBSW_ROOT is set in CMake.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu_base_context.h"
#include "VMcal_Context.h"
#include "vecu/VecuCanTransceiver.h"

#ifdef VECU_BUILD
extern "C" {
#include "Os_Mapping.h"
}
#endif

#include <lifecycle/LifecycleManager.h>
#include <async/AsyncBinding.h>

#include <cstdint>
#include <cstdio>

#ifdef _WIN32
  #define EXPORT extern "C" __declspec(dllexport)
#else
  #define EXPORT extern "C" __attribute__((visibility("default")))
#endif

// ---------------------------------------------------------------------------
// OpenBSW platform layer: vecu-specific implementations
// ---------------------------------------------------------------------------

namespace
{

constexpr size_t MaxNumComponents         = 16;
constexpr size_t MaxNumLevels             = 9;
constexpr size_t MaxNumComponentsPerLevel = MaxNumComponents;

using LifecycleManagerType = ::lifecycle::declare::
    LifecycleManager<MaxNumComponents, MaxNumLevels, MaxNumComponentsPerLevel>;

// Forward declarations for OpenBSW platform hooks.
// These are provided by the application or platform-specific code.
// When building a specific vECU, they would register the actual
// LifecycleComponents from the series ECU project.
void vecuPlatformLifecycleAdd(
    ::lifecycle::LifecycleManager& manager, uint8_t level);

// Static instances
static bool                           g_bridge_initialized = false;
static vecu::VecuCanTransceiver*      g_can_transceiver    = nullptr;

uint32_t getTimestampMs()
{
    // In a real integration this would read the OS tick counter.
    // For now, return 0 — the lifecycle manager uses this only for logging.
    return 0U;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Weak default: no-op platform lifecycle registration.
// A real vECU project overrides this to add its LifecycleComponents.
// ---------------------------------------------------------------------------

#ifndef _WIN32
__attribute__((weak))
#endif
void vecuPlatformLifecycleAdd(
    ::lifecycle::LifecycleManager& /* manager */, uint8_t /* level */)
{}

// ---------------------------------------------------------------------------
// Base_* entry points (vecu ABI)
// ---------------------------------------------------------------------------

EXPORT void Base_Init(vecu_base_context_t const* ctx)
{
    if (ctx == nullptr) { return; }

    // Store context for Virtual-MCAL modules.
    VMcal_Init(ctx);

#ifdef VECU_BUILD
    Os_Init(nullptr);
#endif

    g_bridge_initialized = true;

    // Create virtual CAN transceiver (bus 0).
    static vecu::VecuCanTransceiver canTransceiver(0);
    g_can_transceiver = &canTransceiver;
    canTransceiver.init();
    canTransceiver.open();
}

EXPORT void Base_Step(uint64_t tick)
{
    if (!g_bridge_initialized) { return; }

#ifdef VECU_BUILD
    Os_Tick(static_cast<uint32_t>(tick));
#else
    (void)tick;
#endif

    // Poll RX frames from the Rust runtime into OpenBSW CAN listeners.
    if (g_can_transceiver != nullptr)
    {
        g_can_transceiver->poll(16);
    }
}

EXPORT void Base_Shutdown(void)
{
    if (!g_bridge_initialized) { return; }

    if (g_can_transceiver != nullptr)
    {
        g_can_transceiver->shutdown();
        g_can_transceiver = nullptr;
    }

#ifdef VECU_BUILD
    Os_Shutdown();
#endif

    g_bridge_initialized = false;
}

// ---------------------------------------------------------------------------
// Accessor for platform code to get the virtual CAN transceiver.
// ---------------------------------------------------------------------------

namespace vecu
{
::can::ICanTransceiver* getCanTransceiver()
{
    return g_can_transceiver;
}
} // namespace vecu
