/* Can.c — Virtual CAN Driver (ADR-002 / Virtual-MCAL).
 *
 * Routes CAN frames through vecu_base_context_t push_tx_frame / pop_rx_frame.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Can.h"
#include "VMcal_Context.h"
#include "vecu_frame.h"
#include <string.h>

static boolean g_initialized = FALSE;

void Can_Init(const Can_ConfigType* Config) {
    (void)Config;
    g_initialized = TRUE;
}

void Can_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo) {
    if (!g_initialized || PduInfo == NULL || PduInfo->sdu == NULL) {
        return E_NOT_OK;
    }
    const vecu_base_context_t* ctx = VMcal_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = PduInfo->id;
    frame.bus_type = VECU_BUS_CAN;
    frame.len      = PduInfo->length;
    if (PduInfo->length > 0) {
        memcpy(frame.data, PduInfo->sdu, PduInfo->length);
    }

    (void)Hth;
    int rc = ctx->push_tx_frame(&frame);
    return (rc == VECU_OK) ? E_OK : E_NOT_OK;
}

void Can_MainFunction_Read(void) {
    if (!g_initialized) { return; }
    /* RX is handled by the upper layer (CanIf_RxMainFunction).
     * This stub exists for API completeness. */
}

void Can_MainFunction_Write(void) {
    if (!g_initialized) { return; }
    /* TX confirmation handling — no-op in virtual environment. */
}
