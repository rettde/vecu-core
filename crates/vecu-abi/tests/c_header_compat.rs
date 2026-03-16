//! Integration test: compile the C headers and verify ABI compatibility.
//!
//! Uses the `cc` crate to compile `abi_compat.c` at test time, then
//! checks that the `_Static_assert` checks in the C file pass
//! (i.e. the compilation succeeds).

use std::path::PathBuf;

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

/// Compile `abi_compat.c` with the C headers and verify it succeeds.
///
/// The C file contains `_Static_assert` checks that fail at compile time
/// if any layout mismatch exists.  If this test passes, the C headers
/// are ABI‑compatible with the Rust structs.
#[test]
fn c_headers_compile_and_match_rust_layout() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let include_dir = manifest_dir.join("include");
    let test_c = manifest_dir.join("tests").join("abi_compat.c");

    let out_dir = std::env::temp_dir().join("vecu_abi_compat_test");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    // The cc crate requires TARGET/HOST/OPT_LEVEL env vars which are
    // only set automatically during build.rs.  Detect and set them here.
    let target = detect_target_triple();
    std::env::set_var("TARGET", &target);
    std::env::set_var("HOST", &target);
    std::env::set_var("OPT_LEVEL", "0");

    cc::Build::new()
        .file(&test_c)
        .include(&include_dir)
        .warnings(true)
        .extra_warnings(true)
        .flag_if_supported("-std=c11")
        .flag_if_supported("-pedantic")
        .flag_if_supported("/std:c11")
        .out_dir(&out_dir)
        .try_compile("abi_compat")
        .expect("C header ABI compatibility check failed — layout mismatch");

    // If we get here the C _Static_assert checks all passed.
    // Additionally verify from the Rust side:
    assert_eq!(
        core::mem::size_of::<vecu_abi::VecuFrame>(),
        1560,
        "Rust VecuFrame size must be 1560"
    );
}
