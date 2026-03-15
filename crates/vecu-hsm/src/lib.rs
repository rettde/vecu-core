//! SHE‑compatible HSM module for the vECU execution system (ADR‑002).
//!
//! Exports the single symbol `vecu_get_api` (ADR‑001).
//! Implements a software emulation of the AUTOSAR SHE (Secure Hardware
//! Extension) specification:
//!
//! - **Key store**: 20 AES‑128 key slots
//! - **AES‑128 ECB/CBC** encrypt / decrypt
//! - **AES‑128‑CMAC** generate / verify (sign / verify)
//! - **`SecurityAccess`**: CMAC‑based seed/key challenge‑response
//! - **CSPRNG**: cryptographically secure random number generation
//! - **Diagnostic mailbox** processing via shared memory
//!
//! Compiled as `cdylib` for dynamic loading and `rlib` for tests.

use std::sync::atomic::{AtomicBool, AtomicPtr, AtomicU32, Ordering};
use std::sync::Mutex;

use aes::cipher::{BlockDecrypt, BlockEncrypt, KeyInit};
use aes::Aes128;
use cmac::Mac;
use rand::RngCore;
use zeroize::Zeroize;

use vecu_abi::{
    status, DiagMailbox, ModuleKind, VecuPluginApi, VecuRuntimeContext, VecuShmHeader,
    ABI_VERSION, AES128_BLOCK_SIZE, AES128_CMAC_SIZE, AES128_KEY_SIZE, CAP_DIAGNOSTICS,
    CAP_HSM_ENCRYPT, CAP_HSM_RNG, CAP_HSM_SEED_KEY, CAP_SIGN_VERIFY, HSM_BUF_SIZE,
    SHE_MODE_CBC, SHE_MODE_ECB, SHE_NUM_KEY_SLOTS,
};

// ---------------------------------------------------------------------------
// Well‑known constants
// ---------------------------------------------------------------------------

/// Diagnostic request type: seed/key challenge.
pub const DIAG_REQ_SEED_KEY: u32 = 0x27_01;

/// Default master key (slot 0) — used for `SecurityAccess` CMAC.
/// In production this would be provisioned securely.
const DEFAULT_MASTER_KEY: [u8; AES128_KEY_SIZE] = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F,
];

/// Key slot index used for `SecurityAccess` (seed/key).
const SECURITY_ACCESS_KEY_SLOT: usize = 0;

/// Seed length for `SecurityAccess` (16 bytes = one AES block).
const SEED_LEN: usize = AES128_KEY_SIZE;

// Compile‑time constraint checks.
const _: () = assert!(
    SECURITY_ACCESS_KEY_SLOT < SHE_NUM_KEY_SLOTS,
    "master key slot must be within key store bounds"
);
const _: () = assert!(
    SEED_LEN <= vecu_abi::DIAG_DATA_SIZE,
    "seed must fit in diagnostic mailbox data area"
);
const _: () = assert!(
    SEED_LEN <= HSM_BUF_SIZE,
    "seed must fit in HSM buffer"
);
const _: () = assert!(
    AES128_CMAC_SIZE <= HSM_BUF_SIZE,
    "CMAC tag must fit in HSM buffer"
);

// ---------------------------------------------------------------------------
// Key store
// ---------------------------------------------------------------------------

/// SHE‑compatible key store: 20 slots of AES‑128 keys.
struct KeyStore {
    /// Key data. `None` = slot empty / not provisioned.
    slots: [Option<[u8; AES128_KEY_SIZE]>; SHE_NUM_KEY_SLOTS],
}

impl KeyStore {
    const fn new() -> Self {
        Self {
            slots: [None; SHE_NUM_KEY_SLOTS],
        }
    }

    fn get(&self, slot: u32) -> Option<&[u8; AES128_KEY_SIZE]> {
        self.slots.get(slot as usize)?.as_ref()
    }

    fn load(&mut self, slot: u32, key: [u8; AES128_KEY_SIZE]) -> bool {
        if let Some(s) = self.slots.get_mut(slot as usize) {
            *s = Some(key);
            true
        } else {
            false
        }
    }

    fn reset(&mut self) {
        for slot in &mut self.slots {
            if let Some(k) = slot.as_mut() {
                k.zeroize();
            }
            *slot = None;
        }
    }
}

// ---------------------------------------------------------------------------
// Internal state (module‑private, per ADR‑002)
// ---------------------------------------------------------------------------

static INITIALIZED: AtomicBool = AtomicBool::new(false);
static SHM_BASE: AtomicPtr<u8> = AtomicPtr::new(core::ptr::null_mut());
static SHM_SIZE: AtomicU32 = AtomicU32::new(0);
static STEP_COUNT: AtomicU32 = AtomicU32::new(0);

