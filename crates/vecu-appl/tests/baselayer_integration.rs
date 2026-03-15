//! Integration test: compile the real `BaseLayer` + mock application,
//! load them via bridge mode, and verify end-to-end lifecycle.

use std::path::PathBuf;
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());

fn dylib_ext() -> &'static str {
    if cfg!(target_os = "macos") {
        "dylib"
    } else if cfg!(target_os = "windows") {
        "dll"
    } else {
        "so"
    }
}

/// Compile the real `BaseLayer` from baselayer/src/ into a shared library.
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
    ]
    .iter()
    .map(|s| baselayer_src.join(s))
    .collect();

    let lib_path = out_dir.join(format!("libbase.{}", dylib_ext()));

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
        "BaseLayer compilation failed:\n{}",
        String::from_utf8_lossy(&output.stderr)
    );

    assert!(lib_path.exists(), "libbase not found: {}", lib_path.display());
    lib_path
}

/// Compile the mock application into a shared library.
fn compile_mock_appl(out_dir: &std::path::Path) -> PathBuf {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let src_path = manifest_dir.join("tests").join("mock_appl_minimal.c");
    let lib_path = out_dir.join(format!("libmockappl.{}", dylib_ext()));

    let mut cmd = std::process::Command::new("cc");
    cmd.arg("-shared")
        .arg("-std=c11")
        .arg("-fPIC")
        .arg("-o")
        .arg(&lib_path)
        .arg(&src_path);

    if cfg!(target_os = "macos") {
        cmd.arg("-dynamiclib");
    }

    let output = cmd.output().expect("failed to run cc");
    assert!(
        output.status.success(),
        "mock appl compilation failed:\n{}",
        String::from_utf8_lossy(&output.stderr)
    );

    assert!(lib_path.exists(), "libmockappl not found: {}", lib_path.display());
    lib_path
}

#[test]
fn baselayer_bridge_lifecycle_100_ticks() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_baselayer_integration");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let base_lib = compile_baselayer(&out_dir);
    let appl_lib = compile_mock_appl(&out_dir);

    std::env::set_var("VECU_BASE_LIB", base_lib.to_str().unwrap());
    std::env::set_var("VECU_APPL_LIB", appl_lib.to_str().unwrap());

    // Get API
    let mut api = vecu_abi::VecuPluginApi::zeroed();
    #[allow(unsafe_code)]
    let rc = unsafe { vecu_appl::vecu_get_api(vecu_abi::ABI_VERSION, &mut api) };
    assert_eq!(rc, vecu_abi::status::OK);

    // Init
    let ctx = vecu_abi::VecuRuntimeContext {
        shm_base: core::ptr::null_mut(),
        shm_size: 0,
        pad0: 0,
        tick_interval_us: 1000,
        log_fn: None,
    };
    #[allow(unsafe_code)]
    let rc = unsafe { (api.init.unwrap())(&ctx) };
    assert_eq!(rc, vecu_abi::status::OK);

    // Run 100 ticks
    for tick in 0..100 {
        #[allow(unsafe_code)]
        let rc = unsafe { (api.step.unwrap())(tick) };
        assert_eq!(rc, vecu_abi::status::OK);
    }

    // Shutdown
    (api.shutdown.unwrap())();

    // Clean up env
    std::env::remove_var("VECU_BASE_LIB");
    std::env::remove_var("VECU_APPL_LIB");
}
