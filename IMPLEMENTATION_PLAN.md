# APPL Module Implementation Plan (ADR‑005)

This document defines the phased implementation plan for the APPL module
with external C‑code and AUTOSAR BaseLayer as specified in ADR‑005.

Each phase is self‑contained, testable, and builds on the previous one.

---

## Phase Overview

| Phase | Title | Deliverables | Dependencies |
|-------|-------|-------------|--------------|
| **P1** | C Header & ABI Contract | `vecu_base_context.h`, `vecu_frame.h` | ADR‑005 |
| **P2** | FFI Bridge in vecu‑appl | `dlopen` + symbol lookup + callback wiring | P1 |
| **P3** | Reference BaseLayer | EcuM, SchM, Os, Det stubs (C) | P1 |
| **P4** | Communication Stack | Com, PduR, CanIf (signal I/O) | P2, P3 |
| **P5** | Crypto Integration | Csm, CryIf, Cry → HSM callbacks | P2, P3 |
| **P6** | Diagnostics & Memory | Dcm, Dem, NvM, Fee | P4, P5 |
| **P7** | Transport & Example | CanTp, DoIP, sample ECU project | P6 |

---

## P1 — C Header & ABI Contract

**Goal:** Define the stable C interface between the Rust bridge and the
C BaseLayer / application code. This header is the single source of truth.

### Deliverables

| File | Location | Content |
|------|----------|---------|
| `vecu_base_context.h` | `crates/vecu-abi/include/` | Callback context struct, BaseLayer symbols, application symbols |
| `vecu_frame.h` | `crates/vecu-abi/include/` | C‑compatible `vecu_frame_t` matching `VecuFrame` layout |
| `vecu_status.h` | `crates/vecu-abi/include/` | Status codes matching `vecu_abi::status` |

### Tasks

1. Create `crates/vecu-abi/include/` directory
2. Write `vecu_status.h` — mirror of `vecu_abi::status` constants
3. Write `vecu_frame.h` — `#[repr(C)]` struct as C typedef, with
   `MAX_FRAME_DATA`, `BusType` enum
4. Write `vecu_base_context.h` — full callback context as defined in ADR‑005:
   - Frame I/O: `push_tx_frame`, `pop_rx_frame`
   - HSM delegation: `hsm_encrypt`, `hsm_decrypt`, `hsm_generate_mac`,
     `hsm_verify_mac`, `hsm_seed`, `hsm_key`, `hsm_rng`
   - SHM vars pointer + size
   - Logging callback
   - Tick interval
   - BaseLayer symbols: `Base_Init`, `Base_Step`, `Base_Shutdown`
   - Application symbols: `Appl_Init`, `Appl_MainFunction`, `Appl_Shutdown`
5. Add `static_assert` in a test `.c` file to verify struct sizes match
   Rust definitions
6. Add a `cbindgen`‑style test in vecu‑abi that compiles the C headers
   and checks ABI compatibility

### Acceptance Criteria

- [ ] Headers compile with `gcc -Wall -Wextra -Werror -pedantic -std=c11`
- [ ] Headers compile with `cl.exe /W4` (Windows)
- [ ] `sizeof(vecu_frame_t)` matches `core::mem::size_of::<VecuFrame>()`
- [ ] `sizeof(vecu_base_context_t)` is documented and stable
- [ ] All callback pointer signatures match `VecuPluginApi` function pointers

### Estimated Effort: 1 day

---

## P2 — FFI Bridge in vecu‑appl

**Goal:** Extend `vecu-appl` to dynamically load the BaseLayer and
application shared libraries, inject the callback context, and delegate
lifecycle calls.

### Deliverables

| File | Content |
|------|---------|
| `crates/vecu-appl/src/bridge.rs` | `dlopen` / `LoadLibrary` abstraction using `libloading` crate |
| `crates/vecu-appl/src/context.rs` | Build `vecu_base_context_t` from runtime state + HSM API |
| `crates/vecu-appl/src/lib.rs` | Updated `appl_init` / `appl_step` / `appl_shutdown` delegating to C |

### Tasks

