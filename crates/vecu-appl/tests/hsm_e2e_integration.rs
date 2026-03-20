//! E2E integration test: real `vecu-hsm` AES-128 / CMAC / RNG wired through
//! the `BaseLayer` crypto chain (`Csm` → `CryIf` → `Cry` → HSM callbacks).
//!
//! Unlike `crypto_integration.rs` (which uses mock XOR callbacks), this test
//! exercises the **production** HSM implementation to prove the full wiring
//! from Loader → APPL → `BaseLayer` → Cry → vecu-hsm actually works.
//!
//! Requires a POSIX C compiler (`cc`); skipped on Windows.

#![cfg(not(target_os = "windows"))]
#![allow(unsafe_code)]

use std::path::PathBuf;
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());

// ---------------------------------------------------------------------------
// FFI type aliases
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
// Mirror of vecu_base_context_t (must match vecu_base_context.h)
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
    hsm_encrypt:
        Option<unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32>,
    hsm_decrypt:
        Option<unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32>,
    hsm_generate_mac: Option<unsafe extern "C" fn(u32, *const u8, u32, *mut u8, *mut u32) -> i32>,
    hsm_verify_mac: Option<unsafe extern "C" fn(u32, *const u8, u32, *const u8, u32) -> i32>,
    hsm_seed: Option<unsafe extern "C" fn(*mut u8, *mut u32) -> i32>,
    hsm_key: Option<unsafe extern "C" fn(*const u8, u32) -> i32>,
    hsm_rng: Option<unsafe extern "C" fn(*mut u8, u32) -> i32>,
    shm_vars: *mut u8,
    shm_vars_size: u32,
    _pad0: u32,
    log_fn: Option<unsafe extern "C" fn(u32, *const std::ffi::c_char)>,
    tick_interval_us: u64,
    hsm_hash: Option<unsafe extern "C" fn(u32, *const u8, u32, *mut u8, *mut u32) -> i32>,
}

// ---------------------------------------------------------------------------
// No-op frame / log callbacks
// ---------------------------------------------------------------------------

unsafe extern "C" fn noop_push_tx(_frame: *const TestVecuFrame) -> i32 {
    -4
}
unsafe extern "C" fn noop_pop_rx(_frame: *mut TestVecuFrame) -> i32 {
    -4
}
unsafe extern "C" fn noop_log(_level: u32, _msg: *const std::ffi::c_char) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn dylib_ext() -> &'static str {
    if cfg!(target_os = "macos") {
        "dylib"
    } else if cfg!(target_os = "windows") {
        "dll"
    } else {
        "so"
    }
}

fn compile_baselayer(out_dir: &std::path::Path) -> PathBuf {
    let workspace = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .to_path_buf();

    let baselayer_src = workspace.join("baselayer").join("src");
    let baselayer_inc = workspace.join("baselayer").join("include");
    let abi_inc = workspace.join("crates").join("vecu-abi").join("include");

    let sources: Vec<PathBuf> = [
        "Base_Entry.c",
        "EcuM.c",
        "SchM.c",
        "Os.c",
        "Det.c",
        "Rte.c",
        "Com.c",
        "PduR.c",
        "CanIf.c",
        "EthIf.c",
        "LinIf.c",
        "FrIf.c",
        "Cry.c",
        "CryIf.c",
        "Csm.c",
        "NvM.c",
        "Fee.c",
        "MemIf.c",
        "Dem.c",
        "Dcm.c",
        "FiM.c",
        "WdgM.c",
        "CanTp.c",
        "DoIP.c",
    ]
    .iter()
    .map(|s| baselayer_src.join(s))
    .collect();

    let lib_path = out_dir.join(format!("libbase_hsm_e2e.{}", dylib_ext()));

    let mut cmd = std::process::Command::new("cc");
    cmd.arg("-shared")
        .arg("-std=c11")
        .arg("-fPIC")
        .arg("-I")
        .arg(&baselayer_inc)
        .arg("-I")
        .arg(&abi_inc)
        .arg("-o")
        .arg(&lib_path);

    if cfg!(target_os = "macos") {
        cmd.arg("-dynamiclib");
    }

    for src in &sources {
        cmd.arg(src);
    }

    let output = cmd.output().expect("failed to run cc");
    assert!(
        output.status.success(),
        "`BaseLayer` compilation failed:\n{}",
        String::from_utf8_lossy(&output.stderr)
    );

    assert!(
        lib_path.exists(),
        "libbase_hsm_e2e not found: {}",
        lib_path.display()
    );
    lib_path
}

