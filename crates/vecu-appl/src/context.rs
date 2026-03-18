//! Callback context construction for the C `BaseLayer` (ADR-005).
//!
//! Builds a `VecuBaseContext` that the Rust bridge injects into
//! `Base_Init()`.  All callback function pointers are C-callable
//! trampolines that delegate back to module-level Rust state.

use vecu_abi::{status, VecuFrame};

// ---------------------------------------------------------------------------
// C-compatible context struct (mirrors vecu_base_context.h)
// ---------------------------------------------------------------------------

/// C-compatible callback context injected into the `BaseLayer`.
///
/// Must match the layout of `vecu_base_context_t` in
/// `crates/vecu-abi/include/vecu_base_context.h`.
#[repr(C)]
pub(crate) struct VecuBaseContext {
    // Frame I/O
    pub push_tx_frame: Option<unsafe extern "C" fn(*const VecuFrame) -> i32>,
    pub pop_rx_frame: Option<unsafe extern "C" fn(*mut VecuFrame) -> i32>,

    // HSM crypto delegation
    pub hsm_encrypt:
        Option<unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32>,
    pub hsm_decrypt:
        Option<unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32>,
    pub hsm_generate_mac:
        Option<unsafe extern "C" fn(u32, *const u8, u32, *mut u8, *mut u32) -> i32>,
    pub hsm_verify_mac: Option<unsafe extern "C" fn(u32, *const u8, u32, *const u8, u32) -> i32>,
    pub hsm_seed: Option<unsafe extern "C" fn(*mut u8, *mut u32) -> i32>,
    pub hsm_key: Option<unsafe extern "C" fn(*const u8, u32) -> i32>,
    pub hsm_rng: Option<unsafe extern "C" fn(*mut u8, u32) -> i32>,

    // Shared memory (variable / state block)
    pub shm_vars: *mut u8,
    pub shm_vars_size: u32,
    pub _pad0: u32,

    // Logging
    pub log_fn: Option<unsafe extern "C" fn(u32, *const core::ffi::c_char)>,

    // Time
    pub tick_interval_us: u64,

    // Hash
    pub hsm_hash:
        Option<unsafe extern "C" fn(u32, *const u8, u32, *mut u8, *mut u32) -> i32>,
}

// SAFETY: VecuBaseContext is a plain C struct with no interior mutability.
// All pointers are owned by the Loader and only accessed single-threaded.
#[allow(unsafe_code)]
unsafe impl Send for VecuBaseContext {}
#[allow(unsafe_code)]
unsafe impl Sync for VecuBaseContext {}

// ---------------------------------------------------------------------------
// Trampoline callbacks (C-callable, delegate to module statics)
// ---------------------------------------------------------------------------

/// Push a TX frame from the `BaseLayer` into our outbound buffer.
#[allow(unsafe_code)]
unsafe extern "C" fn trampoline_push_tx(frame: *const VecuFrame) -> i32 {
    if frame.is_null() {
        return status::INVALID_ARGUMENT;
    }
    let frame = unsafe { &*frame };
    let mut guard = super::TX_FRAME.lock().expect("lock");
    *guard = Some(*frame);
    super::TX_FRAME_VALID.store(true, std::sync::atomic::Ordering::Release);
    status::OK
}

/// Pop an RX frame from our inbound buffer for the `BaseLayer`.
#[allow(unsafe_code)]
unsafe extern "C" fn trampoline_pop_rx(frame: *mut VecuFrame) -> i32 {
    if frame.is_null() {
        return status::INVALID_ARGUMENT;
    }
    let mut guard = super::LAST_INBOUND.lock().expect("lock");
    if let Some(inbound) = guard.take() {
        unsafe { *frame = inbound };
        status::OK
    } else {
        status::NOT_SUPPORTED
    }
}

/// Log callback trampoline — forwards to the stored loader log function.
#[allow(unsafe_code)]
unsafe extern "C" fn trampoline_log(level: u32, msg: *const core::ffi::c_char) {
    let log_fn_ptr = super::BRIDGE_LOG_FN.load(std::sync::atomic::Ordering::Acquire);
    if !log_fn_ptr.is_null() {
        // Reconstruct the function pointer and call it.
        // The level from C uses 0-4 (trace..error), our ABI uses i32.
        #[allow(clippy::cast_possible_wrap)]
        let level_i32 = level as i32;
        unsafe {
            let f: unsafe extern "C" fn(i32, *const core::ffi::c_char) =
                core::mem::transmute(log_fn_ptr);
            f(level_i32, msg);
        }
    }
}

// ---------------------------------------------------------------------------
// Builder
// ---------------------------------------------------------------------------

impl VecuBaseContext {
    /// Build a new context from the current module state.
    ///
    /// When `hsm_api` is non-null, the HSM plugin's crypto function pointers
    /// are wired into the context so that the `BaseLayer`'s `Cry` → `CryIf` → `Csm`
    /// chain can delegate to the vecu-hsm plugin.
    ///
    /// # Safety
    ///
    /// `hsm_api` must be null or point to a valid, fully-initialised
    /// [`VecuPluginApi`](vecu_abi::VecuPluginApi).
    #[allow(unsafe_code)]
    pub(crate) fn build(
        shm_vars: *mut u8,
        shm_vars_size: u32,
        tick_interval_us: u64,
        hsm_api: *const vecu_abi::VecuPluginApi,
    ) -> Self {
        let (enc, dec, gen_mac, ver_mac, seed, key, rng, hash) = if hsm_api.is_null() {
            (None, None, None, None, None, None, None, None)
        } else {
            let api = unsafe { &*hsm_api };
            (
                api.encrypt,
                api.decrypt,
                api.generate_mac,
                api.verify_mac,
                api.seed,
                api.key,
                api.rng,
                api.hash,
            )
        };

        Self {
            push_tx_frame: Some(trampoline_push_tx),
            pop_rx_frame: Some(trampoline_pop_rx),
            hsm_encrypt: enc,
            hsm_decrypt: dec,
            hsm_generate_mac: gen_mac,
            hsm_verify_mac: ver_mac,
            hsm_seed: seed,
            hsm_key: key,
            hsm_rng: rng,
            shm_vars,
            shm_vars_size,
            _pad0: 0,
            log_fn: Some(trampoline_log),
            tick_interval_us,
            hsm_hash: hash,
        }
    }
}