1. Add `libloading` dependency to `vecu-appl/Cargo.toml` (workspace)
2. Implement `BridgeLoader` struct:
   ```rust
   struct BridgeLoader {
       base_lib: libloading::Library,
       appl_lib: libloading::Library,
       base_init: unsafe extern "C" fn(*const VecuBaseContext),
       base_step: unsafe extern "C" fn(u64),
       base_shutdown: unsafe extern "C" fn(),
       appl_init: unsafe extern "C" fn(),
       appl_main: unsafe extern "C" fn(),
       appl_shutdown: unsafe extern "C" fn(),
   }
   ```
3. Implement `BridgeLoader::load(base_path, appl_path)` with symbol
   resolution and error handling
4. Implement `VecuBaseContext` construction:
   - Frame I/O callbacks: wrap the existing `LAST_INBOUND` / `TX_FRAME`
     statics into C‑callable function pointers
   - HSM callbacks: read HSM API from a stored `VecuPluginApi` reference
     (passed via `VecuRuntimeContext` extension or global)
   - SHM vars pointer: compute from SHM base + `off_vars` offset
   - Logging: wrap `log_fn` from `VecuRuntimeContext`
5. Update `appl_init`:
   - Read library paths from config (environment variables or SHM header
     extension, TBD)
   - Call `BridgeLoader::load()`
   - Build context and call `Base_Init(&ctx)`
   - Call `Appl_Init()`
6. Update `appl_step`: call `Base_Step(tick)` then `Appl_MainFunction()`
7. Update `appl_shutdown`: call `Appl_Shutdown()`, `Base_Shutdown()`,
   drop libraries
8. Fallback: if no libraries configured, keep existing echo behavior
9. Write unit tests with mock `.so` / `.dylib` (tiny C library compiled
   in `build.rs` or test fixture)

### Design Decisions

- **Library path discovery:** Via environment variables initially
  (`VECU_BASE_LIB`, `VECU_APPL_LIB`). Later via loader config (P7).
- **HSM API forwarding:** The Rust bridge holds a copy of the HSM
  `VecuPluginApi` and wraps each function pointer into a C‑callable
  trampoline. These trampolines are stored as `static` function pointers.
- **Thread safety:** All C calls happen within `step()`, which is
  single‑threaded by ADR‑001. No additional synchronization needed.

### Acceptance Criteria

- [ ] `appl_init` successfully loads a test BaseLayer + application `.so`
- [ ] `appl_step` calls `Base_Step` and `Appl_MainFunction` in correct order
- [ ] `appl_shutdown` calls shutdown functions and drops libraries cleanly
- [ ] Fallback echo mode still works when no libraries are configured
- [ ] All existing tests still pass
- [ ] New tests with mock C libraries pass on Linux + macOS

### Estimated Effort: 3 days

---

## P3 — Reference BaseLayer (Minimal BSW Stubs)

**Goal:** Provide a minimal but functional C BaseLayer that proves the
architecture end‑to‑end. This is the reference implementation that
ECU integrators can use as a template.

### Deliverables

| Directory | Content |
|-----------|---------|
| `baselayer/src/` | C source files for BSW module stubs |
| `baselayer/include/` | AUTOSAR‑compatible BSW headers |
| `baselayer/CMakeLists.txt` | Cross‑platform build (gcc/clang/MSVC) |
| `baselayer/README.md` | Integration guide |

### BSW Modules in P3 (Minimal Set)

| Module | Implementation Level | Description |
|--------|---------------------|-------------|
| **EcuM** | Functional | State machine: STARTUP → RUN → SHUTDOWN. Calls `*_Init()` / `*_MainFunction()` / `*_DeInit()` for all registered modules. |
| **SchM** | Functional | Deterministic scheduler. Iterates BSW `*_MainFunction()` list in fixed order. Driven by `Base_Step()`. |
| **Os** | Stub | `Os_GetCounterValue()` returns tick. Task activation is no‑op (single‑threaded). Alarm callback list. |
| **Det** | Functional | `Det_ReportError()` → `ctx.log_fn(LEVEL_ERROR, formatted_message)`. Counts errors per module. |
| **Rte** (skeleton) | Stub | Empty `Rte_Read_*()` / `Rte_Write_*()` / `Rte_Call_*()` — demonstrates pattern, no real signals yet. |

