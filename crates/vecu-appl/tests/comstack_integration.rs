//! Integration test: verify communication stack signal round-trip.
//!
//! Compiles the `BaseLayer` with P4 comm stack, loads it via `libloading`,
//! injects a CAN frame, and verifies `Com_ReceiveSignal` returns the correct
//! value.  Also verifies `Com_SendSignal` produces the correct outbound frame
//! via `push_tx_frame`.

#![allow(unsafe_code)]

use std::path::PathBuf;
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());

// ---------------------------------------------------------------------------
// FFI type aliases (at module level for clippy::items_after_statements)
// ---------------------------------------------------------------------------

type BaseInitFn = unsafe extern "C" fn(*const TestBaseContext);
type BaseStepFn = unsafe extern "C" fn(u64);
type BaseShutdownFn = unsafe extern "C" fn();
type ComReceiveSignalFn = unsafe extern "C" fn(u16, *mut u8) -> u8;
type ComSendSignalFn = unsafe extern "C" fn(u16, *const u8) -> u8;

// ---------------------------------------------------------------------------
// Mirror of vecu_base_context_t (must match vecu_base_context.h exactly)
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
// Callback state
// ---------------------------------------------------------------------------

static RX_FRAME: Mutex<Option<TestVecuFrame>> = Mutex::new(None);
static TX_FRAME: Mutex<Option<TestVecuFrame>> = Mutex::new(None);

unsafe extern "C" fn test_push_tx(frame: *const TestVecuFrame) -> i32 {
    if frame.is_null() { return -2; }
    let f = unsafe { &*frame };
    let mut copy = TestVecuFrame {
        id: f.id,
        len: f.len,
        bus_type: f.bus_type,
        pad0: f.pad0,
        data: [0u8; 1536],
        timestamp: f.timestamp,
    };
    copy.data[..f.len as usize].copy_from_slice(&f.data[..f.len as usize]);
    *TX_FRAME.lock().unwrap() = Some(copy);
    0 // VECU_OK
}

unsafe extern "C" fn test_pop_rx(frame: *mut TestVecuFrame) -> i32 {
    if frame.is_null() { return -2; }
    let mut guard = RX_FRAME.lock().unwrap();
    if let Some(rx) = guard.take() {
        let out = unsafe { &mut *frame };
        out.id = rx.id;
        out.len = rx.len;
        out.bus_type = rx.bus_type;
        out.pad0 = rx.pad0;
        out.data = rx.data;
        out.timestamp = rx.timestamp;
        0 // VECU_OK
    } else {
        -4 // VECU_NOT_SUPPORTED (no frame available)
    }
}

unsafe extern "C" fn test_log(_level: u32, _msg: *const std::ffi::c_char) {
    // discard
}

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

    let lib_path = out_dir.join(format!("libbase_com.{}", dylib_ext()));

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

    assert!(lib_path.exists(), "libbase_com not found: {}", lib_path.display());
    lib_path
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/// Signal IDs matching the default config in `Base_Entry.c`:
/// 0 = `VehicleSpeed` (16-bit LE, PDU 0x100, RX)
/// 1 = `EngineRpm`    (16-bit BE, PDU 0x101, RX)
/// 2 = `BrakeActive`  (1-bit  LE, PDU 0x100, bit 16, RX)
/// 3 = `TxSignal`     (8-bit  LE, PDU 0x200, TX)

