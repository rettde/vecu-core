//! APPL module for the vECU execution system (ADR‑002 / ADR‑005).
//!
//! Exports the single symbol `vecu_get_api` (ADR‑001).
//! Compiled as `cdylib` for dynamic loading and `rlib` for tests.
//!
//! **Two modes of operation:**
//!
//! - **Bridge mode** (ADR‑005): loads external `BaseLayer` + ECU application
//!   shared libraries via `VECU_BASE_LIB` / `VECU_APPL_LIB` env vars and
//!   delegates lifecycle calls to the C code.
//! - **Echo mode** (fallback): when no env vars are set, echoes the last
//!   inbound frame as outbound (reference implementation from ADR‑002).

#[allow(unsafe_code)]
mod bridge;
#[allow(unsafe_code)]
mod context;

use std::sync::atomic::{AtomicBool, AtomicPtr, AtomicU32, Ordering};
use std::sync::Mutex;

use vecu_abi::{
    status, ModuleKind, VecuFrame, VecuPluginApi, VecuRuntimeContext, ABI_VERSION, CAP_FRAME_IO,
};

// ---------------------------------------------------------------------------
// Internal state (module‑private, per ADR‑002)
// ---------------------------------------------------------------------------

static INITIALIZED: AtomicBool = AtomicBool::new(false);
static SHM_BASE: AtomicPtr<u8> = AtomicPtr::new(core::ptr::null_mut());
static SHM_SIZE: AtomicU32 = AtomicU32::new(0);

/// Simple outbound frame buffer (single frame for the reference impl).
/// In a real module this would be a proper queue.
static TX_FRAME_VALID: AtomicBool = AtomicBool::new(false);

static TX_FRAME: Mutex<Option<VecuFrame>> = Mutex::new(None);
static LAST_INBOUND: Mutex<Option<VecuFrame>> = Mutex::new(None);

/// Bridge state: holds the loaded C libraries and callback context.
static BRIDGE: Mutex<Option<bridge::BridgeLoader>> = Mutex::new(None);

/// Heap‑allocated callback context kept alive for the duration of the bridge.
static BRIDGE_CTX: Mutex<Option<Box<context::VecuBaseContext>>> = Mutex::new(None);

/// Stored loader log function pointer (used by context trampolines).
static BRIDGE_LOG_FN: AtomicPtr<core::ffi::c_void> = AtomicPtr::new(core::ptr::null_mut());

/// Tick interval from the runtime context (used by context builder).
static TICK_INTERVAL_US: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(1000);

// ---------------------------------------------------------------------------
// Bridge mode helpers
// ---------------------------------------------------------------------------

/// Try to load the `BaseLayer` and Application libraries from environment
/// variables.  Returns `true` if bridge mode was activated.
#[allow(unsafe_code)]
fn try_activate_bridge(ctx: &VecuRuntimeContext) -> bool {
    let base_path = match std::env::var("VECU_BASE_LIB") {
        Ok(p) if !p.is_empty() => p,
        _ => return false,
    };
    let appl_path = match std::env::var("VECU_APPL_LIB") {
        Ok(p) if !p.is_empty() => p,
        _ => return false,
    };

    // Store the log function pointer for the trampoline.
    if let Some(f) = ctx.log_fn {
        BRIDGE_LOG_FN.store(f as *mut core::ffi::c_void, Ordering::Release);
    }
    TICK_INTERVAL_US.store(ctx.tick_interval_us, Ordering::Release);

    // Load libraries
    let loader = match unsafe {
        bridge::BridgeLoader::load(
            std::path::Path::new(&base_path),
            std::path::Path::new(&appl_path),
        )
    } {
        Ok(l) => l,
        Err(e) => {
            eprintln!("[vecu-appl] bridge load failed: {e}");
            return false;
        }
    };

    // Build callback context
    // For now shm_vars points to NULL — will be computed from SHM header
    // offset when vecu-shm integration is wired (P4+).
    let base_ctx = Box::new(context::VecuBaseContext::build(
        core::ptr::null_mut(), // shm_vars (TODO: compute from off_vars)
        0,                     // shm_vars_size
        ctx.tick_interval_us,
    ));

    // Call Base_Init and Appl_Init
    unsafe { loader.call_base_init(std::ptr::addr_of!(*base_ctx)) };
    loader.call_appl_init();

    // Store state
    *BRIDGE.lock().expect("lock") = Some(loader);
    *BRIDGE_CTX.lock().expect("lock") = Some(base_ctx);

    true
}

// ---------------------------------------------------------------------------
// ABI functions
// ---------------------------------------------------------------------------