### Tasks

1. Create `baselayer/` directory at workspace root
2. Write `baselayer/include/Std_Types.h` — AUTOSAR standard types
   (`uint8`, `uint16`, `Std_ReturnType`, `E_OK`, `E_NOT_OK`)
3. Write `baselayer/include/EcuM.h` + `baselayer/src/EcuM.c` —
   state machine with `EcuM_Init()`, `EcuM_MainFunction()`,
   `EcuM_GoSleep()`, `EcuM_GetState()`
4. Write `baselayer/include/SchM.h` + `baselayer/src/SchM.c` —
   `SchM_Init()`, `SchM_MainFunction()` calling all registered
   `*_MainFunction()` in order
5. Write `baselayer/include/Os.h` + `baselayer/src/Os.c` —
   `Os_Init()`, `Os_GetCounterValue()`, `Os_Shutdown()`
6. Write `baselayer/include/Det.h` + `baselayer/src/Det.c` —
   `Det_Init()`, `Det_ReportError()`, `Det_GetErrorCount()`
7. Write `baselayer/src/Base_Entry.c` — implements `Base_Init()`,
   `Base_Step()`, `Base_Shutdown()` as defined in `vecu_base_context.h`.
   Stores context pointer, calls EcuM lifecycle.
8. Write `baselayer/CMakeLists.txt` — builds `libbase.so` / `base.dll`
9. Write smoke test: load BaseLayer, init, step 10 ticks, shutdown
10. Write `baselayer/README.md`

### File Structure

```
baselayer/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── Std_Types.h
│   ├── EcuM.h
│   ├── SchM.h
│   ├── Os.h
│   ├── Det.h
│   └── Rte.h           (skeleton)
└── src/
    ├── Base_Entry.c     (Base_Init / Base_Step / Base_Shutdown)
    ├── EcuM.c
    ├── SchM.c
    ├── Os.c
    └── Det.c
```

### Acceptance Criteria

- [ ] `cmake --build .` succeeds on Linux, macOS, Windows
- [ ] `libbase.so` exports `Base_Init`, `Base_Step`, `Base_Shutdown`
- [ ] `Det_ReportError()` calls log callback
- [ ] EcuM state transitions are correct (STARTUP → RUN → SHUTDOWN)
- [ ] Integration test: `vecu-appl` loads reference BaseLayer, runs 100 ticks

### Estimated Effort: 3 days

---

## P4 — Communication Stack

**Goal:** Add Com, PduR, and bus interface stubs so that ECU C‑code can
send and receive signals via `Com_SendSignal()` / `Com_ReceiveSignal()`.

### Deliverables

| Module | Files | Implementation Level |
|--------|-------|---------------------|
| **Com** | `Com.h`, `Com.c` | Functional — signal pack/unpack with configurable signal database |
| **PduR** | `PduR.h`, `PduR.c` | Functional — PDU routing table (Com ↔ CanIf/LinIf/EthIf/FrIf) |
| **CanIf** | `CanIf.h`, `CanIf.c` | Functional — delegates to `ctx.push_tx_frame()` / `ctx.pop_rx_frame()` |
| **LinIf** | `LinIf.h`, `LinIf.c` | Stub — same pattern as CanIf, routes `BusType::Lin` |
| **EthIf** | `EthIf.h`, `EthIf.c` | Stub — same pattern, routes `BusType::Eth` |
| **FrIf** | `FrIf.h`, `FrIf.c` | Stub — same pattern, routes `BusType::FlexRay` |

### Signal Database Format (`com_config.json`)

```json
{
  "signals": [
    {
      "name": "VehicleSpeed",
      "pdu_id": 256,
      "bit_position": 0,
      "bit_length": 16,
      "endianness": "little",
      "bus_type": "can",
      "init_value": 0
    }
  ],
  "pdus": [
    {
      "id": 256,
      "can_id": 1536,
      "dlc": 8,
      "direction": "rx",
      "bus_type": "can"
    }
  ]
}
```

### Tasks

1. Write `Com.h` / `Com.c` — `Com_Init()`, `Com_SendSignal()`,
   `Com_ReceiveSignal()`, `Com_MainFunction()`, `Com_RxIndication()`,
   `Com_TriggerTransmit()`
