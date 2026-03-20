//! Integration test: compile all Virtual-MCAL, vHsm adapter, and OS mapping
//! C code to verify API consistency and header compatibility (ADR-006).
//!
//! This test uses the `cc` crate to compile the C sources at test time.
//! If compilation succeeds, all `_Static_assert` checks pass and the
//! public APIs are structurally sound.

#![cfg(not(target_os = "windows"))]

use std::path::PathBuf;

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

#[test]
fn vmcal_headers_compile() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repo_root = manifest_dir.parent().unwrap().parent().unwrap();

    let abi_include = manifest_dir.join("include");
    let platform_include = repo_root.join("vecu-platform").join("include");
    let baselayer_include = repo_root.join("baselayer").join("include");
    let vmcal_include = repo_root.join("vmcal").join("include");
    let vhsm_include = repo_root.join("vhsm_adapter").join("include");
    let os_include = repo_root.join("os_mapping").join("include");

    let test_c = manifest_dir.join("tests").join("vmcal_compile_check.c");

    let out_dir = std::env::temp_dir().join("vecu_vmcal_compile_test");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let target = detect_target_triple();
    std::env::set_var("TARGET", &target);
    std::env::set_var("HOST", &target);
    std::env::set_var("OPT_LEVEL", "0");

    cc::Build::new()
        .file(&test_c)
        .include(&abi_include)
        .include(&platform_include)
        .include(&baselayer_include)
        .include(&vmcal_include)
        .include(&vhsm_include)
        .include(&os_include)
        .warnings(true)
        .extra_warnings(true)
        .flag_if_supported("-std=c11")
        .flag_if_supported("-pedantic")
        .out_dir(&out_dir)
        .try_compile("vmcal_compile_check")
        .expect("Virtual-MCAL / vHsm / OS-Mapping header compilation failed");
}

#[test]
fn vmcal_sources_compile() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repo_root = manifest_dir.parent().unwrap().parent().unwrap();

    let abi_include = manifest_dir.join("include");
    let platform_include = repo_root.join("vecu-platform").join("include");
    let baselayer_include = repo_root.join("baselayer").join("include");
    let vmcal_include = repo_root.join("vmcal").join("include");
    let vmcal_src = repo_root.join("vmcal").join("src");
    let vhsm_include = repo_root.join("vhsm_adapter").join("include");
    let vhsm_src = repo_root.join("vhsm_adapter").join("src");
    let os_include = repo_root.join("os_mapping").join("include");
    let os_src = repo_root.join("os_mapping").join("src");

    let out_dir = std::env::temp_dir().join("vecu_vmcal_sources_test");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let target = detect_target_triple();
    std::env::set_var("TARGET", &target);
    std::env::set_var("HOST", &target);
    std::env::set_var("OPT_LEVEL", "0");

    cc::Build::new()
        .file(vmcal_src.join("VMcal_Context.c"))
        .file(vmcal_src.join("Can.c"))
        .file(vmcal_src.join("Eth.c"))
        .file(vmcal_src.join("Fr.c"))
        .file(vmcal_src.join("Dio.c"))
        .file(vmcal_src.join("Port.c"))
        .file(vmcal_src.join("Spi.c"))
        .file(vmcal_src.join("Gpt.c"))
        .file(vmcal_src.join("Mcu.c"))
        .file(vmcal_src.join("Fls.c"))
        .file(vmcal_src.join("Adc.c"))
        .file(vmcal_src.join("Pwm.c"))
        .file(vmcal_src.join("Wdg.c"))
        .file(vmcal_src.join("Lin.c"))
        .file(vmcal_src.join("Icu.c"))
        .file(vhsm_src.join("Crypto_30_vHsm.c"))
        .file(os_src.join("Os_Mapping.c"))
        .include(&abi_include)
        .include(&platform_include)
        .include(&baselayer_include)
        .include(&vmcal_include)
        .include(&vhsm_include)
        .include(&os_include)
        .warnings(true)
        .extra_warnings(true)
        .flag_if_supported("-std=c11")
        .flag_if_supported("-pedantic")
        .out_dir(&out_dir)
        .try_compile("vmcal_all_sources")
        .expect("Virtual-MCAL / vHsm / OS-Mapping source compilation failed");
}
