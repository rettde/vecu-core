/* VMcal_Context.c — Virtual-MCAL runtime context storage (ADR-002).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "VMcal_Context.h"
#include <stddef.h>

static const vecu_base_context_t* g_ctx = NULL;

void VMcal_Init(const vecu_base_context_t* ctx) {
    g_ctx = ctx;
}

void VMcal_Shutdown(void) {
    g_ctx = NULL;
}

const vecu_base_context_t* VMcal_GetCtx(void) {
    return g_ctx;
}