/// # Safety
///
/// `ctx` must point to a valid [`VecuRuntimeContext`].
#[allow(unsafe_code)]
unsafe extern "C" fn appl_init(ctx: *const VecuRuntimeContext) -> i32 {
    if ctx.is_null() {
        return status::INVALID_ARGUMENT;
    }
    // SAFETY: caller guarantees ctx is valid.
    let ctx = unsafe { &*ctx };
    SHM_BASE.store(ctx.shm_base, Ordering::Release);
    SHM_SIZE.store(ctx.shm_size, Ordering::Release);
    INITIALIZED.store(true, Ordering::Release);
    *TX_FRAME.lock().expect("lock") = None;
    *LAST_INBOUND.lock().expect("lock") = None;
    TX_FRAME_VALID.store(false, Ordering::Release);

    // Try bridge mode; if env vars not set, fall back to echo mode.
    try_activate_bridge(ctx);

    status::OK
}

extern "C" fn appl_shutdown() {
    // Shutdown bridge if active
    let bridge = BRIDGE.lock().expect("lock").take();
    if let Some(b) = bridge {
        b.call_appl_shutdown();
        b.call_base_shutdown();
        // Libraries dropped here
    }
    *BRIDGE_CTX.lock().expect("lock") = None;
    BRIDGE_LOG_FN.store(core::ptr::null_mut(), Ordering::Release);

    INITIALIZED.store(false, Ordering::Release);
    SHM_BASE.store(core::ptr::null_mut(), Ordering::Release);
    SHM_SIZE.store(0, Ordering::Release);
}

/// # Safety
///
/// Must only be called after a successful `appl_init`.
#[allow(unsafe_code)]
unsafe extern "C" fn appl_step(tick: u64) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }

    let guard = BRIDGE.lock().expect("lock");
    if let Some(bridge) = &*guard {
        // Bridge mode: delegate to C code
        bridge.call_base_step(tick);
        bridge.call_appl_main();
    } else {
        // Echo mode: echo the last inbound frame back as outbound
        let inbound_guard = LAST_INBOUND.lock().expect("lock");
        if let Some(inbound) = &*inbound_guard {
            let mut out = *inbound;
            out.timestamp = tick;
            *TX_FRAME.lock().expect("lock") = Some(out);
            TX_FRAME_VALID.store(true, Ordering::Release);
        }
        drop(inbound_guard);
    }
    drop(guard);

    status::OK
}

/// # Safety
///
/// `frame` must point to a valid [`VecuFrame`].
#[allow(unsafe_code)]
unsafe extern "C" fn appl_push_frame(frame: *const VecuFrame) -> i32 {
    if frame.is_null() {
        return status::INVALID_ARGUMENT;
    }
    // SAFETY: caller guarantees frame is valid.
    let frame = unsafe { &*frame };
    *LAST_INBOUND.lock().expect("lock") = Some(*frame);
    status::OK
}

/// # Safety
///
/// `frame` must point to a writeable [`VecuFrame`].
#[allow(unsafe_code)]
unsafe extern "C" fn appl_poll_frame(frame: *mut VecuFrame) -> i32 {
    if frame.is_null() {
        return status::INVALID_ARGUMENT;
    }
    if !TX_FRAME_VALID.load(Ordering::Acquire) {
        return status::NOT_SUPPORTED;
    }
    let guard = TX_FRAME.lock().expect("lock");
    if let Some(out) = &*guard {
        // SAFETY: caller guarantees frame is writeable.
        unsafe { *frame = *out };
        drop(guard);
        TX_FRAME_VALID.store(false, Ordering::Release);
        *TX_FRAME.lock().expect("lock") = None;
        status::OK
    } else {
        drop(guard);
        status::NOT_SUPPORTED
    }
}

// ---------------------------------------------------------------------------
// Single entry point (ADR‑001)
// ---------------------------------------------------------------------------