static KEY_STORE: Mutex<KeyStore> = Mutex::new(KeyStore::new());
static LAST_SEED: Mutex<Option<[u8; SEED_LEN]>> = Mutex::new(None);

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// AES‑128 ECB encrypt: `data` must be a multiple of 16 bytes.
fn aes_ecb_encrypt(key: &[u8; AES128_KEY_SIZE], data: &[u8], out: &mut [u8]) -> bool {
    if data.is_empty() || data.len() % AES128_BLOCK_SIZE != 0 || out.len() < data.len() {
        return false;
    }
    let cipher = Aes128::new(key.into());
    out[..data.len()].copy_from_slice(data);
    for chunk in out[..data.len()].chunks_exact_mut(AES128_BLOCK_SIZE) {
        cipher.encrypt_block(chunk.into());
    }
    true
}

/// AES‑128 ECB decrypt: `data` must be a multiple of 16 bytes.
fn aes_ecb_decrypt(key: &[u8; AES128_KEY_SIZE], data: &[u8], out: &mut [u8]) -> bool {
    if data.is_empty() || data.len() % AES128_BLOCK_SIZE != 0 || out.len() < data.len() {
        return false;
    }
    let cipher = Aes128::new(key.into());
    out[..data.len()].copy_from_slice(data);
    for chunk in out[..data.len()].chunks_exact_mut(AES128_BLOCK_SIZE) {
        cipher.decrypt_block(chunk.into());
    }
    true
}

/// AES‑128 CBC encrypt: `data` must be a multiple of 16 bytes, `iv` is 16 bytes.
fn aes_cbc_encrypt(
    key: &[u8; AES128_KEY_SIZE],
    iv: &[u8; AES128_BLOCK_SIZE],
    data: &[u8],
    out: &mut [u8],
) -> bool {
    if data.is_empty() || data.len() % AES128_BLOCK_SIZE != 0 || out.len() < data.len() {
        return false;
    }
    let cipher = Aes128::new(key.into());
    let mut prev = *iv;
    for (i, chunk) in data.chunks_exact(AES128_BLOCK_SIZE).enumerate() {
        let offset = i * AES128_BLOCK_SIZE;
        // XOR plaintext with previous ciphertext (or IV)
        let mut block = [0u8; AES128_BLOCK_SIZE];
        for j in 0..AES128_BLOCK_SIZE {
            block[j] = chunk[j] ^ prev[j];
        }
        let block_ref: &mut aes::cipher::generic_array::GenericArray<u8, _> = (&mut block).into();
        cipher.encrypt_block(block_ref);
        out[offset..offset + AES128_BLOCK_SIZE].copy_from_slice(&block);
        prev = block;
    }
    true
}

/// AES‑128 CBC decrypt: `data` must be a multiple of 16 bytes, `iv` is 16 bytes.
fn aes_cbc_decrypt(
    key: &[u8; AES128_KEY_SIZE],
    iv: &[u8; AES128_BLOCK_SIZE],
    data: &[u8],
    out: &mut [u8],
) -> bool {
    if data.is_empty() || data.len() % AES128_BLOCK_SIZE != 0 || out.len() < data.len() {
        return false;
    }
    let cipher = Aes128::new(key.into());
    let mut prev = *iv;
    for (i, chunk) in data.chunks_exact(AES128_BLOCK_SIZE).enumerate() {
        let offset = i * AES128_BLOCK_SIZE;
        let mut block = [0u8; AES128_BLOCK_SIZE];
        block.copy_from_slice(chunk);
        let block_ref: &mut aes::cipher::generic_array::GenericArray<u8, _> = (&mut block).into();
        cipher.decrypt_block(block_ref);
        // XOR with previous ciphertext (or IV)
        for j in 0..AES128_BLOCK_SIZE {
            out[offset + j] = block[j] ^ prev[j];
        }
        prev.copy_from_slice(chunk);
    }
    true
}

/// Compute AES‑128‑CMAC over `data` with the given key.
fn aes_cmac(key: &[u8; AES128_KEY_SIZE], data: &[u8]) -> [u8; AES128_CMAC_SIZE] {
    let mut mac =
        <cmac::Cmac<Aes128> as Mac>::new_from_slice(key).expect("AES-128 key size is always valid");
    mac.update(data);
    let result = mac.finalize();
    let mut tag = [0u8; AES128_CMAC_SIZE];
    tag.copy_from_slice(&result.into_bytes());
    tag
}

/// Verify AES‑128‑CMAC: returns `true` if the MAC matches.
fn aes_cmac_verify(key: &[u8; AES128_KEY_SIZE], data: &[u8], expected: &[u8]) -> bool {
    if expected.len() != AES128_CMAC_SIZE {
        return false;
    }
    let computed = aes_cmac(key, data);
    // Constant‑time comparison.
    let mut diff = 0u8;
    for (a, b) in computed.iter().zip(expected.iter()) {
        diff |= a ^ b;
    }
    diff == 0
}

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

    // Initialize key store with default master key.
    {
        let mut ks = KEY_STORE.lock().expect("key store lock");
        ks.reset();
        #[allow(clippy::cast_possible_truncation)]
        ks.load(SECURITY_ACCESS_KEY_SLOT as u32, DEFAULT_MASTER_KEY);
    }
    *LAST_SEED.lock().expect("seed lock") = None;

    INITIALIZED.store(true, Ordering::Release);
    status::OK
}

