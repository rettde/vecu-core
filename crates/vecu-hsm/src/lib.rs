//! Reference HSM module for the vECU execution system (ADR‑002).
//!
//! Exports the single symbol `vecu_get_api` (ADR‑001).
//! Simulates basic security operations: processes diagnostic mailbox
//! requests (seed/key challenge‑response) via shared memory.
//! Compiled as `cdylib` for dynamic loading and `rlib` for tests.

use std::sync::atomic::{AtomicBool, AtomicPtr, AtomicU32, Ordering};

use vecu_abi::{
    status, DiagMailbox, ModuleKind, VecuPluginApi, VecuRuntimeContext, VecuShmHeader, ABI_VERSION,
    CAP_DIAGNOSTICS, CAP_HSM_SEED_KEY, HSM_BUF_SIZE,
};

// ---------------------------------------------------------------------------
// Well‑known constants
// ---------------------------------------------------------------------------

/// Diagnostic request type: seed/key challenge.
pub const DIAG_REQ_SEED_KEY: u32 = 0x27_01;

// ---------------------------------------------------------------------------
// Internal state (module‑private, per ADR‑002)
// ---------------------------------------------------------------------------

static INITIALIZED: AtomicBool = AtomicBool::new(false);
static SHM_BASE: AtomicPtr<u8> = AtomicPtr::new(core::ptr::null_mut());
static SHM_SIZE: AtomicU32 = AtomicU32::new(0);
static STEP_COUNT: AtomicU32 = AtomicU32::new(0);

/// Fixed reference seed (deterministic, for test / demo).
const REFERENCE_SEED: [u8; 4] = [0xDE, 0xAD, 0xBE, 0xEF];

use std::sync::Mutex;
/// Last seed issued (needed for `key` validation).
static LAST_SEED: Mutex<Option<[u8; 4]>> = Mutex::new(None);

// ---------------------------------------------------------------------------
// ABI functions
// ---------------------------------------------------------------------------

/// # Safety
///
/// `ctx` must point to a valid [`VecuRuntimeContext`].
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_init(ctx: *const VecuRuntimeContext) -> i32 {
    if ctx.is_null() {
        return status::INVALID_ARGUMENT;
    }
    // SAFETY: caller guarantees ctx is valid.
    let ctx = unsafe { &*ctx };
    SHM_BASE.store(ctx.shm_base, Ordering::Release);
    SHM_SIZE.store(ctx.shm_size, Ordering::Release);
    STEP_COUNT.store(0, Ordering::Release);
    INITIALIZED.store(true, Ordering::Release);
    status::OK
}

extern "C" fn hsm_shutdown() {
    INITIALIZED.store(false, Ordering::Release);
    SHM_BASE.store(core::ptr::null_mut(), Ordering::Release);
    SHM_SIZE.store(0, Ordering::Release);
    STEP_COUNT.store(0, Ordering::Release);
}

/// # Safety
///
/// Must only be called after a successful `hsm_init`.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_step(_tick: u64) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }

    STEP_COUNT.fetch_add(1, Ordering::Relaxed);

    // Check diagnostic mailbox in SHM if available.
    let base = SHM_BASE.load(Ordering::Acquire);
    let size = SHM_SIZE.load(Ordering::Acquire);
    if !base.is_null() && size as usize >= core::mem::size_of::<VecuShmHeader>() {
        process_diag_mailbox(base, size as usize);
    }

    status::OK
}

// ---------------------------------------------------------------------------
// HSM Security API – seed / key (ADR‑002 §427)
// ---------------------------------------------------------------------------

/// Generate a seed for `SecurityAccess` challenge.
///
/// # Safety
///
/// `out_seed` must point to a writeable buffer of at least [`HSM_BUF_SIZE`] bytes.
/// `out_len` must point to a writeable `u32`.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn hsm_seed(out_seed: *mut u8, out_len: *mut u32) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if out_seed.is_null() || out_len.is_null() {
        return status::INVALID_ARGUMENT;
    }

    // Store the seed for later key validation.
    *LAST_SEED.lock().unwrap() = Some(REFERENCE_SEED);

    // SAFETY: caller guarantees buffer is large enough.
    unsafe {
        core::ptr::copy_nonoverlapping(REFERENCE_SEED.as_ptr(), out_seed, REFERENCE_SEED.len());
        *out_len = REFERENCE_SEED.len() as u32;
    }
    status::OK
}

/// Validate a key response against the last generated seed.
///
/// The reference implementation expects `key = seed XOR 0xFF`.
///
/// # Safety
///
/// `key_buf` must point to `key_len` readable bytes.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_key(key_buf: *const u8, key_len: u32) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if key_buf.is_null() {
        return status::INVALID_ARGUMENT;
    }

    let guard = LAST_SEED.lock().unwrap();
    let Some(seed) = *guard else {
        return status::MODULE_ERROR; // no seed was generated
    };

    if key_len as usize != seed.len() {
        return status::MODULE_ERROR;
    }

    // SAFETY: caller guarantees buffer is valid.
    let key_slice = unsafe { core::slice::from_raw_parts(key_buf, key_len as usize) };

    // Reference validation: key[i] == seed[i] ^ 0xFF
    for (k, s) in key_slice.iter().zip(seed.iter()) {
        if *k != (*s ^ 0xFF) {
            return status::MODULE_ERROR;
        }
    }

    status::OK
}