fn get_hsm_api() -> vecu_abi::VecuPluginApi {
    let mut api = vecu_abi::VecuPluginApi::zeroed();
    let rc = unsafe { vecu_hsm::vecu_get_api(vecu_abi::ABI_VERSION, &mut api) };
    assert_eq!(rc, vecu_abi::status::OK, "vecu_hsm::vecu_get_api failed");
    api
}

fn init_hsm(api: &vecu_abi::VecuPluginApi) {
    let ctx = vecu_abi::VecuRuntimeContext {
        shm_base: core::ptr::null_mut(),
        shm_size: 0,
        pad0: 0,
        tick_interval_us: 1000,
        log_fn: None,
        hsm_api: core::ptr::null(),
    };
    let rc = unsafe { (api.init.unwrap())(&ctx) };
    assert_eq!(rc, vecu_abi::status::OK, "HSM init failed");
}

fn build_ctx(hsm_api: &vecu_abi::VecuPluginApi) -> TestBaseContext {
    TestBaseContext {
        push_tx_frame: Some(noop_push_tx),
        pop_rx_frame: Some(noop_pop_rx),
        hsm_encrypt: hsm_api.encrypt,
        hsm_decrypt: hsm_api.decrypt,
        hsm_generate_mac: hsm_api.generate_mac,
        hsm_verify_mac: hsm_api.verify_mac,
        hsm_seed: hsm_api.seed,
        hsm_key: hsm_api.key,
        hsm_rng: hsm_api.rng,
        shm_vars: std::ptr::null_mut(),
        shm_vars_size: 0,
        _pad0: 0,
        log_fn: Some(noop_log),
        tick_interval_us: 1000,
        hsm_hash: hsm_api.hash,
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[test]
fn e2e_aes128_ecb_encrypt_decrypt_round_trip() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_hsm_e2e");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.expect("load BaseLayer");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_encrypt: libloading::Symbol<CsmEncryptFn> =
        unsafe { lib.get(b"Csm_Encrypt") }.expect("Csm_Encrypt");
    let csm_decrypt: libloading::Symbol<CsmDecryptFn> =
        unsafe { lib.get(b"Csm_Decrypt") }.expect("Csm_Decrypt");

    let hsm_api = get_hsm_api();
    init_hsm(&hsm_api);

    let ctx = build_ctx(&hsm_api);
    unsafe { (base_init)(&ctx) };

    let plaintext: [u8; 16] = [
        0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96, 0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17,
        0x2A,
    ];
    let mut ciphertext = [0u8; 16];
    let mut ct_len: u32 = 16;

    let rc = unsafe {
        (csm_encrypt)(
            0,
            0, // ECB
            plaintext.as_ptr(),
            16,
            ciphertext.as_mut_ptr(),
            std::ptr::addr_of_mut!(ct_len),
        )
    };
    assert_eq!(rc, 0, "Csm_Encrypt failed");
    assert_eq!(ct_len, 16);
    assert_ne!(
        &ciphertext[..],
        &plaintext[..],
        "Ciphertext must differ from plaintext"
    );

    let mut recovered = [0u8; 16];
    let mut rec_len: u32 = 16;
    let rc = unsafe {
        (csm_decrypt)(
            0,
            0, // ECB
            ciphertext.as_ptr(),
            16,
            recovered.as_mut_ptr(),
            std::ptr::addr_of_mut!(rec_len),
        )
    };
    assert_eq!(rc, 0, "Csm_Decrypt failed");
    assert_eq!(rec_len, 16);
    assert_eq!(
        &recovered[..],
        &plaintext[..],
        "AES-128 ECB decrypt must recover original plaintext"
    );

    unsafe { (base_shutdown)() };
    if let Some(f) = hsm_api.shutdown {
        f();
    }
}

#[test]
fn e2e_cmac_generate_and_verify() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_hsm_e2e");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.expect("load BaseLayer");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_mac_gen: libloading::Symbol<CsmMacGenerateFn> =
        unsafe { lib.get(b"Csm_MacGenerate") }.expect("Csm_MacGenerate");
    let csm_mac_verify: libloading::Symbol<CsmMacVerifyFn> =
        unsafe { lib.get(b"Csm_MacVerify") }.expect("Csm_MacVerify");

    let hsm_api = get_hsm_api();
    init_hsm(&hsm_api);

    let ctx = build_ctx(&hsm_api);
    unsafe { (base_init)(&ctx) };

    let data: [u8; 16] = [
        0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96, 0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17,
        0x2A,
    ];
    let mut mac = [0u8; 16];
    let mut mac_len: u32 = 16;

    let rc = unsafe {
        (csm_mac_gen)(
            0,
            data.as_ptr(),
            16,
            mac.as_mut_ptr(),
            std::ptr::addr_of_mut!(mac_len),
        )
    };
    assert_eq!(rc, 0, "Csm_MacGenerate failed");
    assert_eq!(mac_len, 16);
    assert!(mac.iter().any(|&b| b != 0), "CMAC must not be all zeros");

    let mut verify_result: u8 = 0;
    let rc = unsafe {
        (csm_mac_verify)(
            0,
            data.as_ptr(),
            16,
            mac.as_ptr(),
            16,
            std::ptr::addr_of_mut!(verify_result),
        )
    };
    assert_eq!(rc, 0, "Csm_MacVerify failed");
    assert_eq!(
        verify_result, 1,
        "CMAC verification of correct MAC must succeed"
    );

    let bad_mac = [0xFFu8; 16];
    let mut verify_result2: u8 = 1;
    let rc = unsafe {
        (csm_mac_verify)(
            0,
            data.as_ptr(),
            16,
            bad_mac.as_ptr(),
            16,
            std::ptr::addr_of_mut!(verify_result2),
        )
    };
    assert_eq!(rc, 0, "Csm_MacVerify should return E_OK even on mismatch");
    assert_eq!(
        verify_result2, 0,
        "CMAC verification of wrong MAC must fail"
    );

    unsafe { (base_shutdown)() };
    if let Some(f) = hsm_api.shutdown {
        f();
    }
}

