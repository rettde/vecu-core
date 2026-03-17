//! Integration test: compile, link, and run Virtual-MCAL functional tests.
//!
//! This test builds `vmcal_functional_test.c` together with the actual
//! Virtual-MCAL sources (Can.c, Eth.c, Fls.c, VMcal_Context.c) into an
//! executable, runs it, and asserts exit code 0.

#![cfg(not(target_os = "windows"))]

use std::path::PathBuf;
use std::process::Command;

#[test]
fn vmcal_functional_tests_run() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repo_root = manifest_dir.parent().unwrap().parent().unwrap();

    let abi_include = manifest_dir.join("include");
    let baselayer_include = repo_root.join("baselayer").join("include");
    let vmcal_include = repo_root.join("vmcal").join("include");
    let vmcal_src = repo_root.join("vmcal").join("src");

    let test_c = manifest_dir
        .join("tests")
        .join("vmcal_functional_test.c");

    let out_dir = std::env::temp_dir().join("vecu_vmcal_functional_test");
    std::fs::create_dir_all(&out_dir).expect("create temp dir");

    let exe_path = out_dir.join("vmcal_functional_test");

    let sources = vec![
        test_c.to_str().unwrap().to_string(),
        vmcal_src
            .join("VMcal_Context.c")
            .to_str()
            .unwrap()
            .to_string(),
        vmcal_src.join("Can.c").to_str().unwrap().to_string(),
        vmcal_src.join("Eth.c").to_str().unwrap().to_string(),
        vmcal_src.join("Fls.c").to_str().unwrap().to_string(),
    ];

    let mut cmd = Command::new("cc");
    cmd.arg("-std=c11")
        .arg("-Wall")
        .arg("-Wextra")
        .arg("-pedantic")
        .arg("-o")
        .arg(exe_path.to_str().unwrap())
        .arg(format!("-I{}", abi_include.to_str().unwrap()))
        .arg(format!("-I{}", baselayer_include.to_str().unwrap()))
        .arg(format!("-I{}", vmcal_include.to_str().unwrap()));

    for src in &sources {
        cmd.arg(src);
    }

    let compile_output = cmd
        .output()
        .expect("failed to invoke C compiler");

    if !compile_output.status.success() {
        let stderr = String::from_utf8_lossy(&compile_output.stderr);
        let stdout = String::from_utf8_lossy(&compile_output.stdout);
        panic!(
            "Compilation failed:\nstdout: {stdout}\nstderr: {stderr}"
        );
    }

    let run_output = Command::new(exe_path.to_str().unwrap())
        .output()
        .expect("failed to run functional test binary");

    let stdout = String::from_utf8_lossy(&run_output.stdout);
    let stderr = String::from_utf8_lossy(&run_output.stderr);

    if !run_output.status.success() {
        panic!(
            "Functional test failed (exit code {:?}):\nstdout: {stdout}\nstderr: {stderr}",
            run_output.status.code()
        );
    }

    assert!(
        stdout.contains("ALL PASSED"),
        "Expected 'ALL PASSED' in output, got:\nstdout: {stdout}\nstderr: {stderr}"
    );
}