// ---------------------------------------------------------------------------
// HSM Crypto API – sign / verify stubs (ADR‑002 §437, optional)
// ---------------------------------------------------------------------------

/// Sign data (stub – returns `NOT_SUPPORTED`).
///
/// # Safety
///
/// See [`VecuPluginApi::sign`] contract.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_sign(
    _data: *const u8,
    _data_len: u32,
    _out_sig: *mut u8,
    _out_sig_len: *mut u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    // Stub: not yet implemented.
    status::NOT_SUPPORTED
}

/// Verify signature (stub – returns `NOT_SUPPORTED`).
///
/// # Safety
///
/// See [`VecuPluginApi::verify`] contract.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_verify(
    _data: *const u8,
    _data_len: u32,
    _sig: *const u8,
    _sig_len: u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    // Stub: not yet implemented.
    status::NOT_SUPPORTED
}

// ---------------------------------------------------------------------------
// Diagnostic mailbox processing (ADR‑003 fallback path)
// ---------------------------------------------------------------------------

/// Process a pending diagnostic mailbox request.
///
/// # Safety
///
/// Called only from `hsm_step` which runs after init ensured a valid SHM region.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
fn process_diag_mailbox(base: *mut u8, size: usize) {
    let hdr_size = core::mem::size_of::<VecuShmHeader>();
    if size < hdr_size {
        return;
    }

    // SAFETY: base is a valid pointer to the SHM region, guaranteed by init.
    // We read via a byte slice + bytemuck to avoid alignment issues.
    #[allow(unsafe_code)]
    let hdr_bytes: &[u8] = unsafe { core::slice::from_raw_parts(base, hdr_size) };
    let hdr: &VecuShmHeader = bytemuck::from_bytes(hdr_bytes);

    let mb_off = hdr.off_diag_mb as usize;
    let mb_size = hdr.size_diag_mb as usize;
    let diag_struct_size = core::mem::size_of::<DiagMailbox>();
    if mb_size < diag_struct_size {
        return;
    }
    if mb_off + mb_size > size {
        return;
    }

    // SAFETY: offset and size validated above.
    #[allow(unsafe_code)]
    let mb_bytes: &mut [u8] =
        unsafe { core::slice::from_raw_parts_mut(base.add(mb_off), diag_struct_size) };
    let mb: &mut DiagMailbox = bytemuck::from_bytes_mut(mb_bytes);

    if mb.request_pending != 0 && mb.response_ready == 0 {
        if mb.request_type == DIAG_REQ_SEED_KEY {
            // Delegate to the explicit seed function.
            let mut seed_buf = [0u8; HSM_BUF_SIZE];
            let mut seed_len = 0u32;
            #[allow(unsafe_code)]
            let rc = unsafe { hsm_seed(seed_buf.as_mut_ptr(), &mut seed_len) };
            if rc == status::OK {
                let len = seed_len as usize;
                mb.data[..len].copy_from_slice(&seed_buf[..len]);
                mb.response_status = 0;
            } else {
                mb.response_status = 1;
            }
        } else {
            mb.response_status = 1; // unknown request
        }
        mb.response_ready = 1;
        mb.request_pending = 0;
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
    api.module_kind = ModuleKind::Hsm as u32;
    api.capabilities = CAP_DIAGNOSTICS | CAP_HSM_SEED_KEY;
    api.reserved = 0;
    api.init = Some(hsm_init);
    api.shutdown = Some(hsm_shutdown);
    api.step = Some(hsm_step);
    api.push_frame = None; // HSM does not do frame I/O
    api.poll_frame = None;
    api.seed = Some(hsm_seed);
    api.key = Some(hsm_key);
    api.sign = Some(hsm_sign);
    api.verify = Some(hsm_verify);

    status::OK
}

/// Return the internal step counter (for testing / observability).
pub fn step_count() -> u32 {
    STEP_COUNT.load(Ordering::Relaxed)
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::cast_possible_truncation)]
mod tests {
    use super::*;
    use std::sync::Mutex as TestMutex;
    use vecu_shm::SharedMemory;

    /// Expected key derived from `REFERENCE_SEED` (XOR 0xFF).
    const REFERENCE_KEY: [u8; 4] = [0x21, 0x52, 0x41, 0x10];

    static TEST_LOCK: TestMutex<()> = TestMutex::new(());

