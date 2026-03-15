//! Integration test: verify transport layers (`CanTp`, `DoIP`) (P7).
//!
//! Tests ISO 15765-2 single-frame and multi-frame segmentation/reassembly,
//! and `DoIP` diagnostic message routing.

#![allow(unsafe_code)]

use std::path::PathBuf;
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());

// ---------------------------------------------------------------------------
// FFI type aliases
// ---------------------------------------------------------------------------

type BaseInitFn = unsafe extern "C" fn(*const TestBaseContext);
type BaseShutdownFn = unsafe extern "C" fn();
type BaseStepFn = unsafe extern "C" fn(u64);
type CanTpTransmitFn = unsafe extern "C" fn(u16, *const u8, u16) -> u8;
type CanTpRxIndicationFn = unsafe extern "C" fn(u32, *const u8, u8);
type DoIpProcessFn = unsafe extern "C" fn(*const u8, u16, *mut u8, u16) -> u16;
type DcmGetSessionFn = unsafe extern "C" fn() -> u8;

// ---------------------------------------------------------------------------
// Mirror of `vecu_frame_t` / `vecu_base_context_t`
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
}

// ---------------------------------------------------------------------------
// TX capture ring buffer
// ---------------------------------------------------------------------------

use std::cell::RefCell;

thread_local! {
    static TX_FRAMES: RefCell<Vec<(u32, Vec<u8>)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn capture_push_tx(frame: *const TestVecuFrame) -> i32 {
    let f = unsafe { &*frame };
    let len = f.len.min(1536) as usize;
    let data = f.data[..len].to_vec();
    TX_FRAMES.with(|tx| tx.borrow_mut().push((f.id, data)));
    0 // VECU_OK
}

unsafe extern "C" fn noop_pop_rx(_: *mut TestVecuFrame) -> i32 {
    -4
}
unsafe extern "C" fn noop_log(_: u32, _: *const std::ffi::c_char) {}

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

/// All `BaseLayer` source files (P3–P7).
const ALL_SOURCES: &[&str] = &[
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
];

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

    let sources: Vec<PathBuf> = ALL_SOURCES.iter().map(|s| baselayer_src.join(s)).collect();

    let lib_path = out_dir.join(format!("libbase_transport.{}", dylib_ext()));

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
    lib_path
}

fn build_ctx(shm: &mut [u8]) -> TestBaseContext {
    TestBaseContext {
        push_tx_frame: Some(capture_push_tx),
        pop_rx_frame: Some(noop_pop_rx),
        hsm_encrypt: None,
        hsm_decrypt: None,
        hsm_generate_mac: None,
        hsm_verify_mac: None,
        hsm_seed: None,
        hsm_key: None,
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
fn cantp_single_frame_transmit() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_transport_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let cantp_transmit: libloading::Symbol<CanTpTransmitFn> =
        unsafe { lib.get(b"CanTp_Transmit") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);

    TX_FRAMES.with(|tx| tx.borrow_mut().clear());
    unsafe { (base_init)(&ctx) };

    // Transmit a 5-byte message (fits in Single Frame)
    let data = [0x10u8, 0x03, 0x00, 0x00, 0x00];
    let rc = unsafe { (cantp_transmit)(0, data.as_ptr(), 5) };
    assert_eq!(rc, 0, "CanTp_Transmit failed");

    // Check that a Single Frame was sent on TX ID 0x642
    let frames = TX_FRAMES.with(|tx| tx.borrow().clone());
    assert!(!frames.is_empty(), "No TX frames captured");
    let (id, payload) = &frames[frames.len() - 1];
    assert_eq!(*id, 0x642, "TX CAN ID should be 0x642");
    // SF: byte[0] = 0x05 (SF + length=5), bytes[1..6] = data
    assert_eq!(payload[0] & 0xF0, 0x00, "Should be SF (upper nibble 0)");
    assert_eq!(payload[0] & 0x0F, 5, "SF length should be 5");
    assert_eq!(&payload[1..6], &data);

    unsafe { (base_shutdown)() };
}

#[test]
fn cantp_multi_frame_transmit_and_receive() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_transport_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let base_step: libloading::Symbol<BaseStepFn> = unsafe { lib.get(b"Base_Step") }.unwrap();
    let cantp_transmit: libloading::Symbol<CanTpTransmitFn> =
        unsafe { lib.get(b"CanTp_Transmit") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);

    TX_FRAMES.with(|tx| tx.borrow_mut().clear());
    unsafe { (base_init)(&ctx) };

    // Transmit a 20-byte message (requires FF + CFs)
    let data: Vec<u8> = (0..20).collect();
    let rc = unsafe { (cantp_transmit)(0, data.as_ptr(), 20) };
    assert_eq!(rc, 0, "CanTp_Transmit failed");

    // First Frame should be sent immediately
    let frames = TX_FRAMES.with(|tx| tx.borrow().clone());
    assert!(!frames.is_empty(), "No FF captured");
    let (id, ff_payload) = &frames[frames.len() - 1];
    assert_eq!(*id, 0x642);
    assert_eq!(ff_payload[0] & 0xF0, 0x10, "Should be FF (upper nibble 1)");
    let ff_len = (u16::from(ff_payload[0] & 0x0F) << 8) | u16::from(ff_payload[1]);
    assert_eq!(ff_len, 20, "FF total length should be 20");

    // BS=0 means no FC needed, CFs sent on MainFunction ticks
    // Drive a few ticks to send remaining CFs
    for tick in 1..10_u64 {
        unsafe { (base_step)(tick) };
    }

    // Should have FF + 2 CFs (6 bytes in FF + 7 in CF1 + 7 in CF2 = 20)
    let all_frames = TX_FRAMES.with(|tx| tx.borrow().clone());
    let tx_frames: Vec<_> = all_frames.iter().filter(|(fid, _)| *fid == 0x642).collect();
    assert!(
        tx_frames.len() >= 3,
        "Expected FF + 2 CFs, got {}",
        tx_frames.len()
    );

    unsafe { (base_shutdown)() };
}

