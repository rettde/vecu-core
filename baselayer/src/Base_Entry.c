/* Base_Entry.c — BaseLayer entry points (ADR-005 / P3).
 *
 * Implements the three mandatory exports:
 *   Base_Init(ctx)   — stores context, initialises EcuM
 *   Base_Step(tick)   — advances Os tick, calls EcuM_MainFunction
 *   Base_Shutdown()   — calls EcuM_GoSleep, invalidates context
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <stddef.h>
#include "vecu_base_context.h"
#include "EcuM.h"
#include "Os.h"
#include "Com.h"

/* ── Stored context ─────────────────────────────────────────────── */

static const vecu_base_context_t* g_ctx = NULL;

/* ── Accessors for other BSW modules ────────────────────────────── */

/* Returns the stored context pointer (or NULL if not initialised).
 * Used by CanIf, Det, etc. to access callbacks. */
const vecu_base_context_t* Base_GetCtx(void) {
    return g_ctx;
}

/* Returns the log function pointer from the stored context.
 * Det.c calls this to forward errors to the Rust bridge. */
void (*Base_GetLogFn(void))(uint32_t level, const char* msg) {
    if (g_ctx != NULL && g_ctx->log_fn != NULL) {
        return g_ctx->log_fn;
    }
    return NULL;
}

/* ── Platform export macro ──────────────────────────────────────── */

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

/* ── Mandatory exports ──────────────────────────────────────────── */

/* ── Default Com configuration (hardcoded for P4) ──────────────── */

/* Example signals — projects override this in their own Base_Entry. */
static const Com_SignalConfigType g_default_signals[] = {
    { /* VehicleSpeed: 16-bit LE at bit 0 of PDU 0x100, RX */
      .signal_id    = 0,
      .pdu_id       = 0x100,
      .bit_position = 0,
      .bit_length   = 16,
      .endianness   = COM_LITTLE_ENDIAN,
      .direction    = COM_DIRECTION_RX,
      .init_value   = 0
    },
    { /* EngineRpm: 16-bit BE at bit 0 of PDU 0x101, RX */
      .signal_id    = 1,
      .pdu_id       = 0x101,
      .bit_position = 0,
      .bit_length   = 16,
      .endianness   = COM_BIG_ENDIAN,
      .direction    = COM_DIRECTION_RX,
      .init_value   = 0
    },
    { /* BrakeActive: 1-bit LE at bit 16 of PDU 0x100, RX */
      .signal_id    = 2,
      .pdu_id       = 0x100,
      .bit_position = 16,
      .bit_length   = 1,
      .endianness   = COM_LITTLE_ENDIAN,
      .direction    = COM_DIRECTION_RX,
      .init_value   = 0
    },
    { /* TxSignal: 8-bit LE at bit 0 of PDU 0x200, TX */
      .signal_id    = 3,
      .pdu_id       = 0x200,
      .bit_position = 0,
      .bit_length   = 8,
      .endianness   = COM_LITTLE_ENDIAN,
      .direction    = COM_DIRECTION_TX,
      .init_value   = 0
    },
};

static const Com_PduConfigType g_default_pdus[] = {
    { .pdu_id = 0x100, .frame_id = 0x600, .dlc = 8,
      .direction = COM_DIRECTION_RX, .bus_type = VECU_BUS_CAN, ._pad = 0 },
    { .pdu_id = 0x101, .frame_id = 0x601, .dlc = 8,
      .direction = COM_DIRECTION_RX, .bus_type = VECU_BUS_CAN, ._pad = 0 },
    { .pdu_id = 0x200, .frame_id = 0x700, .dlc = 8,
      .direction = COM_DIRECTION_TX, .bus_type = VECU_BUS_CAN, ._pad = 0 },
};

static const Com_ConfigType g_default_com_config = {
    .signals     = g_default_signals,
    .num_signals = sizeof(g_default_signals) / sizeof(g_default_signals[0]),
    .pdus        = g_default_pdus,
    .num_pdus    = sizeof(g_default_pdus) / sizeof(g_default_pdus[0]),
};

EXPORT void Base_Init(const vecu_base_context_t* ctx) {
    g_ctx = ctx;
    EcuM_Init();
    /* Initialise Com with default signal database after EcuM
     * (EcuM inits CanIf, PduR, etc. which Com depends on). */
    Com_Init(&g_default_com_config);
}

EXPORT void Base_Step(uint64_t tick) {
    Os_SetTick(tick);
    EcuM_MainFunction();
}

EXPORT void Base_Shutdown(void) {
    EcuM_GoSleep();
    g_ctx = NULL;
}