extern "C" fn hsm_shutdown() {
    INITIALIZED.store(false, Ordering::Release);
    SHM_BASE.store(core::ptr::null_mut(), Ordering::Release);
    SHM_SIZE.store(0, Ordering::Release);
    STEP_COUNT.store(0, Ordering::Release);
    KEY_STORE.lock().expect("key store lock").reset();
    *LAST_SEED.lock().expect("seed lock") = None;
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
// SecurityAccess – CMAC‑based seed / key
// ---------------------------------------------------------------------------

/// Generate a random seed for `SecurityAccess` challenge.
///
/// The seed is 16 bytes (one AES block) from the CSPRNG.
/// The expected key response is `CMAC(master_key, seed)`.
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

    // Generate random seed.
    let mut seed = [0u8; SEED_LEN];
    rand::rngs::OsRng.fill_bytes(&mut seed);

    // Store for later key validation.
    *LAST_SEED.lock().expect("seed lock") = Some(seed);

    // SAFETY: caller guarantees buffer is large enough.
    unsafe {
        core::ptr::copy_nonoverlapping(seed.as_ptr(), out_seed, SEED_LEN);
        *out_len = SEED_LEN as u32;
    }
    status::OK
}

/// Validate a key response: expected = `CMAC(master_key, seed)`.
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
    if key_len as usize != AES128_CMAC_SIZE {
        return status::MODULE_ERROR;
    }

    let mut seed_guard = LAST_SEED.lock().expect("seed lock");
    let Some(seed) = seed_guard.take() else {
        return status::MODULE_ERROR;
    };
    drop(seed_guard);

    let ks = KEY_STORE.lock().expect("key store lock");
    #[allow(clippy::cast_possible_truncation)]
    let Some(master_key) = ks.get(SECURITY_ACCESS_KEY_SLOT as u32) else {
        return status::MODULE_ERROR;
    };

    // SAFETY: caller guarantees buffer is valid.
    let key_slice = unsafe { core::slice::from_raw_parts(key_buf, key_len as usize) };
    if aes_cmac_verify(master_key, &seed, key_slice) {
        status::OK
    } else {
        status::MODULE_ERROR
    }
}

// ---------------------------------------------------------------------------
// Sign / Verify (AES‑128‑CMAC with master key)
// ---------------------------------------------------------------------------

/// Sign data with AES‑128‑CMAC using the master key (slot 0).
///
/// # Safety
///
/// See [`VecuPluginApi::sign`] contract.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn hsm_sign(
    data: *const u8,
    data_len: u32,
    out_sig: *mut u8,
    out_sig_len: *mut u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if data.is_null() || out_sig.is_null() || out_sig_len.is_null() {
        return status::INVALID_ARGUMENT;
    }

    let ks = KEY_STORE.lock().expect("key store lock");
    #[allow(clippy::cast_possible_truncation)]
    let Some(key) = ks.get(SECURITY_ACCESS_KEY_SLOT as u32) else {
        return status::MODULE_ERROR;
    };
    let key = *key;
    drop(ks);

    // SAFETY: caller guarantees buffers are valid.
    let data_slice = unsafe { core::slice::from_raw_parts(data, data_len as usize) };
    let tag = aes_cmac(&key, data_slice);
    unsafe {
        core::ptr::copy_nonoverlapping(tag.as_ptr(), out_sig, AES128_CMAC_SIZE);
        *out_sig_len = AES128_CMAC_SIZE as u32;
    }
    status::OK
}

/// Verify AES‑128‑CMAC signature using the master key (slot 0).
///
/// # Safety
///
/// See [`VecuPluginApi::verify`] contract.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_verify(
    data: *const u8,
    data_len: u32,
    sig: *const u8,
    sig_len: u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if data.is_null() || sig.is_null() {
        return status::INVALID_ARGUMENT;
    }

    let ks = KEY_STORE.lock().expect("key store lock");
    #[allow(clippy::cast_possible_truncation)]
    let Some(key) = ks.get(SECURITY_ACCESS_KEY_SLOT as u32) else {
        return status::MODULE_ERROR;
    };
    let key = *key;
    drop(ks);

    // SAFETY: caller guarantees buffers are valid.
    let data_slice = unsafe { core::slice::from_raw_parts(data, data_len as usize) };
    let sig_slice = unsafe { core::slice::from_raw_parts(sig, sig_len as usize) };
    if aes_cmac_verify(&key, data_slice, sig_slice) {
        status::OK
    } else {
        status::MODULE_ERROR
    }
}