2. Implement signal packing: given signal definition, pack value into
   PDU byte array at correct bit position with correct endianness
3. Implement signal unpacking: reverse of packing
4. Write `PduR.h` / `PduR.c` — routing table from config, routes
   TX from Com → CanIf, RX from CanIf → Com
5. Write `CanIf.h` / `CanIf.c` — `CanIf_Transmit()` calls
   `ctx.push_tx_frame()`, `CanIf_RxIndication()` called when
   `ctx.pop_rx_frame()` returns a CAN frame
6. Write LinIf, EthIf, FrIf stubs (same pattern, minimal)
7. Add `Com_MainFunction()` to SchM schedule
8. Load `com_config.json` in `Base_Init()` — parse with minimal JSON
   parser (cJSON or hand‑written for zero‑dependency)
9. Integration test: send a CAN frame with a known signal, verify
   `Com_ReceiveSignal()` returns the correct value

### Acceptance Criteria

- [ ] `Com_SendSignal(VehicleSpeed, &value)` produces correct CAN frame
      via `push_tx_frame`
- [ ] Inbound CAN frame → `Com_ReceiveSignal(VehicleSpeed, &value)` works
- [ ] Little‑endian and big‑endian signals both work
- [ ] PduR routes between Com and correct interface module
- [ ] All four bus types (CAN, LIN, ETH, FR) have interface stubs

### Estimated Effort: 5 days

---

## P5 — Crypto Integration

**Goal:** Add Csm / CryIf / Cry stubs that delegate all crypto operations
to the HSM module via callback injection. Validate end‑to‑end
SecurityAccess flow.

### Deliverables

| Module | Files | Implementation Level |
|--------|-------|---------------------|
| **Csm** | `Csm.h`, `Csm.c` | Functional — routes to CryIf |
| **CryIf** | `CryIf.h`, `CryIf.c` | Functional — routes to Cry driver |
| **Cry** | `Cry.h`, `Cry.c` | Functional — calls `ctx.hsm_*()` callbacks |

### Tasks

1. Write `Csm.h` / `Csm.c`:
   - `Csm_Init()`
   - `Csm_Encrypt(jobId, mode, dataPtr, dataLen, resultPtr, resultLenPtr)`
   - `Csm_Decrypt(jobId, mode, dataPtr, dataLen, resultPtr, resultLenPtr)`
   - `Csm_MacGenerate(jobId, dataPtr, dataLen, macPtr, macLenPtr)`
   - `Csm_MacVerify(jobId, dataPtr, dataLen, macPtr, macLen, verifyPtr)`
   - `Csm_RandomGenerate(jobId, resultPtr, resultLenPtr)`
2. Write `CryIf.h` / `CryIf.c` — thin routing layer (1:1 pass‑through
   for our single HSM driver)
3. Write `Cry.h` / `Cry.c` — wraps each `ctx.hsm_*()` callback:
   - `Cry_Encrypt()` → `ctx.hsm_encrypt()`
   - `Cry_Decrypt()` → `ctx.hsm_decrypt()`
   - `Cry_MacGenerate()` → `ctx.hsm_generate_mac()`
   - `Cry_MacVerify()` → `ctx.hsm_verify_mac()`
   - `Cry_RandomGenerate()` → `ctx.hsm_rng()`
4. Add `Csm_MainFunction()` to SchM schedule
5. Integration test: ECU code calls `Csm_Encrypt()` → verify ciphertext
   matches direct `hsm_encrypt()` output
6. Integration test: Full SecurityAccess flow:
   `Csm_RandomGenerate()` (seed) → `Csm_MacGenerate()` (key) →
   validate via `ctx.hsm_key()`

### Acceptance Criteria

- [ ] `Csm_Encrypt()` produces same output as direct `hsm_encrypt()`
- [ ] `Csm_MacGenerate()` produces valid AES‑128‑CMAC
- [ ] `Csm_MacVerify()` correctly accepts/rejects MACs
- [ ] End‑to‑end SecurityAccess works through Csm → CryIf → Cry → HSM
- [ ] `Csm_RandomGenerate()` returns non‑zero random bytes

