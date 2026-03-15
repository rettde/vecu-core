//! Integration test: verify crypto stack (`Csm` → `CryIf` → `Cry` → HSM callbacks).
//!
//! Compiles the `BaseLayer` with P5 crypto modules, loads it via `libloading`,
//! injects mock HSM callbacks, and verifies:
//! 1. `Csm_Encrypt` produces the same output as the direct `hsm_encrypt` callback
//! 2. `Csm_RandomGenerate` returns non-zero random bytes
//! 3. `SecurityAccess` flow: seed → mac → key validate

#![allow(unsafe_code)]

use std::path::PathBuf;
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());

// ---------------------------------------------------------------------------
// FFI type aliases (module level for clippy::items_after_statements)
// ---------------------------------------------------------------------------

type BaseInitFn = unsafe extern "C" fn(*const TestBaseContext);
type BaseShutdownFn = unsafe extern "C" fn();
type CsmEncryptFn = unsafe extern "C" fn(u32, u32, *const u8, u32, *mut u8, *mut u32) -> u8;
type CsmDecryptFn = unsafe extern "C" fn(u32, u32, *const u8, u32, *mut u8, *mut u32) -> u8;
type CsmMacGenerateFn = unsafe extern "C" fn(u32, *const u8, u32, *mut u8, *mut u32) -> u8;
type CsmMacVerifyFn = unsafe extern "C" fn(u32, *const u8, u32, *const u8, u32, *mut u8) -> u8;
type CsmRandomGenerateFn = unsafe extern "C" fn(u32, *mut u8, *mut u32) -> u8;
type CsmSeedGenerateFn = unsafe extern "C" fn(*mut u8, *mut u32) -> u8;
type CsmKeyValidateFn = unsafe extern "C" fn(*const u8, u32) -> u8;

// ---------------------------------------------------------------------------
// Mirror of vecu_base_context_t
// ---------------------------------------------------------------------------

#[repr(C)]
struct TestVecuFrame {
    id: u32,
    len: u32,
    bus_type: u32,
    pad0: u32,
    data: [u8; 1536],
    timestamp: u64,
}

#[repr(C)]
struct TestBaseContext {
    push_tx_frame: Option<unsafe extern "C" fn(*const TestVecuFrame) -> i32>,
    pop_rx_frame: Option<unsafe extern "C" fn(*mut TestVecuFrame) -> i32>,
    hsm_encrypt: Option<
        unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32,
    >,
    hsm_decrypt: Option<
        unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32,
    >,
    hsm_generate_mac: Option<
        unsafe extern "C" fn(u32, *const u8, u32, *mut u8, *mut u32) -> i32,
    >,
    hsm_verify_mac: Option<
        unsafe extern "C" fn(u32, *const u8, u32, *const u8, u32) -> i32,
    >,
    hsm_seed: Option<unsafe extern "C" fn(*mut u8, *mut u32) -> i32>,
    hsm_key: Option<unsafe extern "C" fn(*const u8, u32) -> i32>,
    hsm_rng: Option<unsafe extern "C" fn(*mut u8, u32) -> i32>,
    shm_vars: *mut u8,
    shm_vars_size: u32,
    _pad0: u32,
    log_fn: Option<unsafe extern "C" fn(u32, *const std::ffi::c_char)>,
    tick_interval_us: u64,
}

// ---------------------------------------------------------------------------
// Mock HSM callbacks
// ---------------------------------------------------------------------------

/// Mock encrypt: XOR each byte with 0xAA (simple reversible "encryption").
unsafe extern "C" fn mock_hsm_encrypt(
    _key_slot: u32, _mode: u32,
    data: *const u8, data_len: u32,
    _iv: *const u8,
    out: *mut u8, out_len: *mut u32,
) -> i32 {
    if data.is_null() || out.is_null() || out_len.is_null() { return -2; }
    for i in 0..data_len as usize {
        unsafe { *out.add(i) = *data.add(i) ^ 0xAA; }
    }
    unsafe { *out_len = data_len; }
    0
}

/// Mock decrypt: XOR with 0xAA again (inverse of mock encrypt).
unsafe extern "C" fn mock_hsm_decrypt(
    _key_slot: u32, _mode: u32,
    data: *const u8, data_len: u32,
    _iv: *const u8,
    out: *mut u8, out_len: *mut u32,
) -> i32 {
    if data.is_null() || out.is_null() || out_len.is_null() { return -2; }
    for i in 0..data_len as usize {
        unsafe { *out.add(i) = *data.add(i) ^ 0xAA; }
    }
    unsafe { *out_len = data_len; }
    0
}