#[test]
fn comstack_rx_signal_le_round_trip() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_comstack_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let lib_path = compile_baselayer(&out_dir);

    // Load the BaseLayer
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load BaseLayer");

    // Resolve symbols
    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_step: libloading::Symbol<BaseStepFn> =
        unsafe { lib.get(b"Base_Step") }.expect("Base_Step");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let com_receive: libloading::Symbol<ComReceiveSignalFn> =
        unsafe { lib.get(b"Com_ReceiveSignal") }.expect("Com_ReceiveSignal");

    // Build context
    let ctx = TestBaseContext {
        push_tx_frame: Some(test_push_tx),
        pop_rx_frame: Some(test_pop_rx),
        hsm_encrypt: None,
        hsm_decrypt: None,
        hsm_generate_mac: None,
        hsm_verify_mac: None,
        hsm_seed: None,
        hsm_key: None,
        hsm_rng: None,
        shm_vars: std::ptr::null_mut(),
        shm_vars_size: 0,
        _pad0: 0,
        log_fn: Some(test_log),
        tick_interval_us: 1000,
    };

    // Init
    unsafe { (base_init)(&ctx) };

    // Inject RX CAN frame for PDU 0x100 with VehicleSpeed = 0x1234 (LE: [0x34, 0x12])
    {
        let mut frame = TestVecuFrame {
            id: 0x100,      // PDU ID (CanIf passes frame.id as PduId)
            len: 8,
            bus_type: 0,    // VECU_BUS_CAN
            pad0: 0,
            data: [0u8; 1536],
            timestamp: 0,
        };
        frame.data[0] = 0x34; // VehicleSpeed LSB
        frame.data[1] = 0x12; // VehicleSpeed MSB
        frame.data[2] = 0x01; // BrakeActive = 1 (bit 16)
        *RX_FRAME.lock().unwrap() = Some(frame);
    }

    // Step — processes CanIf_RxMainFunction -> PduR -> Com_RxIndication
    unsafe { (base_step)(1) };

    // Read VehicleSpeed (signal 0)
    let mut value: u32 = 0;
    let rc = unsafe {
        (com_receive)(0, std::ptr::addr_of_mut!(value).cast::<u8>())
    };
    assert_eq!(rc, 0, "Com_ReceiveSignal for VehicleSpeed failed");
    assert_eq!(value, 0x1234, "VehicleSpeed should be 0x1234 (LE)");

    // Read BrakeActive (signal 2, 1-bit at bit 16)
    let mut brake: u32 = 0;
    let rc = unsafe {
        (com_receive)(2, std::ptr::addr_of_mut!(brake).cast::<u8>())
    };
    assert_eq!(rc, 0, "Com_ReceiveSignal for BrakeActive failed");
    assert_eq!(brake, 1, "BrakeActive should be 1");

    // Shutdown
    unsafe { (base_shutdown)() };

    // Clean up
    *RX_FRAME.lock().unwrap() = None;
    *TX_FRAME.lock().unwrap() = None;
}

#[test]
fn comstack_rx_signal_be_round_trip() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_comstack_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load BaseLayer");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_step: libloading::Symbol<BaseStepFn> =
        unsafe { lib.get(b"Base_Step") }.expect("Base_Step");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let com_receive: libloading::Symbol<ComReceiveSignalFn> =
        unsafe { lib.get(b"Com_ReceiveSignal") }.expect("Com_ReceiveSignal");

    let ctx = TestBaseContext {
        push_tx_frame: Some(test_push_tx),
        pop_rx_frame: Some(test_pop_rx),
        hsm_encrypt: None, hsm_decrypt: None, hsm_generate_mac: None,
        hsm_verify_mac: None, hsm_seed: None, hsm_key: None, hsm_rng: None,
        shm_vars: std::ptr::null_mut(), shm_vars_size: 0, _pad0: 0,
        log_fn: Some(test_log), tick_interval_us: 1000,
    };

    unsafe { (base_init)(&ctx) };

    // Inject RX CAN frame for PDU 0x101 with EngineRpm = 0xABCD (BE)
    // Big-endian packing: MSBit at bit_position=0.
    // For 16-bit BE at bit 0: value bits [15..0] map to physical bits [0..15].
    // Bit 0 (phys) = value bit 15. So byte[0] bit 0 = MSBit of 0xABCD = 1.
    // Actually, our BE pack stores MSBit at bit_pos going forward,
    // so physical bit layout is: bit0=MSBit, bit1=next, ...
    // For 0xABCD = 0b1010_1011_1100_1101:
    //   pack_signal_be: i=0 -> val_bit=15 -> bit_index=0 -> byte[0] bit 0 = bit 15 of value = 1
    //   i=1 -> val_bit=14 -> bit_index=1 -> byte[0] bit 1 = bit 14 = 0
    //   ...
    // This is equivalent to bit-reversing within each group. Let me just compute
    // the expected bytes and inject them directly.
    //
    // Simpler: just use pack_signal_be logic to compute expected bytes,
    // then inject those bytes and verify Com_ReceiveSignal returns 0xABCD.
    {
        let mut frame = TestVecuFrame {
            id: 0x101,
            len: 8,
            bus_type: 0,
            pad0: 0,
            data: [0u8; 1536],
            timestamp: 0,
        };
        // Pack 0xABCD using our BE convention:
        // MSBit of value at bit_position, going forward.
        let value: u32 = 0xABCD;
        let bit_len: u16 = 16;
        for i in 0..bit_len {
            let val_bit = bit_len - 1 - i;
            let bit_index = i;
            let byte_index = (bit_index / 8) as usize;
            let bit_in_byte = (bit_index % 8) as u8;
            if (value >> val_bit) & 1 != 0 {
                frame.data[byte_index] |= 1 << bit_in_byte;
            }
        }
        *RX_FRAME.lock().unwrap() = Some(frame);
    }

    unsafe { (base_step)(1) };

    let mut value: u32 = 0;
    let rc = unsafe {
        (com_receive)(1, std::ptr::addr_of_mut!(value).cast::<u8>())
    };
    assert_eq!(rc, 0, "Com_ReceiveSignal for EngineRpm failed");
    assert_eq!(value, 0xABCD, "EngineRpm should be 0xABCD (BE)");

    unsafe { (base_shutdown)() };

    *RX_FRAME.lock().unwrap() = None;
    *TX_FRAME.lock().unwrap() = None;
}

