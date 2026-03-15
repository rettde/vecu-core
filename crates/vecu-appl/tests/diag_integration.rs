//! Integration test: verify diagnostics & memory stack (P6).
//!
//! Tests `Dcm` (UDS dispatch), `Dem` (DTC lifecycle), and `NvM`
//! (block read/write via SHM) end-to-end through the `BaseLayer`.

#![allow(unsafe_code)]

use std::path::PathBuf;
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());

// ---------------------------------------------------------------------------
// FFI type aliases
// ---------------------------------------------------------------------------

type BaseInitFn = unsafe extern "C" fn(*const TestBaseContext);
type BaseShutdownFn = unsafe extern "C" fn();
type DcmProcessFn = unsafe extern "C" fn(*const u8, u16, *mut u8, u16) -> u16;
type DcmGetSessionFn = unsafe extern "C" fn() -> u8;
type DcmGetSecurityFn = unsafe extern "C" fn() -> u8;
type DemReportFn = unsafe extern "C" fn(u32, u8) -> u8;
type DemGetStatusFn = unsafe extern "C" fn(u32, *mut u8) -> u8;
type DemClearFn = unsafe extern "C" fn(u32) -> u8;
type DemCountFn = unsafe extern "C" fn(u8) -> u16;
type NvmWriteBlockFn = unsafe extern "C" fn(u16, *const u8) -> u8;
type NvmReadBlockFn = unsafe extern "C" fn(u16, *mut u8) -> u8;

// ---------------------------------------------------------------------------
// Mirror of vecu_base_context_t
// ---------------------------------------------------------------------------

#[repr(C)]
struct TestVecuFrame {
    id: u32, len: u32, bus_type: u32, pad0: u32,
    data: [u8; 1536], timestamp: u64,
}

#[repr(C)]
struct TestBaseContext {
    push_tx_frame: Option<unsafe extern "C" fn(*const TestVecuFrame) -> i32>,
    pop_rx_frame: Option<unsafe extern "C" fn(*mut TestVecuFrame) -> i32>,
    hsm_encrypt: Option<unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32>,
    hsm_decrypt: Option<unsafe extern "C" fn(u32, u32, *const u8, u32, *const u8, *mut u8, *mut u32) -> i32>,
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
}

// ---------------------------------------------------------------------------
// Mock HSM callbacks (reuse from crypto_integration pattern)
// ---------------------------------------------------------------------------

unsafe extern "C" fn mock_hsm_generate_mac(
    _ks: u32, _d: *const u8, _dl: u32, out: *mut u8, out_len: *mut u32,
) -> i32 {
    for i in 0..16_usize { unsafe { *out.add(i) = 0xBB; } }
    unsafe { *out_len = 16; }
    0
}

unsafe extern "C" fn mock_hsm_verify_mac(
    _ks: u32, _d: *const u8, _dl: u32, mac: *const u8, ml: u32,
) -> i32 {
    for i in 0..ml as usize {
        if unsafe { *mac.add(i) } != 0xBB { return -5; }
    }
    0
}

static MOCK_SEED: [u8; 16] = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16];

unsafe extern "C" fn mock_hsm_seed(out: *mut u8, out_len: *mut u32) -> i32 {
    std::ptr::copy_nonoverlapping(MOCK_SEED.as_ptr(), out, 16);
    unsafe { *out_len = 16; }
    0
}

unsafe extern "C" fn mock_hsm_key(key: *const u8, kl: u32) -> i32 {
    if kl != 16 { return -5; }
    for i in 0..16_usize {
        if unsafe { *key.add(i) } != 0xBB { return -5; }
    }
    0
}

unsafe extern "C" fn noop_push_tx(_: *const TestVecuFrame) -> i32 { -4 }
unsafe extern "C" fn noop_pop_rx(_: *mut TestVecuFrame) -> i32 { -4 }
unsafe extern "C" fn noop_log(_: u32, _: *const std::ffi::c_char) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn dylib_ext() -> &'static str {
    if cfg!(target_os = "macos") { "dylib" }
    else if cfg!(target_os = "windows") { "dll" }
    else { "so" }
}