    fn reset() {
        INITIALIZED.store(false, Ordering::SeqCst);
        SHM_BASE.store(core::ptr::null_mut(), Ordering::SeqCst);
        SHM_SIZE.store(0, Ordering::SeqCst);
        STEP_COUNT.store(0, Ordering::SeqCst);
        *LAST_SEED.lock().unwrap() = None;
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
        assert_eq!(api.module_kind, ModuleKind::Hsm as u32);
        assert_eq!(api.capabilities, CAP_DIAGNOSTICS | CAP_HSM_SEED_KEY);
        assert!(api.init.is_some());
        assert!(api.shutdown.is_some());
        assert!(api.step.is_some());
        assert!(api.push_frame.is_none());
        assert!(api.poll_frame.is_none());
        assert!(api.seed.is_some());
        assert!(api.key.is_some());
        assert!(api.sign.is_some());
        assert!(api.verify.is_some());
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
            let rc = unsafe { hsm_init(&ctx) };
            assert_eq!(rc, status::OK);
            let rc = unsafe { hsm_step(0) };
            assert_eq!(rc, status::OK);
        }
        assert_eq!(step_count(), 1);
    }

    #[test]
    fn step_before_init_fails() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_step(0) };
        assert_eq!(rc, status::INIT_FAILED);
    }

    #[test]
    fn shutdown_resets_state() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }
        assert!(INITIALIZED.load(Ordering::SeqCst));
        hsm_shutdown();
        assert!(!INITIALIZED.load(Ordering::SeqCst));
    }

    #[test]
    fn diag_mailbox_seed_key() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        // Create an SHM-like buffer with header + diag mailbox.
        let mut shm = SharedMemory::anonymous();
        let (base, size) = shm.raw_parts();

        let ctx = VecuRuntimeContext {
            shm_base: base,
            shm_size: size,
            pad0: 0,
            tick_interval_us: 1000,
            log_fn: None,
        };

        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }

        // Write a seed/key request into the mailbox.
        {
            let mb = shm.diag_mailbox_mut();
            mb.request_pending = 1;
            mb.request_type = DIAG_REQ_SEED_KEY;
        }

        #[allow(unsafe_code)]
        unsafe {
            hsm_step(1);
        }

        // Check response.
        let mb = shm.diag_mailbox();
        assert_eq!(mb.request_pending, 0);
        assert_eq!(mb.response_ready, 1);
        assert_eq!(mb.response_status, 0);
        assert_eq!(&mb.data[..4], &[0xDE, 0xAD, 0xBE, 0xEF]);
    }

    #[test]
    fn seed_generates_reference_seed() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }

        let mut buf = [0u8; HSM_BUF_SIZE];
        let mut len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_seed(buf.as_mut_ptr(), &mut len) };
        assert_eq!(rc, status::OK);
        assert_eq!(len, 4);
        assert_eq!(&buf[..4], &REFERENCE_SEED);
    }

    #[test]
    fn key_validates_correct_response() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }

        // First generate a seed.
        let mut seed_buf = [0u8; HSM_BUF_SIZE];
        let mut seed_len = 0u32;
        #[allow(unsafe_code)]
        unsafe {
            hsm_seed(seed_buf.as_mut_ptr(), &mut seed_len);
        }

        // Now validate the correct key (seed XOR 0xFF).
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(REFERENCE_KEY.as_ptr(), REFERENCE_KEY.len() as u32) };
        assert_eq!(rc, status::OK);
    }

    #[test]
    fn key_rejects_wrong_response() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }

        let mut seed_buf = [0u8; HSM_BUF_SIZE];
        let mut seed_len = 0u32;
        #[allow(unsafe_code)]
        unsafe {
            hsm_seed(seed_buf.as_mut_ptr(), &mut seed_len);
        }

        let bad_key = [0x00, 0x00, 0x00, 0x00];
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(bad_key.as_ptr(), bad_key.len() as u32) };
        assert_eq!(rc, status::MODULE_ERROR);
    }

    #[test]
    fn key_without_seed_fails() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }

        // No seed generated, key should fail.
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(REFERENCE_KEY.as_ptr(), REFERENCE_KEY.len() as u32) };
        assert_eq!(rc, status::MODULE_ERROR);
    }

    #[test]
    fn seed_before_init_fails() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let mut buf = [0u8; HSM_BUF_SIZE];
        let mut len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_seed(buf.as_mut_ptr(), &mut len) };
        assert_eq!(rc, status::INIT_FAILED);
    }

    #[test]
    fn sign_returns_not_supported() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }
        let data = [0u8; 16];
        let mut sig = [0u8; HSM_BUF_SIZE];
        let mut sig_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_sign(
                data.as_ptr(),
                data.len() as u32,
                sig.as_mut_ptr(),
                &mut sig_len,
            )
        };
        assert_eq!(rc, status::NOT_SUPPORTED);
    }

    #[test]
    fn verify_returns_not_supported() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }
        let data = [0u8; 16];
        let sig = [0u8; 32];
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_verify(
                data.as_ptr(),
                data.len() as u32,
                sig.as_ptr(),
                sig.len() as u32,
            )
        };
        assert_eq!(rc, status::NOT_SUPPORTED);
    }
}
