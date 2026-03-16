# AGENTS.md — AI Coding Assistant Guidelines

> This file documents project conventions for AI coding assistants
> (GitHub Copilot, Windsurf Cascade, Cursor, etc.) working on **vecu-core**.

---

## Project Overview

**vecu-core** is a deterministic virtual ECU runtime for executing AUTOSAR-based
ECU C-code on a host PC (Windows, Linux, macOS).  It is dual-licensed under
MIT OR Apache-2.0.

| Layer | Language | Standard | Location |
|-------|----------|----------|----------|
| Runtime + Plugins | Rust | Edition 2021, MSRV 1.75 | `crates/` |
| BaseLayer (24 BSW stubs) | C | C11, no external deps | `baselayer/` |
| Sample ECU application | C | C11 | `examples/sample_ecu/` |

---

## Rust Rules

- **clippy pedantic** is enforced at `deny` level — all new code must pass.
- `unsafe_code` is `deny` by default; use `#[allow(unsafe_code)]` only with
  justification and minimal scope.
- `missing_docs` is `warn` — document every public item.
- Always use **idiomatic Rust**: `thiserror` for errors, `serde` for
  serialization, `tracing` for logging, `bytemuck` for zero-copy casts.
- **No proprietary dependencies.** Only open-source crates from crates.io.
- Every public function must have unit tests.  Integration tests that compile
  C code are gated with `#![cfg(not(target_os = "windows"))]` because Windows
  CI uses MSVC (no `cc` command).
- Imports always at the top of the file.  No `use` statements mid-function.
- Prefer `Mutex<Option<T>>` for lazily-initialized module statics.

### Workspace Crates

| Crate | Type | Key Trait / Export |
|-------|------|--------------------|
| `vecu-abi` | lib | `VecuPluginApi`, `VecuFrame`, `BusType`, SHM layout |
| `vecu-shm` | lib | `SharedMemory` (anonymous + file-backed) |
| `vecu-runtime` | lib | `RuntimeAdapter` (when), `OpenSutApi` (where) |
| `vecu-appl` | cdylib+rlib | `vecu_get_api`, bridge loader for C libs |
| `vecu-hsm` | cdylib+rlib | AES-128, CMAC, SecurityAccess, 20-slot key store |
| `vecu-silkit` | lib | SIL Kit FFI: CAN/ETH/LIN/FlexRay controllers |
| `vecu-loader` | bin | CLI entry point, plugin loading, ABI negotiation |

---

## C Rules (BaseLayer & Sample ECU)

- **C11** standard, no compiler extensions (no `typeof`, no VLAs).
- `__attribute__((weak))` is the sole exception (used in `CanTp.c`).
- No external dependencies — only the C standard library.
- `EXPORT` macro for symbol visibility:
  - Windows: `__declspec(dllexport)`
  - Others: `__attribute__((visibility("default")))`
- All inter-module function calls must have explicit declarations (include
  the header or use `extern`).  Apple Clang 15+ treats implicit declarations
  as errors.
- `_Static_assert` is used in ABI headers — MSVC needs `/std:c11`.
- The `appl_ecu` shared library **must** link against `base` because macOS
  `ld64` requires all symbols resolved at link time.

### BaseLayer Architecture

```
vecu_base_context_t  (injected by Rust → C)
  ├── push_tx_frame / pop_rx_frame   (frame I/O)
  ├── hsm_encrypt / hsm_decrypt      (crypto delegation)
  ├── hsm_generate_mac / hsm_verify_mac
  ├── hsm_seed / hsm_key / hsm_rng
  ├── shm_vars / shm_vars_size       (shared memory)
  └── log_fn                          (tracing bridge)

Lifecycle:
  Base_Init(ctx) → EcuM_Init → 24 BSW modules init
  Base_Step(tick) → EcuM_MainFunction → SchM → all MainFunctions
  Base_Shutdown() → EcuM_GoSleep → reverse de-init
```

### BSW Modules (24)

System: EcuM, SchM, Os, Det  
Communication: Com, PduR, CanIf, EthIf, LinIf, FrIf  
Transport: CanTp (ISO 15765-2), DoIP (ISO 13400)  
Diagnostics: Dcm (9 UDS services), Dem, FiM  
Memory: NvM (SHM-backed), Fee, MemIf  
Crypto: Cry, CryIf, Csm (→ HSM callbacks)  
Safety: WdgM  
Runtime: Rte (project-specific stubs)

---

## Cross-Platform Requirements

All code must build and pass CI on **ubuntu-latest**, **macos-latest**,
and **windows-latest**.

| Concern | Linux | macOS | Windows |
|---------|-------|-------|---------|
| C compiler (CI) | `gcc` | Apple Clang 15+ | MSVC (`cl.exe`) |
| Shared lib extension | `.so` | `.dylib` | `.dll` |
| Linker behavior | unresolved symbols OK | must resolve all | must resolve all |
| C11 flag | `-std=c11` | `-std=c11` | `/std:c11` |

---

## ABI Contract

- `VecuFrame` is exactly **1560 bytes** (1536 B payload for Ethernet).
- `vecu_base_context_t` contains function pointers — never change field order.
- ABI version is `pack_version(major, minor)`.  Bump major for breaking changes.
- `_Static_assert` in `abi_compat.c` guards C↔Rust layout agreement.

---

## Testing

```bash
# Full test suite (Rust unit + integration)
cargo test --workspace

# Clippy (must be clean)
cargo clippy --workspace --all-targets -- -D warnings

# Format check
cargo fmt --all -- --check

# Build BaseLayer + Sample ECU (CMake)
cd examples/sample_ecu && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
```

---

## Do NOT

- Add proprietary dependencies or closed-source code.
- Delete or weaken existing tests without explicit direction.
- Add `unsafe` without justification.
- Use `println!` — use `tracing` macros instead.
- Hard-code file paths or platform-specific assumptions without `cfg` gates.
- Break the 1560-byte `VecuFrame` layout or `vecu_base_context_t` field order.
- Add comments or emojis unless explicitly requested.
