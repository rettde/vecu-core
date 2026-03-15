/* vecu_frame.h — C-compatible VecuFrame definition (ADR-001 / ADR-003).
 *
 * Must stay in sync with vecu_abi::VecuFrame in lib.rs.
 * sizeof(vecu_frame_t) == 1560 bytes.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VECU_FRAME_H
#define VECU_FRAME_H

#include <stdint.h>

/* Maximum payload bytes in a vecu_frame_t.
 * Sized for full Ethernet frames (1518 bytes incl. header).
 * Also covers FlexRay (<=254), CAN-FD (<=64), LIN (<=8). */
#define VECU_MAX_FRAME_DATA 1536

/* Bus type discriminator for vecu_frame_t.bus_type. */
#define VECU_BUS_CAN      0
#define VECU_BUS_ETH      1
#define VECU_BUS_LIN      2
#define VECU_BUS_FLEXRAY  3

/* Communication frame for inter-module / bus I/O.
 * repr(C), matches Rust VecuFrame exactly. */
typedef struct vecu_frame_t {
    uint32_t id;                         /* Frame / message identifier.       */
    uint32_t len;                        /* Valid bytes in data[].            */
    uint32_t bus_type;                   /* VECU_BUS_* discriminator.         */
    uint32_t pad0;                       /* Reserved (must be zero).          */
    uint8_t  data[VECU_MAX_FRAME_DATA];  /* Payload bytes.                    */
    uint64_t timestamp;                  /* Monotonic tick when created.      */
} vecu_frame_t;

/* Compile-time size check (must equal 1560). */
_Static_assert(sizeof(vecu_frame_t) == 1560,
               "vecu_frame_t size must be 1560 bytes");

#endif /* VECU_FRAME_H */