// ---------------------------------------------------------------------------
// AES‑128 Encrypt / Decrypt (SHE commands)
// ---------------------------------------------------------------------------

/// AES‑128 encrypt (ECB or CBC).
///
/// # Safety
///
/// See [`VecuPluginApi::encrypt`] contract.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn hsm_encrypt(
    key_slot: u32,
    mode: u32,
    data: *const u8,
    data_len: u32,
    iv: *const u8,
    out: *mut u8,
    out_len: *mut u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if data.is_null() || out.is_null() || out_len.is_null() {
        return status::INVALID_ARGUMENT;
    }
    let len = data_len as usize;
    if len % AES128_BLOCK_SIZE != 0 || len == 0 {
        return status::INVALID_ARGUMENT;
    }

    let ks = KEY_STORE.lock().expect("key store lock");
    let Some(key) = ks.get(key_slot) else {
        return status::MODULE_ERROR;
    };
    let key = *key;
    drop(ks);

    // SAFETY: caller guarantees buffers are valid.
    let data_slice = unsafe { core::slice::from_raw_parts(data, len) };
    let out_slice = unsafe { core::slice::from_raw_parts_mut(out, len) };

    let ok = match mode {
        SHE_MODE_ECB => aes_ecb_encrypt(&key, data_slice, out_slice),
        SHE_MODE_CBC => {
            if iv.is_null() {
                return status::INVALID_ARGUMENT;
            }
            let iv_slice = unsafe { &*iv.cast::<[u8; AES128_BLOCK_SIZE]>() };
            aes_cbc_encrypt(&key, iv_slice, data_slice, out_slice)
        }
        _ => return status::INVALID_ARGUMENT,
    };

    if ok {
        unsafe { *out_len = data_len };
        status::OK
    } else {
        status::MODULE_ERROR
    }
}

/// AES‑128 decrypt (ECB or CBC).
///
/// # Safety
///
/// See [`VecuPluginApi::decrypt`] contract.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn hsm_decrypt(
    key_slot: u32,
    mode: u32,
    data: *const u8,
    data_len: u32,
    iv: *const u8,
    out: *mut u8,
    out_len: *mut u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if data.is_null() || out.is_null() || out_len.is_null() {
        return status::INVALID_ARGUMENT;
    }
    let len = data_len as usize;
    if len % AES128_BLOCK_SIZE != 0 || len == 0 {
        return status::INVALID_ARGUMENT;
    }

    let ks = KEY_STORE.lock().expect("key store lock");
    let Some(key) = ks.get(key_slot) else {
        return status::MODULE_ERROR;
    };
    let key = *key;
    drop(ks);

    // SAFETY: caller guarantees buffers are valid.
    let data_slice = unsafe { core::slice::from_raw_parts(data, len) };
    let out_slice = unsafe { core::slice::from_raw_parts_mut(out, len) };

    let ok = match mode {
        SHE_MODE_ECB => aes_ecb_decrypt(&key, data_slice, out_slice),
        SHE_MODE_CBC => {
            if iv.is_null() {
                return status::INVALID_ARGUMENT;
            }
            let iv_slice = unsafe { &*iv.cast::<[u8; AES128_BLOCK_SIZE]>() };
            aes_cbc_decrypt(&key, iv_slice, data_slice, out_slice)
        }
        _ => return status::INVALID_ARGUMENT,
    };

    if ok {
        unsafe { *out_len = data_len };
        status::OK
    } else {
        status::MODULE_ERROR
    }
}

// ---------------------------------------------------------------------------
// AES‑128‑CMAC generate / verify (with key slot)
// ---------------------------------------------------------------------------

/// Generate AES‑128‑CMAC with a specific key slot.
///
/// # Safety
///
/// See [`VecuPluginApi::generate_mac`] contract.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn hsm_generate_mac(
    key_slot: u32,
    data: *const u8,
    data_len: u32,
    out_mac: *mut u8,
    out_mac_len: *mut u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if data.is_null() || out_mac.is_null() || out_mac_len.is_null() {
        return status::INVALID_ARGUMENT;
    }

    let ks = KEY_STORE.lock().expect("key store lock");
    let Some(key) = ks.get(key_slot) else {
        return status::MODULE_ERROR;
    };
    let key = *key;
    drop(ks);

    let data_slice = unsafe { core::slice::from_raw_parts(data, data_len as usize) };
    let tag = aes_cmac(&key, data_slice);
    unsafe {
        core::ptr::copy_nonoverlapping(tag.as_ptr(), out_mac, AES128_CMAC_SIZE);
        *out_mac_len = AES128_CMAC_SIZE as u32;
    }
    status::OK
}