/// All `BaseLayer` source files (P3–P6).
const ALL_SOURCES: &[&str] = &[
    "Base_Entry.c", "EcuM.c", "SchM.c", "Os.c", "Det.c", "Rte.c",
    "Com.c", "PduR.c", "CanIf.c", "EthIf.c", "LinIf.c", "FrIf.c",
    "Cry.c", "CryIf.c", "Csm.c",
    "NvM.c", "Fee.c", "MemIf.c", "Dem.c", "Dcm.c", "FiM.c", "WdgM.c",
    "CanTp.c", "DoIP.c",
];

fn compile_baselayer(out_dir: &std::path::Path) -> PathBuf {
    let workspace = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent().unwrap().parent().unwrap().to_path_buf();
    let baselayer_src = workspace.join("baselayer").join("src");
    let baselayer_inc = workspace.join("baselayer").join("include");
    let abi_inc = workspace.join("crates").join("vecu-abi").join("include");

    let sources: Vec<PathBuf> = ALL_SOURCES.iter()
        .map(|s| baselayer_src.join(s)).collect();

    let lib_path = out_dir.join(format!("libbase_diag.{}", dylib_ext()));

    let mut cmd = std::process::Command::new("cc");
    cmd.arg("-shared").arg("-std=c11").arg("-fPIC")
        .arg("-I").arg(&baselayer_inc)
        .arg("-I").arg(&abi_inc)
        .arg("-o").arg(&lib_path);
    if cfg!(target_os = "macos") { cmd.arg("-dynamiclib"); }
    for src in &sources { cmd.arg(src); }

    let output = cmd.output().expect("failed to run cc");
    assert!(output.status.success(),
        "`BaseLayer` compilation failed:\n{}",
        String::from_utf8_lossy(&output.stderr));
    lib_path
}

