//! Integration test: compile mock C libraries, set env vars, and verify
//! bridge mode works end-to-end via the vecu-appl ABI.

use std::path::PathBuf;
use std::sync::Mutex;

/// Serialize bridge tests — they mutate process-wide env vars and module statics.
static LOCK: Mutex<()> = Mutex::new(());

/// Detect the current host target triple from `rustc -vV`.
fn detect_target_triple() -> String {
    let output = std::process::Command::new("rustc")
        .args(["-vV"])
        .output()
        .expect("rustc must be available");
    let stdout = String::from_utf8_lossy(&output.stdout);
    for line in stdout.lines() {
        if let Some(host) = line.strip_prefix("host: ") {
            return host.trim().to_string();
        }
    }
    panic!("could not determine host triple from `rustc -vV`");
}

/// Shared library extension for the current platform.
fn dylib_ext() -> &'static str {
    if cfg!(target_os = "macos") {
        "dylib"
    } else if cfg!(target_os = "windows") {
        "dll"
    } else {
        "so"
    }
}

/// Compile a C file into a shared library, returning the path to it.
fn compile_mock_lib(name: &str, source: &str, out_dir: &std::path::Path) -> PathBuf {
    let target = detect_target_triple();
    std::env::set_var("TARGET", &target);
    std::env::set_var("HOST", &target);
    std::env::set_var("OPT_LEVEL", "0");

    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let src_path = manifest_dir.join("tests").join(source);

    // Use cc to compile the object file
    cc::Build::new()
        .file(&src_path)
        .pic(true)
        .warnings(true)
        .out_dir(out_dir)
        .try_compile(name)
        .unwrap_or_else(|e| panic!("failed to compile {source}: {e}"));

    // The cc crate produces a static archive. We need a shared library.
    // Compile manually with the system C compiler instead.
    let lib_path = out_dir.join(format!("lib{name}.{}", dylib_ext()));

    let mut cmd = std::process::Command::new("cc");
    cmd.arg("-shared").arg("-o").arg(&lib_path).arg(&src_path);

    if cfg!(target_os = "macos") {
        cmd.arg("-dynamiclib");
    } else {
        cmd.arg("-fPIC");
    }

    let output = cmd.output().expect("failed to run cc");
    assert!(
        output.status.success(),
        "cc failed for {source}:\n{}",
        String::from_utf8_lossy(&output.stderr)
    );

    assert!(
        lib_path.exists(),
        "shared library not found: {}",
        lib_path.display()
    );
    lib_path
}

#[test]
fn bridge_mode_loads_and_calls_c_libraries() {
    let _g = LOCK.lock().unwrap();

    let out_dir = std::env::temp_dir().join("vecu_appl_bridge_test");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let base_lib = compile_mock_lib("mockbase", "mock_base.c", &out_dir);
    let appl_lib = compile_mock_lib("mockappl", "mock_appl.c", &out_dir);

    // Set env vars so bridge mode activates
    std::env::set_var("VECU_BASE_LIB", base_lib.to_str().unwrap());
    std::env::set_var("VECU_APPL_LIB", appl_lib.to_str().unwrap());

    // Get the plugin API
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

    // Step a few ticks
    for tick in 0..5 {
        #[allow(unsafe_code)]
        let rc = unsafe { (api.step.unwrap())(tick) };
        assert_eq!(rc, vecu_abi::status::OK);
    }

    // Shutdown
    (api.shutdown.unwrap())();

    // Clear env vars
    std::env::remove_var("VECU_BASE_LIB");
    std::env::remove_var("VECU_APPL_LIB");
}

#[test]
fn echo_mode_still_works_without_env_vars() {
    let _g = LOCK.lock().unwrap();

    // Ensure bridge env vars are not set
    std::env::remove_var("VECU_BASE_LIB");
    std::env::remove_var("VECU_APPL_LIB");

    let mut api = vecu_abi::VecuPluginApi::zeroed();
    #[allow(unsafe_code)]
    let rc = unsafe { vecu_appl::vecu_get_api(vecu_abi::ABI_VERSION, &mut api) };
    assert_eq!(rc, vecu_abi::status::OK);

    let ctx = vecu_abi::VecuRuntimeContext {
        shm_base: core::ptr::null_mut(),
        shm_size: 0,
        pad0: 0,
        tick_interval_us: 1000,
        log_fn: None,
    };

    #[allow(unsafe_code)]
    unsafe {
        (api.init.unwrap())(&ctx);

        // Push a frame
        let frame = vecu_abi::VecuFrame::with_data(0x42, &[0xAA, 0xBB], 0);
        let rc = (api.push_frame.unwrap())(&frame);
        assert_eq!(rc, vecu_abi::status::OK);

        // Step — echo mode stamps the tick
        let rc = (api.step.unwrap())(99);
        assert_eq!(rc, vecu_abi::status::OK);

        // Poll — should get echoed frame
        let mut out = vecu_abi::VecuFrame::new(0);
        let rc = (api.poll_frame.unwrap())(&mut out);
        assert_eq!(rc, vecu_abi::status::OK);
        assert_eq!(out.id, 0x42);
        assert_eq!(out.timestamp, 99);
        assert_eq!(out.payload(), &[0xAA, 0xBB]);
    }

    (api.shutdown.unwrap())();
}