/// Verify AES‑128‑CMAC with a specific key slot.
///
/// # Safety
///
/// See [`VecuPluginApi::verify_mac`] contract.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_verify_mac(
    key_slot: u32,
    data: *const u8,
    data_len: u32,
    mac: *const u8,
    mac_len: u32,
) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if data.is_null() || mac.is_null() {
        return status::INVALID_ARGUMENT;
    }

    let ks = KEY_STORE.lock().expect("key store lock");
    let Some(key) = ks.get(key_slot) else {
        return status::MODULE_ERROR;
    };
    let key = *key;
    drop(ks);

    let data_slice = unsafe { core::slice::from_raw_parts(data, data_len as usize) };
    let mac_slice = unsafe { core::slice::from_raw_parts(mac, mac_len as usize) };
    if aes_cmac_verify(&key, data_slice, mac_slice) {
        status::OK
    } else {
        status::MODULE_ERROR
    }
}

// ---------------------------------------------------------------------------
// Key management
// ---------------------------------------------------------------------------

/// Load a plain AES‑128 key into a key slot.
///
/// # Safety
///
/// `key_data` must point to `key_len` readable bytes.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_load_key(slot: u32, key_data: *const u8, key_len: u32) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if key_data.is_null() {
        return status::INVALID_ARGUMENT;
    }
    if key_len as usize != AES128_KEY_SIZE {
        return status::INVALID_ARGUMENT;
    }
    // Slot 0 is the SecurityAccess master key — not user-writable.
    #[allow(clippy::cast_possible_truncation)]
    if slot == SECURITY_ACCESS_KEY_SLOT as u32 {
        return status::INVALID_ARGUMENT;
    }

    let key_slice = unsafe { core::slice::from_raw_parts(key_data, AES128_KEY_SIZE) };
    let mut key = [0u8; AES128_KEY_SIZE];
    key.copy_from_slice(key_slice);

    let mut ks = KEY_STORE.lock().expect("key store lock");
    if ks.load(slot, key) {
        status::OK
    } else {
        status::INVALID_ARGUMENT
    }
}

// ---------------------------------------------------------------------------
// Random number generation
// ---------------------------------------------------------------------------