fn build_ctx(shm: &mut [u8]) -> TestBaseContext {
    TestBaseContext {
        push_tx_frame: Some(noop_push_tx),
        pop_rx_frame: Some(noop_pop_rx),
        hsm_encrypt: None, hsm_decrypt: None,
        hsm_generate_mac: Some(mock_hsm_generate_mac),
        hsm_verify_mac: Some(mock_hsm_verify_mac),
        hsm_seed: Some(mock_hsm_seed),
        hsm_key: Some(mock_hsm_key),
        hsm_rng: None,
        shm_vars: shm.as_mut_ptr(),
        #[allow(clippy::cast_possible_truncation)]
        shm_vars_size: shm.len() as u32,
        _pad0: 0,
        log_fn: Some(noop_log),
        tick_interval_us: 1000,
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[test]
fn diag_security_access_via_dcm() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_diag_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> = unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let dcm_process: libloading::Symbol<DcmProcessFn> = unsafe { lib.get(b"Dcm_ProcessRequest") }.unwrap();
    let dcm_session: libloading::Symbol<DcmGetSessionFn> = unsafe { lib.get(b"Dcm_GetActiveSession") }.unwrap();
    let dcm_security: libloading::Symbol<DcmGetSecurityFn> = unsafe { lib.get(b"Dcm_GetSecurityLevel") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);
    unsafe { (base_init)(&ctx) };

    // 1. Switch to Extended session (0x10 0x03)
    let req = [0x10u8, 0x03];
    let mut resp = [0u8; 256];
    let len = unsafe { (dcm_process)(req.as_ptr(), 2, resp.as_mut_ptr(), 256) };
    assert!(len >= 2, "Session control response too short");
    assert_eq!(resp[0], 0x50, "Positive response for 0x10");
    assert_eq!(resp[1], 0x03, "Extended session");
    assert_eq!(unsafe { (dcm_session)() }, 0x03);

    // 2. Request seed (0x27 0x01)
    let req = [0x27u8, 0x01];
    let len = unsafe { (dcm_process)(req.as_ptr(), 2, resp.as_mut_ptr(), 256) };
    assert!(len >= 18, "Seed response should be 2 + 16 bytes");
    assert_eq!(resp[0], 0x67, "Positive response for 0x27");
    assert_eq!(resp[1], 0x01);
    let seed = &resp[2..18];
    assert_eq!(seed, &MOCK_SEED, "Seed should match mock");

    // 3. Compute key = MAC(seed) and send it (0x27 0x02 + key)
    let mut key_req = [0u8; 18];
    key_req[0] = 0x27;
    key_req[1] = 0x02;
    // Mock MAC = all 0xBB
    for b in &mut key_req[2..18] { *b = 0xBB; }
    let len = unsafe { (dcm_process)(key_req.as_ptr(), 18, resp.as_mut_ptr(), 256) };
    assert!(len >= 2);
    assert_eq!(resp[0], 0x67, "Positive response for key send");
    assert_eq!(resp[1], 0x02);
    assert_eq!(unsafe { (dcm_security)() }, 0x01, "Security should be unlocked");

    unsafe { (base_shutdown)() };
}

#[test]
fn diag_read_write_did_via_nvm() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_diag_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> = unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let dcm_process: libloading::Symbol<DcmProcessFn> = unsafe { lib.get(b"Dcm_ProcessRequest") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);
    unsafe { (base_init)(&ctx) };

    // Write DID 0xF101 (4 bytes) via UDS 0x2E
    let req = [0x2Eu8, 0xF1, 0x01, 0xDE, 0xAD, 0xBE, 0xEF];
    let mut resp = [0u8; 256];
    let len = unsafe { (dcm_process)(req.as_ptr(), 7, resp.as_mut_ptr(), 256) };
    assert!(len >= 3);
    assert_eq!(resp[0], 0x6E, "Positive response for WriteDID");

    // Read DID 0xF101 back via UDS 0x22
    let req = [0x22u8, 0xF1, 0x01];
    let len = unsafe { (dcm_process)(req.as_ptr(), 3, resp.as_mut_ptr(), 256) };
    assert!(len >= 7, "ReadDID response should be 3 header + 4 data");
    assert_eq!(resp[0], 0x62, "Positive response for ReadDID");
    assert_eq!(resp[1], 0xF1);
    assert_eq!(resp[2], 0x01);
    assert_eq!(&resp[3..7], &[0xDE, 0xAD, 0xBE, 0xEF]);

    // Verify data persisted to SHM (NvM block 1 at offset 32)
    assert_eq!(&shm[32..36], &[0xDE, 0xAD, 0xBE, 0xEF]);

    unsafe { (base_shutdown)() };
}