### Estimated Effort: 2 days

---

## P6 — Diagnostics & Memory

**Goal:** Add Dcm (UDS service dispatch), Dem (DTC management), NvM
(non‑volatile memory), and Fee (flash emulation) to enable full
diagnostic testing.

### Deliverables

| Module | Implementation Level |
|--------|---------------------|
| **Dcm** | Functional — UDS SID dispatch, session management |
| **Dem** | Functional — DTC storage, status bits, snapshots |
| **FiM** | Stub — function inhibition based on Dem status |
| **NvM** | Functional — block read/write backed by SHM vars |
| **MemIf** | Stub — routes to Fee |
| **Fee** | Functional — RAM‑backed emulation in SHM vars block |
| **WdgM** | Stub — alive supervision, reports to Det |

### UDS Services Supported (Dcm)

| SID | Service | Dcm Implementation |
|-----|---------|-------------------|
| `0x10` | DiagnosticSessionControl | Session state machine (Default, Extended, Programming) |
| `0x11` | ECUReset | Calls `EcuM_GoSleep()` + re‑init |
| `0x14` | ClearDiagnosticInformation | `Dem_ClearDTC()` |
| `0x19` | ReadDTCInformation | `Dem_GetDTCByStatus()`, `Dem_GetDTCSnapshot()` |
| `0x22` | ReadDataByIdentifier | DID table → Rte_Read or NvM_ReadBlock |
| `0x27` | SecurityAccess | Delegates to Csm (→ HSM) |
| `0x2E` | WriteDataByIdentifier | DID table → Rte_Write or NvM_WriteBlock |
| `0x31` | RoutineControl | Routine table → application callbacks |
| `0x3E` | TesterPresent | Resets session timer |

### Tasks

1. Write `Dcm.h` / `Dcm.c` — service dispatch table, session state,
   `Dcm_MainFunction()` for timeout handling
2. Write `Dem.h` / `Dem.c` — DTC status byte management (ISO 14229
   status bits), snapshot storage in NvM
3. Write `NvM.h` / `NvM.c` — block descriptor table, `NvM_ReadBlock()`,
   `NvM_WriteBlock()`, `NvM_WriteAll()`, `NvM_MainFunction()`.
   Backed by `ctx.shm_vars` pointer.
4. Write `Fee.h` / `Fee.c` — RAM emulation layer under NvM
5. Write `FiM.h` / `FiM.c` — queries Dem for DTC status, returns
   permission/inhibition for function IDs
6. Write `WdgM.h` / `WdgM.c` — checkpoint sequence monitoring,
   reports to Det on timeout
7. Add all `*_MainFunction()` to SchM schedule
8. Integration test: SecurityAccess (0x27) end‑to‑end through Dcm →
   Csm → HSM
9. Integration test: ReadDID (0x22) reads value from NvM
10. Integration test: DTC lifecycle — report, read, clear

### Acceptance Criteria

- [ ] SecurityAccess (0x27) works end‑to‑end: request seed → compute
      key → unlock
- [ ] ReadDID (0x22) returns correct data from NvM
- [ ] WriteDID (0x2E) persists data in SHM vars block
- [ ] DTC report → ReadDTC → ClearDTC lifecycle works
- [ ] Session timeout in Dcm produces correct NRC (0x7F)
- [ ] NvM_WriteAll() flushes all modified blocks

### Estimated Effort: 7 days

---

## P7 — Transport Layers & Example Project

**Goal:** Add CanTp (ISO 15765‑2) and DoIP (ISO 13400) for segmented
UDS messages, and provide a complete example ECU project that
demonstrates the full stack.

### Deliverables

| Component | Content |
|-----------|---------|
| **CanTp** | ISO 15765‑2 segmentation (SF, FF, CF, FC) and reassembly |
| **DoIP** | ISO 13400 TCP framing for UDS over Ethernet |
| **Example ECU** | Sample C application with 3 SWCs, signal I/O, diagnostics |
| **Example Config** | `config.yaml`, `com_config.json`, `dcm_config.json`, `nvm_config.json` |
| **CI Pipeline** | GitHub Actions workflow: build BaseLayer + example + run tests |