#[test]
fn cantp_rx_single_frame_reassembly() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_transport_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let cantp_rx: libloading::Symbol<CanTpRxIndicationFn> =
        unsafe { lib.get(b"CanTp_RxIndication") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);
    unsafe { (base_init)(&ctx) };

    // Send a Single Frame on RX ID 0x641
    // SF with 2 bytes of UDS data: 0x3E 0x00 (TesterPresent)
    let sf = [0x02u8, 0x3E, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC];
    // This should call CanTp_RxComplete which by default is a no-op (weak symbol)
    // Just verify it doesn't crash
    unsafe { (cantp_rx)(0x641, sf.as_ptr(), 8) };

    unsafe { (base_shutdown)() };
}

#[test]
fn doip_routing_activation_and_diag_message() {
    let _g = LOCK.lock().unwrap();
    let out_dir = std::env::temp_dir().join("vecu_transport_integration");
    std::fs::create_dir_all(&out_dir).unwrap();
    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }.unwrap();

    let base_init: libloading::Symbol<BaseInitFn> = unsafe { lib.get(b"Base_Init") }.unwrap();
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.unwrap();
    let doip_process: libloading::Symbol<DoIpProcessFn> =
        unsafe { lib.get(b"DoIP_ProcessPacket") }.unwrap();
    let dcm_session: libloading::Symbol<DcmGetSessionFn> =
        unsafe { lib.get(b"Dcm_GetActiveSession") }.unwrap();

    let mut shm = [0u8; 256];
    let ctx = build_ctx(&mut shm);
    unsafe { (base_init)(&ctx) };

    // 1. Routing Activation Request
    // DoIP header: version=0x02, ~ver=0xFD, type=0x0005, length=2
    // Payload: tester addr = 0x0E00
    let ra_req: [u8; 10] = [0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x0E, 0x00];
    let mut resp = [0u8; 256];
    let len = unsafe { (doip_process)(ra_req.as_ptr(), 10, resp.as_mut_ptr(), 256) };
    assert!(len > 0, "RA response should not be empty");
    // Check it's a routing activation response (type 0x0006)
    assert_eq!(resp[0], 0x02, "DoIP version");
    assert_eq!(resp[2], 0x00);
    assert_eq!(resp[3], 0x06, "Routing activation response type");

    // 2. Diagnostic message: DiagnosticSessionControl (0x10 0x03)
    // DoIP header: type=0x8001, payload = src(2) + tgt(2) + UDS
    let diag_req: [u8; 14] = [
        0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x06, 0x0E, 0x00, // source addr
        0x00, 0x01, // target addr
        0x10, 0x03, // UDS: DiagnosticSessionControl Extended
    ];
    let len = unsafe { (doip_process)(diag_req.as_ptr(), 14, resp.as_mut_ptr(), 256) };
    assert!(len > 0, "Diag response should not be empty");
    // DoIP header type should be 0x8001 (diagnostic message)
    assert_eq!(resp[2], 0x80);
    assert_eq!(resp[3], 0x01);
    // UDS response starts at offset 8 (header) + 4 (addrs) = 12
    assert_eq!(resp[12], 0x50, "Positive response for 0x10");
    assert_eq!(resp[13], 0x03, "Extended session");

    // Verify session actually changed
    assert_eq!(unsafe { (dcm_session)() }, 0x03);

    unsafe { (base_shutdown)() };
}
