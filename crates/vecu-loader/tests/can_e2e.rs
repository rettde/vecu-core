//! CAN End-to-End integration test for the MICROSAR vECU.
//!
//! Verifies the CAN RX path: frame injection → vmcal `Can_MainFunction_Read`
//! → `CanIf_RxIndication` → CanTp/PduR → (optional Dcm response on TX).
//!
//! **Requires** the pre-built MICROSAR dylib at
//! `examples/zc_d_vecu/build/libzc_d_c0_vecu.dylib`.
//! The test is skipped when the dylib does not exist (CI without `CMake` build).

#![cfg(not(target_os = "windows"))]

use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Mutex;

use vecu_abi::{BusType, VecuFrame};

fn workspace_root() -> PathBuf {
    let manifest = Path::new(env!("CARGO_MANIFEST_DIR"));
    manifest.parent().unwrap().parent().unwrap().to_path_buf()
}

fn microsar_dylib() -> PathBuf {
    workspace_root().join("examples/zc_d_vecu/build/libzc_d_c0_vecu.dylib")
}

fn appl_dylib() -> PathBuf {
    workspace_root().join("target/debug/libvecu_appl.dylib")
}

static DET_ERROR_COUNT: AtomicU32 = AtomicU32::new(0);
static LOG_MESSAGES: Mutex<Vec<String>> = Mutex::new(Vec::new());

#[allow(unsafe_code)]
unsafe extern "C" fn test_log_fn(level: i32, msg: *const core::ffi::c_char) {
    if msg.is_null() {
        return;
    }
    let s = unsafe { std::ffi::CStr::from_ptr(msg) }
        .to_string_lossy()
        .into_owned();
    if s.contains("Det_ReportError") || s.contains("DET") || s.contains("Det ERROR") || s.contains("Det RUNTIME") {
        DET_ERROR_COUNT.fetch_add(1, Ordering::Relaxed);
    }
    if let Ok(mut log) = LOG_MESSAGES.lock() {
        log.push(format!("[L{level}] {s}"));
    }
}

#[test]
fn can_rx_path_no_crash_no_det_errors() {
    let dylib = microsar_dylib();
    if !dylib.exists() {
        eprintln!(
            "SKIP: MICROSAR dylib not found at {}",
            dylib.display()
        );
        return;
    }
    let appl = appl_dylib();
    if !appl.exists() {
        eprintln!(
            "SKIP: vecu-appl dylib not found at {}",
            appl.display()
        );
        return;
    }

    DET_ERROR_COUNT.store(0, Ordering::SeqCst);
    if let Ok(mut log) = LOG_MESSAGES.lock() {
        log.clear();
    }

    let dylib_str = dylib.to_str().unwrap();
    std::env::set_var("VECU_BASE_LIB", dylib_str);
    std::env::set_var("VECU_APPL_LIB", dylib_str);

    let plugin = vecu_loader::LoadedPlugin::load(&appl).expect("load vecu-appl");
    let layout = vecu_shm::ShmLayout {
        queue_capacity: 64,
        vars_size: 4096,
    };
    let shm = vecu_shm::SharedMemory::with_layout(layout);
    shm.validate().expect("SHM valid");

    let mut runtime = vecu_runtime::Runtime::new(shm);
    runtime.set_appl(vecu_runtime::PluginSlot::new(plugin.into_api()));

    let (shm_base, shm_size) = runtime.shm_mut().raw_parts();
    let ctx = vecu_abi::VecuRuntimeContext {
        shm_base,
        shm_size,
        pad0: 0,
        tick_interval_us: 1000,
        log_fn: Some(test_log_fn),
        hsm_api: core::ptr::null(),
    };

    runtime.init_all(&ctx).expect("init_all");

    runtime.run(200).expect("warm-up 200 ticks");

    let obd_can_id: u32 = 0x8000_0000 | 0x18DB_33F1;
    let tester_present_sf: [u8; 8] = [0x02, 0x3E, 0x00, 0x55, 0x55, 0x55, 0x55, 0x55];
    let rx_frame = VecuFrame::with_bus_data(
        BusType::Can,
        obd_can_id,
        &tester_present_sf,
        200,
    );
    runtime
        .shm_mut()
        .rx_push(&rx_frame)
        .expect("rx_push frame");

    runtime.run(100).expect("post-inject 100 ticks");

    let mut tx_frames = Vec::new();
    while let Some(frame) = runtime.shm_mut().tx_pop() {
        tx_frames.push(frame);
    }

    runtime.shutdown_all();

    let det_errors = DET_ERROR_COUNT.load(Ordering::SeqCst);
    let logs = LOG_MESSAGES.lock().unwrap();

    eprintln!("--- CAN E2E Test Results ---");
    eprintln!("Total ticks: 300 (200 warm-up + 100 post-inject)");
    eprintln!("Det errors: {det_errors}");
    eprintln!("TX frames collected: {}", tx_frames.len());
    eprintln!("Log messages: {}", logs.len());
    for msg in logs.iter() {
        eprintln!("  {msg}");
    }
    if !tx_frames.is_empty() {
        for (i, f) in tx_frames.iter().enumerate() {
            eprintln!(
                "  TX[{i}]: id=0x{:08X} len={} bus={} data={:02X?}",
                f.id,
                f.len,
                f.bus_type,
                &f.data[..f.len as usize]
            );
        }
    }

    let rx_reached_canif = logs
        .iter()
        .any(|m| m.contains("RX id=0x98DB33F1"));
    let controllers_started = logs
        .iter()
        .any(|m| m.contains("CAN controllers force-started"));

    assert_eq!(
        det_errors, 0,
        "Expected zero Det errors, got {det_errors}. Check logs above."
    );
    assert!(
        controllers_started,
        "CAN controllers were not force-started during init."
    );
    assert!(
        rx_reached_canif,
        "Injected CAN frame (0x98DB33F1) did not reach CanIf_RxIndication."
    );
}