/// Mock MAC generate: fill with 0xBB (16 bytes).
unsafe extern "C" fn mock_hsm_generate_mac(
    _key_slot: u32,
    _data: *const u8, _data_len: u32,
    out_mac: *mut u8, out_mac_len: *mut u32,
) -> i32 {
    if out_mac.is_null() || out_mac_len.is_null() { return -2; }
    for i in 0..16_usize {
        unsafe { *out_mac.add(i) = 0xBB; }
    }
    unsafe { *out_mac_len = 16; }
    0
}

/// Mock MAC verify: check all bytes are 0xBB.
unsafe extern "C" fn mock_hsm_verify_mac(
    _key_slot: u32,
    _data: *const u8, _data_len: u32,
    mac: *const u8, mac_len: u32,
) -> i32 {
    if mac.is_null() { return -2; }
    for i in 0..mac_len as usize {
        if unsafe { *mac.add(i) } != 0xBB {
            return -5; // VECU_MODULE_ERROR
        }
    }
    0
}

static MOCK_SEED: [u8; 16] = [
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
];

/// Mock seed generate: return fixed seed.
unsafe extern "C" fn mock_hsm_seed(out_seed: *mut u8, out_len: *mut u32) -> i32 {
    if out_seed.is_null() || out_len.is_null() { return -2; }
    std::ptr::copy_nonoverlapping(MOCK_SEED.as_ptr(), out_seed, 16);
    unsafe { *out_len = 16; }
    0
}

/// Expected key = CMAC(master, seed). For mock: MAC of seed = all 0xBB.
unsafe extern "C" fn mock_hsm_key(key_buf: *const u8, key_len: u32) -> i32 {
    if key_buf.is_null() || key_len != 16 { return -5; }
    // Accept key that matches our mock MAC output (all 0xBB)
    for i in 0..16_usize {
        if unsafe { *key_buf.add(i) } != 0xBB {
            return -5;
        }
    }
    0
}

/// Mock RNG: fill with incrementing bytes.
unsafe extern "C" fn mock_hsm_rng(out_buf: *mut u8, buf_len: u32) -> i32 {
    if out_buf.is_null() { return -2; }
    for i in 0..buf_len as usize {
        #[allow(clippy::cast_possible_truncation)]
        unsafe { *out_buf.add(i) = (i + 1) as u8; }
    }
    0
}

unsafe extern "C" fn noop_push_tx(_frame: *const TestVecuFrame) -> i32 { -4 }
unsafe extern "C" fn noop_pop_rx(_frame: *mut TestVecuFrame) -> i32 { -4 }
unsafe extern "C" fn noop_log(_level: u32, _msg: *const std::ffi::c_char) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn dylib_ext() -> &'static str {
    if cfg!(target_os = "macos") { "dylib" }
    else if cfg!(target_os = "windows") { "dll" }
    else { "so" }
}

fn compile_baselayer(out_dir: &std::path::Path) -> PathBuf {
    let workspace = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent().unwrap()
        .parent().unwrap()
        .to_path_buf();

    let baselayer_src = workspace.join("baselayer").join("src");
    let baselayer_inc = workspace.join("baselayer").join("include");
    let abi_inc = workspace.join("crates").join("vecu-abi").join("include");

    let sources: Vec<PathBuf> = [
        "Base_Entry.c", "EcuM.c", "SchM.c", "Os.c", "Det.c", "Rte.c",
        "Com.c", "PduR.c", "CanIf.c", "EthIf.c", "LinIf.c", "FrIf.c",
        "Cry.c", "CryIf.c", "Csm.c",
        "NvM.c", "Fee.c", "MemIf.c", "Dem.c", "Dcm.c", "FiM.c", "WdgM.c",
        "CanTp.c", "DoIP.c",
    ].iter().map(|s| baselayer_src.join(s)).collect();

    let lib_path = out_dir.join(format!("libbase_crypto.{}", dylib_ext()));

    let mut cmd = std::process::Command::new("cc");
    cmd.arg("-shared")
        .arg("-std=c11")
        .arg("-fPIC")
        .arg("-I").arg(&baselayer_inc)
        .arg("-I").arg(&abi_inc)
        .arg("-o").arg(&lib_path);

    if cfg!(target_os = "macos") { cmd.arg("-dynamiclib"); }

    for src in &sources { cmd.arg(src); }

    let output = cmd.output().expect("failed to run cc");
    assert!(
        output.status.success(),
        "`BaseLayer` compilation failed:\n{}",
        String::from_utf8_lossy(&output.stderr)
    );

    assert!(lib_path.exists(), "libbase_crypto not found: {}", lib_path.display());
    lib_path
}