/// Populate the plugin API table.
///
/// # Safety
///
/// `out_api` must point to a valid, writeable [`VecuPluginApi`].
#[allow(unsafe_code)]
#[no_mangle]
pub unsafe extern "C" fn vecu_get_api(requested_version: u32, out_api: *mut VecuPluginApi) -> i32 {
    if out_api.is_null() {
        return status::INVALID_ARGUMENT;
    }

    let (req_major, _) = vecu_abi::unpack_version(requested_version);
    let (our_major, _) = vecu_abi::unpack_version(ABI_VERSION);
    if req_major != our_major {
        return status::VERSION_MISMATCH;
    }

    // SAFETY: caller guarantees out_api is valid and writeable.
    let api = unsafe { &mut *out_api };
    api.abi_version = ABI_VERSION;
    api.module_kind = ModuleKind::Appl as u32;
    api.capabilities = CAP_FRAME_IO;
    api.reserved = 0;
    api.init = Some(appl_init);
    api.shutdown = Some(appl_shutdown);
    api.step = Some(appl_step);
    api.push_frame = Some(appl_push_frame);
    api.poll_frame = Some(appl_poll_frame);
    api.seed = None; // APPL does not provide HSM functions
    api.key = None;
    api.sign = None;
    api.verify = None;
    api.encrypt = None;
    api.decrypt = None;
    api.generate_mac = None;
    api.verify_mac = None;
    api.load_key = None;
    api.rng = None;

    status::OK
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    /// Serialize tests that mutate module‑level statics.
    static TEST_LOCK: Mutex<()> = Mutex::new(());

    fn reset() {
        INITIALIZED.store(false, Ordering::SeqCst);
        SHM_BASE.store(core::ptr::null_mut(), Ordering::SeqCst);
        SHM_SIZE.store(0, Ordering::SeqCst);
        TX_FRAME_VALID.store(false, Ordering::SeqCst);
        *TX_FRAME.lock().unwrap() = None;
        *LAST_INBOUND.lock().unwrap() = None;
    }

    fn make_ctx() -> VecuRuntimeContext {
        VecuRuntimeContext {
            shm_base: core::ptr::null_mut(),
            shm_size: 0,
            pad0: 0,
            tick_interval_us: 1000,
            log_fn: None,
        }
    }

    #[test]
    fn get_api_fills_table() {
        let _g = TEST_LOCK.lock().unwrap();
        let mut api = VecuPluginApi::zeroed();
        #[allow(unsafe_code)]
        let rc = unsafe { vecu_get_api(ABI_VERSION, &mut api) };
        assert_eq!(rc, status::OK);
        assert_eq!(api.abi_version, ABI_VERSION);
        assert_eq!(api.module_kind, ModuleKind::Appl as u32);
        assert_eq!(api.capabilities, CAP_FRAME_IO);
        assert!(api.init.is_some());
        assert!(api.shutdown.is_some());
        assert!(api.step.is_some());
        assert!(api.push_frame.is_some());
        assert!(api.poll_frame.is_some());
    }

    #[test]
    fn get_api_rejects_version_mismatch() {
        let _g = TEST_LOCK.lock().unwrap();
        let mut api = VecuPluginApi::zeroed();
        let bad_version = vecu_abi::pack_version(99, 0);
        #[allow(unsafe_code)]
        let rc = unsafe { vecu_get_api(bad_version, &mut api) };
        assert_eq!(rc, status::VERSION_MISMATCH);
    }

    #[test]
    fn get_api_rejects_null() {
        let _g = TEST_LOCK.lock().unwrap();
        #[allow(unsafe_code)]
        let rc = unsafe { vecu_get_api(ABI_VERSION, core::ptr::null_mut()) };
        assert_eq!(rc, status::INVALID_ARGUMENT);
    }

    #[test]
    fn init_and_step_succeed() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        {
            let rc = unsafe { appl_init(&ctx) };
            assert_eq!(rc, status::OK);
            let rc = unsafe { appl_step(0) };
            assert_eq!(rc, status::OK);
        }
    }

    #[test]
    fn push_frame_and_poll_frame_echo() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            appl_init(&ctx);
            let frame = VecuFrame::with_data(0x100, &[1, 2, 3], 0);
            let rc = appl_push_frame(&frame);
            assert_eq!(rc, status::OK);

            // step produces outbound echo
            appl_step(42);

            let mut out = VecuFrame::new(0);
            let rc = appl_poll_frame(&mut out);
            assert_eq!(rc, status::OK);
            assert_eq!(out.id, 0x100);
            assert_eq!(out.timestamp, 42);
            assert_eq!(out.payload(), &[1, 2, 3]);
        }
    }

    #[test]
    fn poll_frame_returns_not_supported_when_empty() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            appl_init(&ctx);
            let mut out = VecuFrame::new(0);
            let rc = appl_poll_frame(&mut out);
            assert_eq!(rc, status::NOT_SUPPORTED);
        }
    }

    #[test]
    fn step_before_init_fails() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        #[allow(unsafe_code)]
        let rc = unsafe { appl_step(0) };
        assert_eq!(rc, status::INIT_FAILED);
    }

    #[test]
    fn shutdown_resets_state() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            appl_init(&ctx);
        }
        assert!(INITIALIZED.load(Ordering::SeqCst));
        appl_shutdown();
        assert!(!INITIALIZED.load(Ordering::SeqCst));
    }
}
