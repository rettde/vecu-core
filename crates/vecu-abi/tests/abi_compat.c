/* abi_compat.c — Compile-time ABI compatibility checks.
 *
 * This file is compiled by the Rust test suite (via the cc crate) to
 * verify that the C headers produce identical struct layouts to the
 * Rust definitions.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <stddef.h>
#include "vecu_frame.h"
#include "vecu_base_context.h"

/* ── Size checks ──────────────────────────────────────────────────── */

_Static_assert(sizeof(vecu_frame_t) == 1560,
               "vecu_frame_t must be 1560 bytes");

/* ── Field offset checks for vecu_frame_t ─────────────────────────── */

_Static_assert(offsetof(vecu_frame_t, id) == 0,
               "vecu_frame_t.id must be at offset 0");
_Static_assert(offsetof(vecu_frame_t, len) == 4,
               "vecu_frame_t.len must be at offset 4");
_Static_assert(offsetof(vecu_frame_t, bus_type) == 8,
               "vecu_frame_t.bus_type must be at offset 8");
_Static_assert(offsetof(vecu_frame_t, pad0) == 12,
               "vecu_frame_t.pad0 must be at offset 12");
_Static_assert(offsetof(vecu_frame_t, data) == 16,
               "vecu_frame_t.data must be at offset 16");
_Static_assert(offsetof(vecu_frame_t, timestamp) == 1552,
               "vecu_frame_t.timestamp must be at offset 1552");

/* ── Status code checks ───────────────────────────────────────────── */

_Static_assert(VECU_OK == 0, "VECU_OK must be 0");
_Static_assert(VECU_VERSION_MISMATCH == -1, "VECU_VERSION_MISMATCH must be -1");
_Static_assert(VECU_INVALID_ARGUMENT == -2, "VECU_INVALID_ARGUMENT must be -2");
_Static_assert(VECU_INIT_FAILED == -3, "VECU_INIT_FAILED must be -3");
_Static_assert(VECU_NOT_SUPPORTED == -4, "VECU_NOT_SUPPORTED must be -4");
_Static_assert(VECU_MODULE_ERROR == -5, "VECU_MODULE_ERROR must be -5");

/* ── Bus type checks ──────────────────────────────────────────────── */

_Static_assert(VECU_BUS_CAN == 0, "VECU_BUS_CAN must be 0");
_Static_assert(VECU_BUS_ETH == 1, "VECU_BUS_ETH must be 1");
_Static_assert(VECU_BUS_LIN == 2, "VECU_BUS_LIN must be 2");
_Static_assert(VECU_BUS_FLEXRAY == 3, "VECU_BUS_FLEXRAY must be 3");

/* ── MAX_FRAME_DATA check ─────────────────────────────────────────── */

_Static_assert(VECU_MAX_FRAME_DATA == 1536,
               "VECU_MAX_FRAME_DATA must be 1536");

/* ── Export sizes for Rust to read ────────────────────────────────── */

const size_t C_SIZEOF_VECU_FRAME = sizeof(vecu_frame_t);
const size_t C_SIZEOF_VECU_BASE_CONTEXT = sizeof(vecu_base_context_t);
const size_t C_OFFSETOF_FRAME_TIMESTAMP = offsetof(vecu_frame_t, timestamp);