fn build_ctx() -> TestBaseContext {
    TestBaseContext {
        push_tx_frame: Some(noop_push_tx),
        pop_rx_frame: Some(noop_pop_rx),
        hsm_encrypt: Some(mock_hsm_encrypt),
        hsm_decrypt: Some(mock_hsm_decrypt),
        hsm_generate_mac: Some(mock_hsm_generate_mac),
        hsm_verify_mac: Some(mock_hsm_verify_mac),
        hsm_seed: Some(mock_hsm_seed),
        hsm_key: Some(mock_hsm_key),
        hsm_rng: Some(mock_hsm_rng),
        shm_vars: std::ptr::null_mut(),
        shm_vars_size: 0,
        _pad0: 0,
        log_fn: Some(noop_log),
        tick_interval_us: 1000,
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[test]
fn crypto_csm_encrypt_matches_direct_hsm() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_crypto_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load `BaseLayer`");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_encrypt: libloading::Symbol<CsmEncryptFn> =
        unsafe { lib.get(b"Csm_Encrypt") }.expect("Csm_Encrypt");

    let ctx = build_ctx();
    unsafe { (base_init)(&ctx) };

    // Encrypt 16 bytes of plaintext
    let plaintext: [u8; 16] = [0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF];
    let mut ciphertext = [0u8; 16];
    let mut cipher_len: u32 = 16;

    let rc = unsafe {
        (csm_encrypt)(
            0, // jobId
            0, // mode (ECB)
            plaintext.as_ptr(),
            16,
            std::ptr::addr_of_mut!(ciphertext).cast::<u8>(),
            std::ptr::addr_of_mut!(cipher_len),
        )
    };
    assert_eq!(rc, 0, "Csm_Encrypt failed");
    assert_eq!(cipher_len, 16);

    // Verify: mock XOR's with 0xAA
    let expected: Vec<u8> = plaintext.iter().map(|b| b ^ 0xAA).collect();
    assert_eq!(&ciphertext[..], &expected[..], "Csm_Encrypt output should match direct hsm_encrypt (XOR 0xAA)");

    unsafe { (base_shutdown)() };
}

#[test]
fn crypto_csm_decrypt_reverses_encrypt() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_crypto_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load `BaseLayer`");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_encrypt: libloading::Symbol<CsmEncryptFn> =
        unsafe { lib.get(b"Csm_Encrypt") }.expect("Csm_Encrypt");
    let csm_decrypt: libloading::Symbol<CsmDecryptFn> =
        unsafe { lib.get(b"Csm_Decrypt") }.expect("Csm_Decrypt");

    let ctx = build_ctx();
    unsafe { (base_init)(&ctx) };

    let plaintext: [u8; 16] = [0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
                                0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C];
    let mut ciphertext = [0u8; 16];
    let mut len: u32 = 16;

    // Encrypt
    let rc = unsafe {
        (csm_encrypt)(0, 0, plaintext.as_ptr(), 16,
                      std::ptr::addr_of_mut!(ciphertext).cast::<u8>(),
                      std::ptr::addr_of_mut!(len))
    };
    assert_eq!(rc, 0);

    // Decrypt
    let mut recovered = [0u8; 16];
    let mut rlen: u32 = 16;
    let rc = unsafe {
        (csm_decrypt)(0, 0, ciphertext.as_ptr(), 16,
                      std::ptr::addr_of_mut!(recovered).cast::<u8>(),
                      std::ptr::addr_of_mut!(rlen))
    };
    assert_eq!(rc, 0);
    assert_eq!(&recovered[..], &plaintext[..], "Decrypt should reverse encrypt");

    unsafe { (base_shutdown)() };
}