#[test]
fn e2e_rng_produces_nonzero_bytes() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_hsm_e2e");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.expect("load BaseLayer");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_rng: libloading::Symbol<CsmRandomGenerateFn> =
        unsafe { lib.get(b"Csm_RandomGenerate") }.expect("Csm_RandomGenerate");

    let hsm_api = get_hsm_api();
    init_hsm(&hsm_api);

    let ctx = build_ctx(&hsm_api);
    unsafe { (base_init)(&ctx) };

    let mut buf = [0u8; 32];
    let mut buf_len: u32 = 32;
    let rc = unsafe { (csm_rng)(0, buf.as_mut_ptr(), std::ptr::addr_of_mut!(buf_len)) };
    assert_eq!(rc, 0, "Csm_RandomGenerate failed");
    assert!(
        buf.iter().any(|&b| b != 0),
        "RNG output should contain non-zero bytes"
    );

    unsafe { (base_shutdown)() };
    if let Some(f) = hsm_api.shutdown {
        f();
    }
}

#[test]
fn e2e_security_access_seed_and_key() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_hsm_e2e");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.expect("load BaseLayer");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let csm_seed: libloading::Symbol<CsmSeedGenerateFn> =
        unsafe { lib.get(b"Csm_SeedGenerate") }.expect("Csm_SeedGenerate");
    let csm_key_validate: libloading::Symbol<CsmKeyValidateFn> =
        unsafe { lib.get(b"Csm_KeyValidate") }.expect("Csm_KeyValidate");

    let hsm_api = get_hsm_api();
    init_hsm(&hsm_api);

    let ctx = build_ctx(&hsm_api);
    unsafe { (base_init)(&ctx) };

    let mut seed = [0u8; 64];
    let mut seed_len: u32 = 0;
    let rc = unsafe { (csm_seed)(seed.as_mut_ptr(), std::ptr::addr_of_mut!(seed_len)) };
    assert_eq!(rc, 0, "Csm_SeedGenerate failed");
    assert!(seed_len > 0, "Seed length must be > 0");

    let actual_seed = &seed[..seed_len as usize];
    let mut key_response = [0u8; 16];
    let mut key_len: u32 = 16;
    let rc = unsafe {
        (hsm_api.generate_mac.unwrap())(
            0,
            actual_seed.as_ptr(),
            seed_len,
            key_response.as_mut_ptr(),
            std::ptr::addr_of_mut!(key_len),
        )
    };
    assert_eq!(rc, 0, "Direct CMAC of seed failed");

    let rc = unsafe { (csm_key_validate)(key_response.as_ptr(), key_len) };
    assert_eq!(rc, 0, "Key derived from seed+CMAC must be accepted");

    let bad_key = [0u8; 16];
    let rc = unsafe { (csm_key_validate)(bad_key.as_ptr(), 16) };
    assert_ne!(rc, 0, "Wrong key must be rejected");

    unsafe { (base_shutdown)() };
    if let Some(f) = hsm_api.shutdown {
        f();
    }
}