#[test]
fn diag_dtc_lifecycle() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_diag_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> = unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let dem_report: libloading::Symbol<DemReportFn> = unsafe { lib.get(b"Dem_ReportErrorStatus") }.unwrap();
    let dem_status: libloading::Symbol<DemGetStatusFn> = unsafe { lib.get(b"Dem_GetDTCStatus") }.unwrap();
    let _dem_clear: libloading::Symbol<DemClearFn> = unsafe { lib.get(b"Dem_ClearDTC") }.unwrap();
    let dem_count: libloading::Symbol<DemCountFn> = unsafe { lib.get(b"Dem_GetNumberOfDTCByStatusMask") }.unwrap();
    let dcm_process: libloading::Symbol<DcmProcessFn> = unsafe { lib.get(b"Dcm_ProcessRequest") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);
    unsafe { (base_init)(&ctx) };

    // 1. Report a DTC
    let rc = unsafe { (dem_report)(0x00C0_7300, 1) };
    assert_eq!(rc, 0, "Dem_ReportErrorStatus failed");

    // 2. Verify status
    let mut status: u8 = 0;
    let rc = unsafe { (dem_status)(0x00C0_7300, std::ptr::addr_of_mut!(status)) };
    assert_eq!(rc, 0);
    assert_ne!(status & 0x01, 0, "testFailed bit should be set");
    assert_ne!(status & 0x08, 0, "confirmed bit should be set");

    // 3. Count DTCs
    let count = unsafe { (dem_count)(0xFF) };
    assert_eq!(count, 1, "Should have 1 DTC");

    // 4. Read DTCs via UDS 0x19 sub 0x01
    let req = [0x19u8, 0x01, 0xFF]; // reportNumberOfDTCByStatusMask
    let mut resp = [0u8; 256];
    let len = unsafe { (dcm_process)(req.as_ptr(), 3, resp.as_mut_ptr(), 256) };
    assert!(len >= 6);
    assert_eq!(resp[0], 0x59, "Positive response for ReadDTC");
    let dtc_count = (u16::from(resp[4]) << 8) | u16::from(resp[5]);
    assert_eq!(dtc_count, 1);

    // 5. Clear DTCs via UDS 0x14
    let req = [0x14u8, 0xFF, 0xFF, 0xFF]; // clear all
    let len = unsafe { (dcm_process)(req.as_ptr(), 4, resp.as_mut_ptr(), 256) };
    assert!(len >= 1);
    assert_eq!(resp[0], 0x54, "Positive response for ClearDTC");

    // 6. Verify cleared
    let count = unsafe { (dem_count)(0xFF) };
    assert_eq!(count, 0, "DTCs should be cleared");

    unsafe { (base_shutdown)() };
}

#[test]
fn diag_nvm_direct_read_write() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_diag_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> = unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let nvm_write: libloading::Symbol<NvmWriteBlockFn> = unsafe { lib.get(b"NvM_WriteBlock") }.unwrap();
    let nvm_read: libloading::Symbol<NvmReadBlockFn> = unsafe { lib.get(b"NvM_ReadBlock") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);
    unsafe { (base_init)(&ctx) };

    // Write block 0 (VIN, 17 bytes)
    let vin = b"WVWZZZ3CZWE123456";
    let rc = unsafe { (nvm_write)(0, vin.as_ptr()) };
    assert_eq!(rc, 0, "NvM_WriteBlock failed");

    // Verify SHM was written (block 0 at offset 0, 17 bytes)
    assert_eq!(&shm[0..17], &vin[..]);

    // Read it back
    let mut buf = [0u8; 17];
    let rc = unsafe { (nvm_read)(0, buf.as_mut_ptr()) };
    assert_eq!(rc, 0, "NvM_ReadBlock failed");
    assert_eq!(&buf, &vin[..]);

    unsafe { (base_shutdown)() };
}

#[test]
fn diag_session_timeout_nrc() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_diag_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> = unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let dcm_process: libloading::Symbol<DcmProcessFn> = unsafe { lib.get(b"Dcm_ProcessRequest") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);
    unsafe { (base_init)(&ctx) };

    // SecurityAccess in default session should be rejected
    let req = [0x27u8, 0x01];
    let mut resp = [0u8; 256];
    let len = unsafe { (dcm_process)(req.as_ptr(), 2, resp.as_mut_ptr(), 256) };
    assert!(len >= 3);
    assert_eq!(resp[0], 0x7F, "Negative response");
    assert_eq!(resp[1], 0x27, "SID echo");
    assert_eq!(resp[2], 0x22, "conditionsNotCorrect NRC");

    // Unknown SID should return serviceNotSupported
    let req = [0xFFu8];
    let len = unsafe { (dcm_process)(req.as_ptr(), 1, resp.as_mut_ptr(), 256) };
    assert!(len >= 3);
    assert_eq!(resp[0], 0x7F);
    assert_eq!(resp[2], 0x11, "serviceNotSupported NRC");

    unsafe { (base_shutdown)() };
}
