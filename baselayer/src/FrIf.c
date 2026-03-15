/* FrIf.c — FlexRay Interface stub (ADR-005 / P4).
 *
 * Same pattern as CanIf but for BusType::FlexRay.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "FrIf.h"
#include "PduR.h"
#include "vecu_base_context.h"
#include "vecu_frame.h"
#include <string.h>

extern const vecu_base_context_t* Base_GetCtx(void);

static boolean g_initialized = FALSE;

void FrIf_Init(void) {
    g_initialized = TRUE;
}

void FrIf_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType FrIf_Transmit(uint32_t FrameId, const uint8_t* Data, uint8_t Length) {
    if (!g_initialized || Data == NULL) { return E_NOT_OK; }

    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) { return E_NOT_OK; }

    vecu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.id       = FrameId;
    frame.bus_type = VECU_BUS_FLEXRAY;
    frame.len      = Length;
    memcpy(frame.data, Data, Length);

    int rc = ctx->push_tx_frame(&frame);
    return (rc == VECU_OK) ? E_OK : E_NOT_OK;
}

void FrIf_RxMainFunction(void) {
    /* Stub — same pattern as CanIf, filter for VECU_BUS_FLEXRAY. */
    (void)0;
}