#[test]
fn crypto_csm_mac_generate_and_verify() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_crypto_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load `BaseLayer`");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_mac_gen: libloading::Symbol<CsmMacGenerateFn> =
        unsafe { lib.get(b"Csm_MacGenerate") }.expect("Csm_MacGenerate");
    let csm_mac_verify: libloading::Symbol<CsmMacVerifyFn> =
        unsafe { lib.get(b"Csm_MacVerify") }.expect("Csm_MacVerify");

    let ctx = build_ctx();
    unsafe { (base_init)(&ctx) };

    let data: [u8; 8] = [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08];
    let mut mac = [0u8; 16];
    let mut mac_len: u32 = 16;

    // Generate MAC
    let rc = unsafe {
        (csm_mac_gen)(0, data.as_ptr(), 8,
                      mac.as_mut_ptr(), std::ptr::addr_of_mut!(mac_len))
    };
    assert_eq!(rc, 0, "Csm_MacGenerate failed");
    assert_eq!(mac_len, 16);
    assert!(mac.iter().all(|&b| b == 0xBB), "Mock MAC should be all 0xBB");

    // Verify MAC (should succeed)
    let mut verify_result: u8 = 0;
    let rc = unsafe {
        (csm_mac_verify)(0, data.as_ptr(), 8,
                         mac.as_ptr(), 16,
                         std::ptr::addr_of_mut!(verify_result))
    };
    assert_eq!(rc, 0, "Csm_MacVerify failed");
    assert_eq!(verify_result, 1, "MAC verification should succeed (TRUE)");

    // Verify with bad MAC (should fail)
    let bad_mac = [0u8; 16];
    let mut verify_result2: u8 = 1;
    let rc = unsafe {
        (csm_mac_verify)(0, data.as_ptr(), 8,
                         bad_mac.as_ptr(), 16,
                         std::ptr::addr_of_mut!(verify_result2))
    };
    assert_eq!(rc, 0, "Csm_MacVerify should return E_OK even on mismatch");
    assert_eq!(verify_result2, 0, "MAC verification should fail (FALSE)");

    unsafe { (base_shutdown)() };
}

#[test]
fn crypto_csm_random_generate() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_crypto_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load `BaseLayer`");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_rng: libloading::Symbol<CsmRandomGenerateFn> =
        unsafe { lib.get(b"Csm_RandomGenerate") }.expect("Csm_RandomGenerate");

    let ctx = build_ctx();
    unsafe { (base_init)(&ctx) };

    let mut buf = [0u8; 32];
    let mut buf_len: u32 = 32;

    let rc = unsafe {
        (csm_rng)(0, buf.as_mut_ptr(), std::ptr::addr_of_mut!(buf_len))
    };
    assert_eq!(rc, 0, "Csm_RandomGenerate failed");
    // Mock RNG fills with incrementing bytes: [1, 2, 3, ...]
    assert!(buf.iter().any(|&b| b != 0), "Random bytes should be non-zero");
    assert_eq!(buf[0], 1);
    assert_eq!(buf[31], 32);

    unsafe { (base_shutdown)() };
}

#[test]
fn crypto_security_access_flow() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_crypto_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load `BaseLayer`");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_seed: libloading::Symbol<CsmSeedGenerateFn> =
        unsafe { lib.get(b"Csm_SeedGenerate") }.expect("Csm_SeedGenerate");
    let csm_mac_gen: libloading::Symbol<CsmMacGenerateFn> =
        unsafe { lib.get(b"Csm_MacGenerate") }.expect("Csm_MacGenerate");
    let csm_key_validate: libloading::Symbol<CsmKeyValidateFn> =
        unsafe { lib.get(b"Csm_KeyValidate") }.expect("Csm_KeyValidate");

    let ctx = build_ctx();
    unsafe { (base_init)(&ctx) };

    // Step 1: Generate seed
    let mut seed = [0u8; 16];
    let mut seed_len: u32 = 0;
    let rc = unsafe {
        (csm_seed)(seed.as_mut_ptr(), std::ptr::addr_of_mut!(seed_len))
    };
    assert_eq!(rc, 0, "Csm_SeedGenerate failed");
    assert_eq!(seed_len, 16);
    assert_eq!(&seed, &MOCK_SEED, "Seed should match mock");

    // Step 2: Compute key = CMAC(master_key, seed)
    let mut key = [0u8; 16];
    let mut key_len: u32 = 16;
    let rc = unsafe {
        (csm_mac_gen)(0, seed.as_ptr(), seed_len,
                      key.as_mut_ptr(), std::ptr::addr_of_mut!(key_len))
    };
    assert_eq!(rc, 0, "Csm_MacGenerate for key failed");
    assert_eq!(key_len, 16);

    // Step 3: Validate key with HSM
    let rc = unsafe {
        (csm_key_validate)(key.as_ptr(), key_len)
    };
    assert_eq!(rc, 0, "Csm_KeyValidate should accept the computed key");

    // Step 4: Verify wrong key is rejected
    let bad_key = [0u8; 16];
    let rc = unsafe {
        (csm_key_validate)(bad_key.as_ptr(), 16)
    };
    assert_ne!(rc, 0, "Csm_KeyValidate should reject a wrong key");

    unsafe { (base_shutdown)() };
}