#[test]
fn comstack_tx_signal_produces_frame() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_comstack_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let lib_path = compile_baselayer(&out_dir);
    let lib = unsafe { libloading::Library::new(&lib_path) }
        .expect("failed to load BaseLayer");

    let base_init: libloading::Symbol<BaseInitFn> =
        unsafe { lib.get(b"Base_Init") }.expect("Base_Init");
    let base_step: libloading::Symbol<BaseStepFn> =
        unsafe { lib.get(b"Base_Step") }.expect("Base_Step");
    let base_shutdown: libloading::Symbol<BaseShutdownFn> =
        unsafe { lib.get(b"Base_Shutdown") }.expect("Base_Shutdown");
    let com_send: libloading::Symbol<ComSendSignalFn> =
        unsafe { lib.get(b"Com_SendSignal") }.expect("Com_SendSignal");

    let ctx = TestBaseContext {
        push_tx_frame: Some(test_push_tx),
        pop_rx_frame: Some(test_pop_rx),
        hsm_encrypt: None, hsm_decrypt: None, hsm_generate_mac: None,
        hsm_verify_mac: None, hsm_seed: None, hsm_key: None, hsm_rng: None,
        shm_vars: std::ptr::null_mut(), shm_vars_size: 0, _pad0: 0,
        log_fn: Some(test_log), tick_interval_us: 1000,
    };

    // Clear TX
    *TX_FRAME.lock().unwrap() = None;
    *RX_FRAME.lock().unwrap() = None;

    unsafe { (base_init)(&ctx) };

    // Send TxSignal (signal 3, 8-bit LE at bit 0 of PDU 0x200, TX)
    let value: u8 = 0x42;
    let rc = unsafe {
        (com_send)(3, std::ptr::addr_of!(value))
    };
    assert_eq!(rc, 0, "Com_SendSignal for TxSignal failed");

    // Step triggers Com_MainFunction -> PduR -> CanIf -> push_tx_frame
    unsafe { (base_step)(1) };

    // Verify push_tx_frame received the correct frame
    let guard = TX_FRAME.lock().unwrap();
    let tx = guard.as_ref().expect("push_tx_frame should have been called");
    assert_eq!(tx.id, 0x200, "TX frame ID should be PDU ID 0x200");
    assert_eq!(tx.bus_type, 0, "TX frame should be CAN");
    assert_eq!(tx.data[0], 0x42, "TX frame data[0] should be 0x42");

    drop(guard);

    unsafe { (base_shutdown)() };

    *TX_FRAME.lock().unwrap() = None;
}