### Example ECU Project Structure

```
examples/sample_ecu/
├── CMakeLists.txt
├── config.yaml              # Loader configuration
├── base_config.json         # BaseLayer configuration
├── com_config.json          # Signal database
├── include/
│   ├── Rte_SwcSensor.h      # Generated RTE for sensor SWC
│   ├── Rte_SwcActuator.h    # Generated RTE for actuator SWC
│   └── Rte_SwcDiag.h        # Generated RTE for diagnostic SWC
└── src/
    ├── Appl_Entry.c          # Appl_Init / Appl_MainFunction / Appl_Shutdown
    ├── SwcSensor.c           # Reads vehicle speed from CAN
    ├── SwcActuator.c         # Writes actuator command to CAN
    └── SwcDiag.c             # Handles RoutineControl requests
```

### Tasks

1. Write `CanTp.h` / `CanTp.c` — ISO 15765‑2 state machine:
   Single Frame, First Frame, Consecutive Frame, Flow Control.
   `CanTp_Transmit()`, `CanTp_RxIndication()`, `CanTp_MainFunction()`
2. Write `DoIP.h` / `DoIP.c` — ISO 13400 header parsing,
   routing activation, diagnostic message handling
3. Create `examples/sample_ecu/` directory
4. Write 3 example SWCs (sensor read, actuator write, diagnostic routine)
5. Write RTE headers connecting SWCs to Com and Dcm
6. Write `Appl_Entry.c` calling SWC runnables
7. Write configuration files (com_config, dcm_config, nvm_config)
8. Write `config.yaml` for the loader
9. Write `CMakeLists.txt` that builds BaseLayer + sample ECU
10. Write GitHub Actions CI workflow:
    - Build BaseLayer (`cmake`)
    - Build sample ECU (`cmake`)
    - Build vECU (`cargo build`)
    - Run integration test: 1000 ticks with signal exchange + UDS
11. Write `examples/sample_ecu/README.md` with step‑by‑step instructions

### Acceptance Criteria

- [ ] `cmake --build .` builds BaseLayer + sample ECU on all 3 platforms
- [ ] `cargo test` runs integration test with loaded C libraries
- [ ] CAN signal round‑trip: SWC sends → bus → SWC receives
- [ ] UDS SecurityAccess via CanTp: multi‑frame request → correct response
- [ ] Full CI pipeline green on GitHub Actions
- [ ] README enables a new developer to run the example in < 15 minutes

### Estimated Effort: 7 days

---

## Timeline Summary

| Phase | Effort | Cumulative | Key Milestone |
|-------|--------|-----------|---------------|
| P1 | 1 day | 1 day | C headers compile on all platforms |
| P2 | 3 days | 4 days | `dlopen` + callback injection works |
| P3 | 3 days | 7 days | Reference BaseLayer loads and runs |
| P4 | 5 days | 12 days | Signal I/O end‑to‑end |
| P5 | 2 days | 14 days | Crypto delegation to HSM works |
| P6 | 7 days | 21 days | Full UDS diagnostics operational |
| P7 | 7 days | 28 days | Complete example project with CI |

**Total: ~28 working days (6 weeks)**

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| C compiler differences (gcc/clang/MSVC) | CI on all 3 platforms from P1 |
| AUTOSAR header compatibility | Use `Std_Types.h` subset, not full AUTOSAR headers |
| Signal endianness bugs | Exhaustive unit tests for pack/unpack in P4 |
| `dlopen` portability | Use `libloading` crate (proven cross‑platform) |
| NvM data corruption | Checksums on NvM blocks, validate on read |
| Dcm service complexity | Start with 8 SIDs, extend incrementally |
| Memory safety across FFI | All C code runs within single `step()` call, no threads |

---

## Dependencies

### External Crates (Rust)

| Crate | Purpose | Phase |
|-------|---------|-------|
| `libloading` | Cross‑platform `dlopen` / `LoadLibrary` | P2 |

### Build Tools (C)

| Tool | Purpose | Phase |
|------|---------|-------|
| CMake ≥ 3.16 | Cross‑platform C build | P3 |
| gcc / clang / MSVC | C compiler | P3 |

No proprietary dependencies.
