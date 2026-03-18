/* vecu_base_context.h — Callback context for the AUTOSAR BaseLayer (ADR-005).
 *
 * This header defines the sole coupling point between the Rust ABI bridge
 * (vecu-appl) and the C BaseLayer / application code.  The bridge builds
 * the context struct and injects it into Base_Init().
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VECU_BASE_CONTEXT_H
#define VECU_BASE_CONTEXT_H

#include <stdint.h>
#include "vecu_frame.h"
#include "vecu_status.h"

/* ------------------------------------------------------------------ */
/* Callback context (Rust bridge → C BaseLayer)                       */
/* ------------------------------------------------------------------ */

typedef struct vecu_base_context_t {
    /* ── Frame I/O (BaseLayer → Loader) ──────────────────────────── */

    /** Push a TX frame from the BaseLayer to the Loader.
     *  Returns VECU_OK on success. */
    int (*push_tx_frame)(const vecu_frame_t* frame);

    /** Pop an RX frame delivered by the Loader.
     *  Returns VECU_OK if a frame was written, VECU_NOT_SUPPORTED if empty. */
    int (*pop_rx_frame)(vecu_frame_t* frame);

    /* ── HSM Crypto Delegation ───────────────────────────────────── */

    /** AES-128 encrypt (ECB or CBC). */
    int (*hsm_encrypt)(uint32_t key_slot, uint32_t mode,
                       const uint8_t* data, uint32_t data_len,
                       const uint8_t* iv,
                       uint8_t* out, uint32_t* out_len);

    /** AES-128 decrypt (ECB or CBC). */
    int (*hsm_decrypt)(uint32_t key_slot, uint32_t mode,
                       const uint8_t* data, uint32_t data_len,
                       const uint8_t* iv,
                       uint8_t* out, uint32_t* out_len);

    /** Generate AES-128-CMAC over data. */
    int (*hsm_generate_mac)(uint32_t key_slot,
                            const uint8_t* data, uint32_t data_len,
                            uint8_t* out_mac, uint32_t* out_mac_len);

    /** Verify AES-128-CMAC over data. */
    int (*hsm_verify_mac)(uint32_t key_slot,
                          const uint8_t* data, uint32_t data_len,
                          const uint8_t* mac, uint32_t mac_len);

    /** Generate a SecurityAccess seed. */
    int (*hsm_seed)(uint8_t* out_seed, uint32_t* out_len);

    /** Validate a SecurityAccess key against the last seed. */
    int (*hsm_key)(const uint8_t* key_buf, uint32_t key_len);

    /** Generate cryptographically secure random bytes. */
    int (*hsm_rng)(uint8_t* out_buf, uint32_t buf_len);

    /* ── Shared Memory (variable / state block) ──────────────────── */

    /** Pointer to the SHM variable block (off_vars region). */
    void*    shm_vars;

    /** Size of the SHM variable block in bytes. */
    uint32_t shm_vars_size;

    /** Padding for 8-byte alignment. */
    uint32_t _pad0;

    /* ── Logging ─────────────────────────────────────────────────── */

    /** Log callback provided by the Loader.
     *  level: 0=trace, 1=debug, 2=info, 3=warn, 4=error.
     *  msg must be a NUL-terminated C string. */
    void (*log_fn)(uint32_t level, const char* msg);

    /* ── Time ────────────────────────────────────────────────────── */

    /** Simulation tick interval in microseconds. */
    uint64_t tick_interval_us;

    /* ── Hash ─────────────────────────────────────────────────────── */

    /** Compute a cryptographic hash (e.g. SHA-256).
     *  algorithm: 0=SHA-256.
     *  Returns VECU_OK on success. */
    int (*hsm_hash)(uint32_t algorithm,
                    const uint8_t* data, uint32_t data_len,
                    uint8_t* out, uint32_t* out_len);

} vecu_base_context_t;

/* ------------------------------------------------------------------ */
/* BaseLayer mandatory exports                                        */
/* ------------------------------------------------------------------ */

/** Initialise the BaseLayer.  Called once by vecu-appl during init.
 *  The BaseLayer must store ctx internally for the lifetime of the
 *  simulation (until Base_Shutdown is called). */
void Base_Init(const vecu_base_context_t* ctx);

/** Execute one simulation tick.  Called once per tick by vecu-appl.
 *  Drives SchM_MainFunction() and all registered BSW MainFunctions. */
void Base_Step(uint64_t tick);

/** Shut down the BaseLayer.  Called once by vecu-appl during shutdown.
 *  Must release all resources.  After this call the context pointer
 *  is invalid. */
void Base_Shutdown(void);

/* ------------------------------------------------------------------ */
/* Application code mandatory exports                                 */
/* ------------------------------------------------------------------ */

/** Initialise the ECU application (SWC init, Rte_Start equivalent). */
void Appl_Init(void);

/** Application main function, called each tick after Base_Step.
 *  Drives SWC runnables. */
void Appl_MainFunction(void);

/** Shut down the ECU application (SWC de-init, Rte_Stop equivalent). */
void Appl_Shutdown(void);

#endif /* VECU_BASE_CONTEXT_H */
