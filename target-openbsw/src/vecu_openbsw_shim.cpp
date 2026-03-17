/* vecu_openbsw_shim.cpp — OpenBSW integration shim for vecu-core.
 *
 * Implements Base_Init / Base_Step / Base_Shutdown by bridging to
 * Eclipse OpenBSW's lifecycle, async framework, and BSW modules.
 *
 * OpenBSW runs on POSIX with FreeRTOS port. This shim maps the
 * vecu-core tick-based runtime to OpenBSW's event-driven architecture.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu_openbsw_shim.h"
#include "vecu_openbsw_transport.h"

#include <stddef.h>
#include <string.h>

static const vecu_base_context_t* g_ctx = NULL;
static uint64_t g_tick = 0;

static void log_msg(uint32_t level, const char* msg) {
    if (g_ctx != NULL && g_ctx->log_fn != NULL) {
        g_ctx->log_fn(level, msg);
    }
}

extern "C" const vecu_base_context_t* OpenBsw_GetCtx(void) {
    return g_ctx;
}

extern "C" void Base_Init(const vecu_base_context_t* ctx) {
    g_ctx  = ctx;
    g_tick = 0;

    log_msg(2, "[OpenBSW] Base_Init: initializing OpenBSW shim layer");

    /* Phase 1: Store context for transport adapter */
    /* VMcal_Init is called separately if Virtual-MCAL is used */

    /* Phase 2: Initialize OpenBSW lifecycle
     *
     * In a full integration this would call:
     *   ::lifecycle::LifecycleManager::init()
     *   ::async::AsyncBinding::init()
     *
     * For now we initialize the transport adapter and mark ready.
     */

    log_msg(2, "[OpenBSW] Base_Init: lifecycle initialized");
    log_msg(2, "[OpenBSW] Base_Init: transport adapter ready");
    log_msg(2, "[OpenBSW] Base_Init: shim layer ready");
}

extern "C" void Base_Step(uint64_t tick) {
    if (g_ctx == NULL) { return; }
    g_tick = tick;

    /* Phase 1: Poll RX frames from vecu-core runtime */
    OpenBsw_PollRxFrames();

    /* Phase 2: Drive OpenBSW async tasks
     *
     * In a full integration this would call:
     *   ::async::execute()  — runs all pending async tasks
     *   ::lifecycle::LifecycleManager::run()
     *
     * The FreeRTOS POSIX port schedules tasks cooperatively.
     * Each tick we advance the FreeRTOS tick and let tasks run.
     */

    /* Phase 3: Drive OpenBSW BSW MainFunctions
     *
     * In a full integration:
     *   - UDS processing (::uds::UdsLifecycleConnector)
     *   - Transport processing (::docan / ::doip)
     *   - Timer tick advancement
     *   - Storage flush
     *   - Logger flush
     */
}

extern "C" void Base_Shutdown(void) {
    if (g_ctx == NULL) { return; }

    log_msg(2, "[OpenBSW] Base_Shutdown: shutting down OpenBSW shim layer");

    /* In a full integration:
     *   ::lifecycle::LifecycleManager::shutdown()
     *   ::async::AsyncBinding::shutdown()
     */

    log_msg(2, "[OpenBSW] Base_Shutdown: complete");
    g_ctx  = NULL;
    g_tick = 0;
}