/// Generate cryptographically secure random bytes.
///
/// # Safety
///
/// `out_buf` must point to a writeable buffer of at least `buf_len` bytes.
#[allow(unsafe_code)]
unsafe extern "C" fn hsm_rng(out_buf: *mut u8, buf_len: u32) -> i32 {
    if !INITIALIZED.load(Ordering::Acquire) {
        return status::INIT_FAILED;
    }
    if out_buf.is_null() || buf_len == 0 {
        return status::INVALID_ARGUMENT;
    }

    let out_slice = unsafe { core::slice::from_raw_parts_mut(out_buf, buf_len as usize) };
    rand::rngs::OsRng.fill_bytes(out_slice);
    status::OK
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

    // Read header values via a temporary shared reference, then drop it
    // before creating a mutable reference to the mailbox region.
    let (mb_off, mb_size) = {
        // SAFETY: base is a valid pointer to the SHM region, guaranteed by init.
        #[allow(unsafe_code)]
        let hdr_bytes: &[u8] = unsafe { core::slice::from_raw_parts(base, hdr_size) };
        let hdr: &VecuShmHeader = bytemuck::from_bytes(hdr_bytes);
        (hdr.off_diag_mb as usize, hdr.size_diag_mb as usize)
    }; // hdr_bytes and hdr are dropped here — no aliasing.

    let diag_struct_size = core::mem::size_of::<DiagMailbox>();
    if mb_size < diag_struct_size {
        return;
    }
    // Guard against overflow in offset + size arithmetic.
    let Some(mb_end) = mb_off.checked_add(mb_size) else {
        return;
    };
    if mb_end > size {
        return;
    }

    // SAFETY: offset and size validated above; no outstanding shared references.
    #[allow(unsafe_code)]
    let mb_bytes: &mut [u8] =
        unsafe { core::slice::from_raw_parts_mut(base.add(mb_off), diag_struct_size) };
    let mb: &mut DiagMailbox = bytemuck::from_bytes_mut(mb_bytes);

    if mb.request_pending != 0 && mb.response_ready == 0 {
        if mb.request_type == DIAG_REQ_SEED_KEY {
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
    api.capabilities =
        CAP_DIAGNOSTICS | CAP_HSM_SEED_KEY | CAP_SIGN_VERIFY | CAP_HSM_ENCRYPT | CAP_HSM_RNG;
    api.reserved = 0;
    api.init = Some(hsm_init);
    api.shutdown = Some(hsm_shutdown);
    api.step = Some(hsm_step);
    api.push_frame = None;
    api.poll_frame = None;
    api.seed = Some(hsm_seed);
    api.key = Some(hsm_key);
    api.sign = Some(hsm_sign);
    api.verify = Some(hsm_verify);
    api.encrypt = Some(hsm_encrypt);
    api.decrypt = Some(hsm_decrypt);
    api.generate_mac = Some(hsm_generate_mac);
    api.verify_mac = Some(hsm_verify_mac);
    api.load_key = Some(hsm_load_key);
    api.rng = Some(hsm_rng);

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

    static TEST_LOCK: TestMutex<()> = TestMutex::new(());

    fn reset() {
        INITIALIZED.store(false, Ordering::SeqCst);
        SHM_BASE.store(core::ptr::null_mut(), Ordering::SeqCst);
        SHM_SIZE.store(0, Ordering::SeqCst);
        STEP_COUNT.store(0, Ordering::SeqCst);
        KEY_STORE.lock().unwrap().reset();
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

    fn init_hsm() {
        let ctx = make_ctx();
        #[allow(unsafe_code)]
        unsafe {
            hsm_init(&ctx);
        }
    }

    // -- API table -----------------------------------------------------------

    #[test]
    fn get_api_fills_table() {
        let _g = TEST_LOCK.lock().unwrap();
        let mut api = VecuPluginApi::zeroed();
        #[allow(unsafe_code)]
        let rc = unsafe { vecu_get_api(ABI_VERSION, &mut api) };
        assert_eq!(rc, status::OK);
        assert_eq!(api.abi_version, ABI_VERSION);
        assert_eq!(api.module_kind, ModuleKind::Hsm as u32);
        assert!(api.capabilities & CAP_HSM_SEED_KEY != 0);
        assert!(api.capabilities & CAP_SIGN_VERIFY != 0);
        assert!(api.capabilities & CAP_HSM_ENCRYPT != 0);
        assert!(api.capabilities & CAP_HSM_RNG != 0);
        assert!(api.init.is_some());
        assert!(api.shutdown.is_some());
        assert!(api.step.is_some());
        assert!(api.push_frame.is_none());
        assert!(api.poll_frame.is_none());
        assert!(api.seed.is_some());
        assert!(api.key.is_some());
        assert!(api.sign.is_some());
        assert!(api.verify.is_some());
        assert!(api.encrypt.is_some());
        assert!(api.decrypt.is_some());
        assert!(api.generate_mac.is_some());
        assert!(api.verify_mac.is_some());
        assert!(api.load_key.is_some());
        assert!(api.rng.is_some());
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

    // -- Lifecycle -----------------------------------------------------------

    #[test]
    fn init_and_step_succeed() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_step(0) };
        assert_eq!(rc, status::OK);
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
        init_hsm();
        assert!(INITIALIZED.load(Ordering::SeqCst));
        hsm_shutdown();
        assert!(!INITIALIZED.load(Ordering::SeqCst));
    }

    // -- SecurityAccess (CMAC‑based) -----------------------------------------

    #[test]
    fn seed_key_cmac_round_trip() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        // Generate seed.
        let mut seed_buf = [0u8; HSM_BUF_SIZE];
        let mut seed_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_seed(seed_buf.as_mut_ptr(), &mut seed_len) };
        assert_eq!(rc, status::OK);
        assert_eq!(seed_len as usize, SEED_LEN);

        // Compute expected key = CMAC(master_key, seed).
        let seed = &seed_buf[..SEED_LEN];
        let expected_key = aes_cmac(&DEFAULT_MASTER_KEY, seed);

        // Validate key.
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(expected_key.as_ptr(), AES128_CMAC_SIZE as u32) };
        assert_eq!(rc, status::OK);
    }

    #[test]
    fn seed_key_rejects_wrong_key() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let mut seed_buf = [0u8; HSM_BUF_SIZE];
        let mut seed_len = 0u32;
        #[allow(unsafe_code)]
        unsafe {
            hsm_seed(seed_buf.as_mut_ptr(), &mut seed_len);
        }

        let bad_key = [0u8; AES128_CMAC_SIZE];
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(bad_key.as_ptr(), AES128_CMAC_SIZE as u32) };
        assert_eq!(rc, status::MODULE_ERROR);
    }

    #[test]
    fn key_without_seed_fails() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        let key = [0u8; AES128_CMAC_SIZE];
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(key.as_ptr(), AES128_CMAC_SIZE as u32) };
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

    // -- AES‑128 ECB ---------------------------------------------------------

    #[test]
    fn aes_ecb_encrypt_decrypt_round_trip() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let plaintext = [0x00u8; 32]; // two blocks
        let mut ciphertext = [0u8; 32];
        let mut ct_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_encrypt(
                0,
                SHE_MODE_ECB,
                plaintext.as_ptr(),
                32,
                core::ptr::null(),
                ciphertext.as_mut_ptr(),
                &mut ct_len,
            )
        };
        assert_eq!(rc, status::OK);
        assert_eq!(ct_len, 32);
        assert_ne!(&ciphertext[..], &plaintext[..]);

        let mut decrypted = [0u8; 32];
        let mut pt_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_decrypt(
                0,
                SHE_MODE_ECB,
                ciphertext.as_ptr(),
                32,
                core::ptr::null(),
                decrypted.as_mut_ptr(),
                &mut pt_len,
            )
        };
        assert_eq!(rc, status::OK);
        assert_eq!(&decrypted[..], &plaintext[..]);
    }

    #[test]
    fn aes_ecb_rejects_non_block_aligned() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        let data = [0u8; 15]; // not a multiple of 16
        let mut out = [0u8; 16];
        let mut out_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_encrypt(
                0,
                SHE_MODE_ECB,
                data.as_ptr(),
                15,
                core::ptr::null(),
                out.as_mut_ptr(),
                &mut out_len,
            )
        };
        assert_eq!(rc, status::INVALID_ARGUMENT);
    }

    // -- AES‑128 CBC ---------------------------------------------------------

    #[test]
    fn aes_cbc_encrypt_decrypt_round_trip() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let iv = [0x10u8; AES128_BLOCK_SIZE];
        let plaintext = [0xABu8; 48]; // three blocks
        let mut ciphertext = [0u8; 48];
        let mut ct_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_encrypt(
                0,
                SHE_MODE_CBC,
                plaintext.as_ptr(),
                48,
                iv.as_ptr(),
                ciphertext.as_mut_ptr(),
                &mut ct_len,
            )
        };
        assert_eq!(rc, status::OK);
        assert_eq!(ct_len, 48);

        let mut decrypted = [0u8; 48];
        let mut pt_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_decrypt(
                0,
                SHE_MODE_CBC,
                ciphertext.as_ptr(),
                48,
                iv.as_ptr(),
                decrypted.as_mut_ptr(),
                &mut pt_len,
            )
        };
        assert_eq!(rc, status::OK);
        assert_eq!(&decrypted[..], &plaintext[..]);
    }

    // -- AES‑128‑CMAC --------------------------------------------------------

    #[test]
    fn cmac_generate_verify_round_trip() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let data = b"Hello SHE CMAC!";
        let mut mac = [0u8; AES128_CMAC_SIZE];
        let mut mac_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_generate_mac(
                0,
                data.as_ptr(),
                data.len() as u32,
                mac.as_mut_ptr(),
                &mut mac_len,
            )
        };
        assert_eq!(rc, status::OK);
        assert_eq!(mac_len as usize, AES128_CMAC_SIZE);

        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_verify_mac(0, data.as_ptr(), data.len() as u32, mac.as_ptr(), mac_len)
        };
        assert_eq!(rc, status::OK);
    }

    #[test]
    fn cmac_verify_rejects_wrong_mac() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let data = b"test data";
        let bad_mac = [0u8; AES128_CMAC_SIZE];
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_verify_mac(
                0,
                data.as_ptr(),
                data.len() as u32,
                bad_mac.as_ptr(),
                AES128_CMAC_SIZE as u32,
            )
        };
        assert_eq!(rc, status::MODULE_ERROR);
    }

    // -- Sign / Verify (CMAC with master key) --------------------------------

    #[test]
    fn sign_verify_round_trip() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let data = b"sign this payload";
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
        assert_eq!(rc, status::OK);
        assert_eq!(sig_len as usize, AES128_CMAC_SIZE);

        #[allow(unsafe_code)]
        let rc =
            unsafe { hsm_verify(data.as_ptr(), data.len() as u32, sig.as_ptr(), sig_len) };
        assert_eq!(rc, status::OK);
    }

    #[test]
    fn verify_rejects_bad_signature() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let data = b"test";
        let bad_sig = [0u8; AES128_CMAC_SIZE];
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_verify(
                data.as_ptr(),
                data.len() as u32,
                bad_sig.as_ptr(),
                AES128_CMAC_SIZE as u32,
            )
        };
        assert_eq!(rc, status::MODULE_ERROR);
    }

    // -- Key management ------------------------------------------------------

    #[test]
    fn load_key_and_use() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let custom_key = [0xFFu8; AES128_KEY_SIZE];
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_load_key(5, custom_key.as_ptr(), AES128_KEY_SIZE as u32)
        };
        assert_eq!(rc, status::OK);

        // Encrypt with the custom key (slot 5).
        let plaintext = [0x42u8; 16];
        let mut ciphertext = [0u8; 16];
        let mut ct_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_encrypt(
                5,
                SHE_MODE_ECB,
                plaintext.as_ptr(),
                16,
                core::ptr::null(),
                ciphertext.as_mut_ptr(),
                &mut ct_len,
            )
        };
        assert_eq!(rc, status::OK);

        // Decrypt and verify round‑trip.
        let mut decrypted = [0u8; 16];
        let mut pt_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_decrypt(
                5,
                SHE_MODE_ECB,
                ciphertext.as_ptr(),
                16,
                core::ptr::null(),
                decrypted.as_mut_ptr(),
                &mut pt_len,
            )
        };
        assert_eq!(rc, status::OK);
        assert_eq!(&decrypted[..], &plaintext[..]);
    }

    #[test]
    fn load_key_rejects_invalid_slot() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        let key = [0u8; AES128_KEY_SIZE];
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_load_key(99, key.as_ptr(), AES128_KEY_SIZE as u32) };
        assert_eq!(rc, status::INVALID_ARGUMENT);
    }

    #[test]
    fn load_key_rejects_wrong_length() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        let key = [0u8; 8]; // wrong size
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_load_key(1, key.as_ptr(), 8) };
        assert_eq!(rc, status::INVALID_ARGUMENT);
    }

    #[test]
    fn encrypt_with_empty_slot_fails() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        let data = [0u8; 16];
        let mut out = [0u8; 16];
        let mut out_len = 0u32;
        // Slot 5 is empty.
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_encrypt(
                5,
                SHE_MODE_ECB,
                data.as_ptr(),
                16,
                core::ptr::null(),
                out.as_mut_ptr(),
                &mut out_len,
            )
        };
        assert_eq!(rc, status::MODULE_ERROR);
    }

    // -- RNG -----------------------------------------------------------------

    #[test]
    fn rng_generates_bytes() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let mut buf = [0u8; 32];
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_rng(buf.as_mut_ptr(), 32) };
        assert_eq!(rc, status::OK);
        // Extremely unlikely all 32 bytes are zero from a CSPRNG.
        assert_ne!(buf, [0u8; 32]);
    }

    #[test]
    fn rng_before_init_fails() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        let mut buf = [0u8; 16];
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_rng(buf.as_mut_ptr(), 16) };
        assert_eq!(rc, status::INIT_FAILED);
    }

    // -- Diag mailbox --------------------------------------------------------

    #[test]
    fn diag_mailbox_seed_key() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
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

        {
            let mb = shm.diag_mailbox_mut();
            mb.request_pending = 1;
            mb.request_type = DIAG_REQ_SEED_KEY;
        }

        #[allow(unsafe_code)]
        unsafe {
            hsm_step(1);
        }

        let mb = shm.diag_mailbox();
        assert_eq!(mb.request_pending, 0);
        assert_eq!(mb.response_ready, 1);
        assert_eq!(mb.response_status, 0);
        // Seed should be 16 bytes of random data (not all zero).
        assert_ne!(&mb.data[..SEED_LEN], &[0u8; SEED_LEN]);
    }

    // -- Constraint regression tests -----------------------------------------

    #[test]
    fn seed_consumed_after_successful_key_validation() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        // Generate seed and compute valid key.
        let mut seed_buf = [0u8; HSM_BUF_SIZE];
        let mut seed_len = 0u32;
        #[allow(unsafe_code)]
        unsafe {
            hsm_seed(seed_buf.as_mut_ptr(), &mut seed_len);
        }
        let expected_key = aes_cmac(&DEFAULT_MASTER_KEY, &seed_buf[..SEED_LEN]);

        // First validation succeeds.
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(expected_key.as_ptr(), AES128_CMAC_SIZE as u32) };
        assert_eq!(rc, status::OK);

        // Second attempt with the SAME key must fail (seed consumed).
        #[allow(unsafe_code)]
        let rc = unsafe { hsm_key(expected_key.as_ptr(), AES128_CMAC_SIZE as u32) };
        assert_eq!(rc, status::MODULE_ERROR, "seed replay must be rejected");
    }

    #[test]
    fn load_key_rejects_master_key_slot() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();

        let key = [0xFFu8; AES128_KEY_SIZE];
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_load_key(
                SECURITY_ACCESS_KEY_SLOT as u32,
                key.as_ptr(),
                AES128_KEY_SIZE as u32,
            )
        };
        assert_eq!(
            rc,
            status::INVALID_ARGUMENT,
            "slot 0 (master key) must not be overwritable"
        );
    }

    #[test]
    fn encrypt_rejects_zero_length_data() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        let mut out = [0u8; 16];
        let mut out_len = 0u32;
        // data_len = 0 is invalid even though 0 % 16 == 0.
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_encrypt(
                0,
                SHE_MODE_ECB,
                [].as_ptr(),
                0,
                core::ptr::null(),
                out.as_mut_ptr(),
                &mut out_len,
            )
        };
        assert_eq!(rc, status::INVALID_ARGUMENT);
    }

    #[test]
    fn decrypt_rejects_zero_length_data() {
        let _g = TEST_LOCK.lock().unwrap();
        reset();
        init_hsm();
        let mut out = [0u8; 16];
        let mut out_len = 0u32;
        #[allow(unsafe_code)]
        let rc = unsafe {
            hsm_decrypt(
                0,
                SHE_MODE_ECB,
                [].as_ptr(),
                0,
                core::ptr::null(),
                out.as_mut_ptr(),
                &mut out_len,
            )
        };
        assert_eq!(rc, status::INVALID_ARGUMENT);
    }
}
