# ADR-007: Real Level 3 vECU -- Open-Source Core with AUTOSAR Classic Plugin

## Status

**Proposed**

## Date

2026-03-20

## Related Decisions

- **[ADR-001](ADR-001-level3-vecu-architecture.md)** -- Level-3 vECU Architecture
- **[ADR-002](ADR-002-virtual-mcal-scope-guarantees.md)** -- Virtual-MCAL Scope & Guarantees
- **[ADR-003](ADR-003-vhsm-integration-guarantees.md)** -- vHsm Integration & Guarantees
- **[ADR-004](ADR-004-os-semantics-mapping.md)** -- OS-Semantics Mapping
- **[ADR-005](ADR-005-build-toolchain-integration.md)** -- Build- & Toolchain-Integration

---

## Decision Drivers

1. **Real Level 3** -- the complete, unmodified series AUTOSAR Classic BSW
   stack compiles and runs on the host. No code changes, no `#ifdef VECU`,
   no simplified stubs replacing production modules.
2. **vecu-core is 100% open-source (MIT OR Apache-2.0)** -- zero
   proprietary build-time, link-time, or runtime dependencies in the core.
3. **Full MICROSAR coverage NOW** -- Vector MICROSAR is the primary and
   currently only validated BSW stack. Every BSW module that the series
   ECU uses must compile and link on the host vECU.
4. **Stack-switchable LATER** -- the architecture does not hard-code
   MICROSAR. A future switch to ETAS ISOLAR-AB, EB tresos (Continental),
   or any other AUTOSAR Classic stack must be possible without changing
   vecu-core itself.
5. **No AUTOSAR consortium or Vector license violation** -- vecu-core's
   type headers are independently authored clean-room implementations
   referencing only publicly documented specifications.

---

## Scope & Intended Use

vecu-core targets **Software-in-the-Loop (SIL) testing** in CI/CD pipelines:

| Use Case | Supported | Notes |
|----------|-----------|-------|
| SIL testing in CI/CD | **Primary** | Automated regression on every commit |
| Developer desktop simulation | **Yes** | Fast iteration, no HW needed |
| Diagnostic protocol validation (UDS) | **Yes** | Dcm runs unmodified |
| Communication matrix testing (CAN/ETH) | **Yes** | Via SIL Kit bus simulation |
| NvM / persistent data round-trip | **Yes** | SHM-backed virtual flash |
| SecOC / Crypto chain validation | **Yes** | vhsm_adapter provides SW crypto |
| HiL replacement | **No** | No real-time guarantees |
| Cycle-accurate timing validation | **No** | Tick-based, not cycle-accurate |
| ASIL-rated safety testing | **No** | QM tool qualification only |
| Production deployment | **No** | SIL only, never on target HW |

---

## What Is Real Level 3?

Level 3 has TWO requirements -- **build correctness** AND **runtime
behavioral equivalence**:

### Build Correctness (necessary)

| Criterion | Required |
|-----------|----------|
| Application SWCs | Compile unmodified from series source |
| RTE | Series-generated RTE compiles unmodified |
| BSW (all modules) | Series BSW compiles unmodified |
| MCAL | **Replaced** by vmcal (virtual MCAL) |
| OS kernel | **Replaced** by os_mapping (deterministic tick dispatch) |
| Crypto HW driver | **Replaced** by vhsm_adapter (software HSM) |
| GenData | Series GenData compiles unmodified (NOT VTT variant) |
| Compiler | Host compiler (GCC/Clang/MSVC), NOT target cross-compiler |

**"Unmodified" means:** zero changes to `.c` and `.h` files from the series
build. The only difference is the build profile (include paths, compiler
flags, linked libraries).

### Runtime Behavioral Equivalence (sufficient)

The vECU must produce the **same observable outputs** as the series ECU
for the same input stimuli, within defined tolerances:

| Observable | Equivalence Criterion |
|------------|----------------------|
| CAN/ETH/LIN TX frames | Same PDU content, same signal values |
| UDS responses | Identical response codes and data |
| DTC storage (Dem) | Same DTC entries for same fault injection |
| NvM block content | Same persistent data after same write sequence |
| BSW state machines | Same states (EcuM, ComM, BswM) for same stimuli |
| Crypto results | Bit-identical (AES, CMAC, SHA are deterministic) |
| Task execution order | Same order per tick (deterministic scheduling) |

**Not required** (and explicitly out of scope):

| Aspect | Why not equivalent |
|--------|-------------------|
| Wall-clock timing | Host is faster/slower than target |
| ISR latency | No real interrupts on host |
| Register-level side effects | No real MCAL HW |
| Cycle count | Different CPU architecture |

### What vecu-core Replaces (and ONLY what it replaces)

| Series ECU Layer | vECU Level 3 | Status |
|------------------|-------------|--------|
| Application SWCs | UNCHANGED | series code |
| RTE (generated) | UNCHANGED | series GenData |
| BSW Stack (Com, PduR, Dcm, Dem, NvM, BswM, SecOC, ...) | UNCHANGED | series SIP + GenData |
| MCAL HW drivers (Can_30_*, Eth_30_*, Gpt_30_*, ...) | **vmcal** | open-source replacement |
| OS kernel (MICROSAR OS / RTA-OS) | **os_mapping** | open-source replacement |
| Crypto HW (Crypto_30_vHsm) | **vhsm_adapter** | open-source replacement |
| HW instrumentation (VX1000) | **Disabled** | empty header on host |

---

## Context: Why Previous Approaches Failed

### ZC_D_C0 Integration (~3 days bespoke patching)

| Issue | Root Cause |
|-------|------------|
| 120+ MemMap stubs | No universal no-op MemMap in vecu-core |
| Type redefinitions (Gpt, Crypto) | vecu-core AND SIP both define same types |
| VX1000 section errors | HW instrumentation header on host |
| Missing OS macros (TASK/ISR) | Incomplete Os.h |

### License Audit of "SIP-First" Approach

An earlier revision proposed delegating all type definitions to the SIP.
This creates a **proprietary build dependency**:

| Element | License Risk |
|---------|-------------|
| "SIP headers as type authority" | vecu-core can't build without Vector SIP |
| "vmcal includes SIP GeneralTypes" | Open-source core depends on proprietary input |

**vecu-core must build, test, and ship without any proprietary input.**

---

## Decision

### Architecture: Open-Source Core + AUTOSAR Classic Plugin

vecu-core is a self-contained open-source platform. The AUTOSAR Classic
BSW stack (MICROSAR today, potentially ETAS or EB tresos tomorrow)
connects through a plugin boundary.

```
+---------------------------------------------------------------+
|  vecu-core (MIT OR Apache-2.0) -- 100% open-source            |
|                                                                |
|  Rust Layer:                                                   |
|    vecu-runtime   vecu-abi        vecu-hsm                     |
|    (tick engine,  (VecuPluginApi, (SW crypto: AES-128,         |
|     SHM, SIL Kit) ABI contract)   CMAC, SHA-256, RNG)         |
|                                                                |
|  C Layer -- vecu-platform (clean-room AUTOSAR types):          |
|    Std_Types.h  Platform_Types.h  Compiler.h  MemMap.h  Os.h  |
|    ComStack_Types.h  Can_GeneralTypes.h  Eth_GeneralTypes.h   |
|    Fr_GeneralTypes.h  Lin_GeneralTypes.h  MemIf_Types.h       |
|    Crypto_GeneralTypes.h                                       |
|                                                                |
|  C Layer -- Virtualization:                                    |
|    vmcal/         vhsm_adapter/       os_mapping/              |
|    (15 MCAL       (Crypto_30_vHsm     (task dispatch,          |
|     modules)       API-compatible)      alarms, counters)      |
|                                                                |
|  C Layer -- baselayer (24 BSW stubs, PoC/test only):           |
|    EcuM, SchM, Com, PduR, Dcm, Dem, NvM, ... (NOT Level 3)   |
+------------------------------+--------------------------------+
                               |
               VecuPluginApi   |  (C ABI, function pointers)
               vecu_base_context_t
                               |
+------------------------------+--------------------------------+
|  AUTOSAR Classic Plugin (project-specific, NOT in vecu-core)  |
|                                                                |
|  The project builds appl_ecu.dylib/.so/.dll linking:          |
|                                                                |
|  FROM vecu-core (open-source):       FROM BSW vendor:          |
|    vmcal/*.c                           SIP BSW *.c             |
|    vhsm_adapter/*.c                   GenData *.c              |
|    os_mapping/*.c                     Application SWCs         |
|    vecu_microsar_bridge.c             RTE (generated)          |
|                                                                |
|  Include priority (project's build):                           |
|    1. vecu-platform/  (host types, Compiler, MemMap, Os)       |
|    2. SIP _Common/    (Std_Types.h uses OUR Platform_Types.h)  |
|    3. SIP Components/ (BSW module headers)                     |
|    4. GenData/        (project configuration)                  |
|    5. vmcal/include/  (MCAL API)                               |
|    6. vhsm_adapter/  (Crypto API)                              |
|                                                                |
|  Supported BSW stacks:                                         |
|    - Vector MICROSAR  (validated, primary)                     |
|    - ETAS ISOLAR-AB   (future, same plugin architecture)       |
|    - EB tresos / CAPI (future, same plugin architecture)       |
+---------------------------------------------------------------+
```

---

## Five Rules

### Rule 1: vecu-core owns its AUTOSAR-compatible types (clean-room)

vecu-core provides independently authored type headers under MIT/Apache-2.0:

| Header | Location | Basis |
|--------|----------|-------|
| `Std_Types.h` | `baselayer/include/` | `<stdint.h>`, AUTOSAR SWS |
| `ComStack_Types.h` | `vmcal/include/` | AUTOSAR SWS |
| `Can_GeneralTypes.h` | `vmcal/include/` | AUTOSAR SWS |
| `Eth_GeneralTypes.h` | `vmcal/include/` | AUTOSAR SWS |
| `MemIf_Types.h` | `vmcal/include/` | AUTOSAR SWS |
| `Os.h` | `baselayer/include/` | ISO 17356 (OSEK/VDX) |

Types use `<stdint.h>` (`uint8_t` -> `uint8`), correct on any host
(LP64, ILP32, LLP64). No `unsigned long` ambiguity.

**Legal basis:** These are functional interface types, not copyrightable
expression. AUTOSAR SWS documents are publicly available. Type definitions
(`typedef uint8_t uint8;`) are trivial. Oracle v. Google (2021) confirms
API reimplementation as fair use.

**When building with MICROSAR:** the project's build places SIP headers
at higher include priority. SIP's `Std_Types.h` includes its own
`Platform_Types.h` -- but ours shadows it (include priority level 1).
Result: SIP types use correct host widths. No conflict.

### Rule 2: AUTOSAR Classic BSW connects via plugin boundary

The `VecuPluginApi` in `vecu-abi` is the plugin boundary:

- vecu-core ships: runtime, platform types, vmcal, vhsm, os_mapping
- The **project** builds the plugin (`appl_ecu.dylib`) linking
  vecu-core's virtualization layers + the BSW vendor's stack + GenData
- The BSW vendor (Vector, ETAS, EB) is a **build-time input to the
  project**, not a dependency of vecu-core

### Rule 3: vecu-core builds and tests without ANY proprietary input

```bash
cargo test --workspace
cd examples/sample_ecu && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
```

CI uses only: vecu-core's own headers, baselayer stubs, `<stdint.h>`,
open-source Rust crates. Zero vendor license required.

### Rule 4: Target MCU platform is invisible to vecu-core

```c
#include <stdint.h>
typedef uint8_t  uint8;     // Always 1 byte
typedef uint32_t uint32;    // Always 4 bytes
```

No `unsigned long`, no `__ghs__`, no `_GREENHILLS_C_RH850_`.

When a MICROSAR project switches from RH850 to TriCore to S32K:
- SIP changes (different Compiler.h, different MCAL components)
- GenData is re-generated by DaVinci
- **vecu-core: zero changes**

### Rule 5: HW instrumentation is disabled at the project build level

VX1000, ETAS measurement, or any HW instrumentation:
- Project adds empty interception header to its include path
- Or project excludes instrumentation SIP components
- vecu-core provides universal no-op `MemMap.h` for section neutralization
- vecu-core itself has zero HW instrumentation references

---

## Full MICROSAR Coverage Target

### BSW Modules That Must Compile Unmodified

The following MICROSAR modules must compile and link in the vECU without
source modifications. This is the **completeness criterion for Level 3**.

| Category | Modules | Status |
|----------|---------|--------|
| System | EcuM, SchM, BswM, Det | Validated (ZC_D_C0) |
| Communication | Com, PduR, CanIf, EthIf, LinIf, FrIf | Validated |
| Transport | CanTp, DoIP, SomeIpTp, IpduM | Validated (SomeIpTp excluded due to version mismatch) |
| Diagnostics | Dcm, Dem, FiM | Validated |
| Memory | NvM, Fee, MemIf | Validated (NvM_Cfg excluded -- static assert) |
| Crypto | Csm, CryIf | Validated (CryIf_Cfg excluded -- vhsm replaces driver) |
| Security | SecOC, KeyM | To be validated |
| Watchdog | WdgM, WdgIf | To be validated |
| Network Mgmt | Nm, CanNm, ComM | To be validated |
| Ethernet | EthSM, TcpIp, SoAd | To be validated |
| FlexRay | FrSM, FrNm, FrTp | To be validated |
| LIN | LinSM, LinTrcv | To be validated |
| RTE | Rte, Rte_* (per OS-Application) | Validated |

### Modules Replaced by vecu-core (NOT compiled from SIP)

| Replaced Module | vecu-core Replacement |
|-----------------|-----------------------|
| Can_30_*, Eth_30_*, Fr_30_*, Lin_30_* | vmcal (Can.c, Eth.c, Fr.c, Lin.c) |
| Gpt_30_*, Dio_30_*, Port_30_*, Spi_30_* | vmcal (Gpt.c, Dio.c, Port.c, Spi.c) |
| Mcu_30_*, Fls_30_*, Adc_30_*, Pwm_30_*, Icu_30_* | vmcal (Mcu.c, Fls.c, Adc.c, Pwm.c, Icu.c) |
| Wdg_30_* | vmcal (Wdg.c) |
| Crypto_30_vHsm | vhsm_adapter (Crypto_30_vHsm.c) |
| Os (kernel, scheduler, interrupts) | os_mapping (deterministic tick dispatch) |
| VX1000, VX1000If, VX1000TlIf, VX1000HookIf | Disabled (empty header) |

### Known Exclusions (GenData)

Some GenData files cannot compile on host due to target-specific content:

| Excluded GenData | Reason | Impact |
|------------------|--------|--------|
| NvM_Cfg.c | Static assert references app variables | NvM init uses default config |
| CanIf_Lcfg.c | References excluded MCAL Can_30_* types | CanIf uses vmcal Can types |
| CryIf_Cfg.c | References Crypto_30_vHsm internals | CryIf routes to vhsm_adapter |
| Crypto_30_vHsm*.c | Replaced entirely by vhsm_adapter | N/A |
| RteAnalyzer/Source/*.c | Duplicate symbols with real BSW | Analysis stubs, not needed |
| *_Unity.c | Include sub-.c files causing duplicates | Individual .c files used instead |

**Goal: minimize this list with each MICROSAR SIP version.**

---

## Stack Switchability

The plugin architecture is BSW-vendor-neutral by design:

| Concern | How it stays neutral |
|---------|---------------------|
| Type headers | vecu-core's clean-room types; any SIP shadows them at build time |
| MCAL API | AUTOSAR SWS-defined (Can_Init, Eth_Init, ...) -- same for all vendors |
| Crypto API | AUTOSAR SWS-defined (Crypto_30_vHsm is Vector-specific name, but API is SWS Crypto) |
| OS API | OSEK/VDX (ISO 17356) -- same for MICROSAR OS, RTA-OS, EB tresos OS |
| GenData format | Vendor-specific, but gendata-adapt tool abstracts this |
| Build integration | CMake with configurable paths -- vendor is a build parameter |

**To switch from MICROSAR to ETAS ISOLAR-AB:**
1. Point SIP path to ETAS delivery
2. Point GenData path to ETAS-generated config
3. Re-run gendata-adapt
4. Rename vhsm_adapter (Crypto_30_vHsm -> vendor's crypto driver name)
5. Build

vecu-core itself: **zero changes**.

---

## Concrete Refactoring Plan

### Phase 1: Consolidate vecu-platform/ (clean-room types)

```
vecu-platform/
  include/
    Std_Types.h            # from baselayer/ (exists)
    Platform_Types.h       # NEW: host-ABI, <stdint.h>
    Compiler.h             # NEW: GCC/Clang AUTOSAR macros
    Compiler_Cfg.h         # NEW: empty defaults
    MemMap.h               # from examples/ (exists, universal no-op)
    Os.h                   # from baselayer/ (exists, extend OSEK macros)
    ComStack_Types.h       # from vmcal/ (exists)
    Can_GeneralTypes.h     # from vmcal/ (exists)
    Eth_GeneralTypes.h     # from vmcal/ (exists)
    Fr_GeneralTypes.h      # NEW: clean-room
    Lin_GeneralTypes.h     # NEW: clean-room
    MemIf_Types.h          # from vmcal/ (exists)
    Crypto_GeneralTypes.h  # NEW: full AUTOSAR Crypto types
```

### Phase 2: gendata-adapt Tool

```bash
tools/gendata-adapt \
  --gendata /path/to/GenData \
  --sip /path/to/SIP_Appl \
  --output /path/to/vecu_build/
```

Scans `.c`/`.h` files (text processing, no proprietary parser). Detects
Unity files, MemMap patterns, module names. Emits ready-to-build CMake.

### Phase 3: Full MICROSAR Validation

Validate ALL BSW modules from the coverage table above against ZC_D_C0.
Target: zero excluded modules except the known GenData exclusions.

---

## Platform Exchangeability

| When project changes... | vecu-core impact |
|------------------------|------------------|
| MCU (RH850 -> TriCore) | Zero -- `<stdint.h>` types, no MCU refs |
| Compiler (GHS -> TASKING) | Zero -- project shadows Compiler.h |
| BSW vendor (MICROSAR -> ETAS) | Zero -- same plugin architecture |
| SIP version (34.7 -> 35.x) | Zero -- gendata-adapt re-run |
| GenData re-generated | Zero -- gendata-adapt re-run |

---

## Consequences

### Positive

- **Real Level 3** -- series BSW compiles unmodified, full MICROSAR coverage
- **100% open-source core** -- MIT/Apache-2.0, zero vendor dependencies
- **No license risk** -- clean-room types, no SIP in repo, no AUTOSAR membership needed
- **MICROSAR validated** -- ZC_D_C0 proves the concept works
- **Stack-switchable** -- ETAS, EB tresos possible without core changes
- **Platform-agnostic** -- RH850/TriCore/S32K transparent
- **Automated integration** -- gendata-adapt reduces days to minutes

### Trade-offs

- vecu-core's clean-room types may lag behind SIP types (missing fields)
  -- projects validate compatibility at build time
- gendata-adapt needs testing against each new SIP version
- Some GenData files may require exclusion (target-specific static asserts)
- vhsm_adapter uses Vector-specific module name (Crypto_30_vHsm) --
  needs renaming for other vendors

---

## Known Limitations

| Limitation | Impact | Mitigation |
|------------|--------|------------|
| No real-time scheduling | Task jitter not modeled | Deterministic tick-based dispatch (same order, reproducible) |
| No ISR preemption | ISR timing not modeled | ISR bodies executed synchronously in tick |
| No HW register access | MCAL side effects simplified | vmcal provides AUTOSAR API semantics, not HW fidelity |
| No multicore parallelism | All cores execute sequentially per tick | os_mapping maps cores to sequential partitions |
| Software HSM (no HW HSM) | Key storage in RAM, not tamper-resistant | Acceptable for SIL; crypto results are bit-identical |
| No flash wear modeling | NvM always succeeds | SHM-backed; Fee/Fls emulation is instant |
| Host compiler differences | Struct padding, alignment may differ | `_Static_assert` guards in ABI headers; `CPU_TYPE_32` forced |
| No VX1000 measurement | XCP-on-ETH HW measurement unavailable | Measurement via SIL Kit XCP or tracing callbacks |

---

## Validation Strategy

### Level 1: Build Validation (automated, CI)

- vECU library compiles with zero errors, zero warnings (`-Wall -Werror`)
- All vecu-core unit tests pass (`cargo test --workspace`)
- Link succeeds: all BSW symbols resolved by vmcal/vhsm/os_mapping

### Level 2: Smoke Test (automated, CI)

- `Base_Init()` -> `EcuM_Init()` -> all BSW `*_Init()` succeed
- `Base_Step(tick)` -> `EcuM_MainFunction()` -> `SchM` dispatches
  all `*_MainFunction()` without crash/assertion
- `Base_Shutdown()` -> `EcuM_GoSleep()` -> clean de-init

### Level 3: Behavioral Regression (automated, CI)

- Inject CAN RX frame -> verify Com signal propagation -> TX frame content
- UDS request (0x22 ReadDataByIdentifier) -> verify correct positive response
- UDS request (0x31 RoutineControl) -> verify Dcm routing
- NvM write -> shutdown -> re-init -> NvM read -> verify data persisted
- Crypto: plaintext -> Csm_Encrypt -> Csm_Decrypt -> verify round-trip
- DTC: inject fault -> Dem_SetEventStatus -> verify DTC stored

### Level 4: Project-Specific Validation (per-project, manual/automated)

- Full communication matrix: all configured CAN/ETH/LIN PDUs
- Full diagnostic service coverage: all configured UDS services
- Full SecOC chain: authenticated PDU round-trip
- Comparison with HiL/target test results (where available)

---

## Quality & Process Context

### ASPICE / ISO 26262

vecu-core is a **SIL test tool**, not a production software component.
It falls under **tool qualification** (ISO 26262-8, Clause 11), not
under software development process requirements:

| Aspect | Classification |
|--------|---------------|
| Tool Confidence Level | TCL 1 (SIL testing, QM context) |
| Tool Impact | TI 1 (can fail to detect errors) |
| Tool Error Detection | TD 1 (qualified by validation suite) |
| ASIL scope | QM only -- not for ASIL-rated test verdicts |

**Tool qualification evidence:**
- Validation suite (Level 1-4 tests, see below)
- Known limitations documented (see above)
- Version-controlled, reproducible builds
- Traceability: vecu-core version + SIP version + GenData hash = vECU identity

### Existing Quality Infrastructure (already implemented)

**Documentation:**

| Asset | Location | Scope |
|-------|----------|-------|
| Integration HowTo | `HOWTO_ECU_INTEGRATION.md` (1500+ lines, 14 sections) | End-to-end: from project setup to UDS testing |
| Architecture overview | `README.md` (bilingual EN/DE) | Quick-start, repo structure, tech stack |
| ADR-001 through ADR-007 | `docs/adr/` | All architectural decisions documented |
| ZC_D_C0 integration guide | `examples/zc_d_vecu/README.md` | MICROSAR-specific build, exclusions, troubleshooting |
| API documentation | `cargo doc --workspace` (CI-enforced, `-Dwarnings`) | Every public Rust item documented |

**Unit & Integration Tests:**

| Test Category | Count | Coverage |
|---------------|-------|----------|
| Rust unit tests (all crates) | 144 | ABI, SHM, runtime, HSM, plugin loading |
| C integration tests (`vmcal_compile.rs`) | included | All vmcal/vhsm/os_mapping headers + sources compile |
| HSM E2E integration | 4 | Full Csm -> CryIf -> Cry -> vecu-hsm chain |
| Functional vmcal tests | 9 | Can/Eth/Fls lifecycle, CAN E2E loopback |
| Cross-platform CI | 3 OS | ubuntu-latest, macos-latest, windows-latest |

All tests run on every push to `main` and every pull request.

**Rust Quality Gates (enforced in CI, zero exceptions):**

| Gate | Configuration | Enforcement |
|------|---------------|-------------|
| `clippy::pedantic` | `deny` (workspace `Cargo.toml`) | CI job: `cargo clippy --workspace --all-targets -- -D warnings` |
| `unsafe_code` | `deny` | Workspace lint; `#[allow(unsafe_code)]` only with justification |
| `missing_docs` | `warn` | All public items must be documented |
| `unreachable_pub` | `warn` | No accidentally public APIs |
| `cargo fmt` | `--check` | CI job: `cargo fmt --all -- --check` |
| `RUSTFLAGS` | `-Dwarnings` | All compiler warnings are errors in CI |
| `RUSTDOCFLAGS` | `-Dwarnings` | All doc warnings are errors in CI |

**CI Pipeline (`.github/workflows/ci.yml`, 6 jobs):**

| Job | Runs on | What it checks |
|-----|---------|----------------|
| Check | ubuntu, macOS, Windows | `cargo check --workspace --all-targets` |
| Test | ubuntu, macOS, Windows | `cargo test --workspace` (144 tests) |
| Clippy | ubuntu | `cargo clippy` pedantic deny |
| Format | ubuntu | `cargo fmt --check` |
| BaseLayer | ubuntu, macOS | CMake build of C11 baselayer + sample_ecu |
| Documentation | ubuntu | `cargo doc --workspace --no-deps` with `-Dwarnings` |

**Every PR must pass all 6 jobs before merge. No exceptions.**

### Reproducible Builds

```
vECU Identity = f(vecu-core commit, SIP version, GenData hash, host toolchain)
```

| Artifact | Pinned by |
|----------|----------|
| vecu-core | Git commit SHA |
| Rust toolchain | `rust-toolchain.toml` |
| C compiler | Docker image tag (e.g., `vecu-builder:gcc-13.2`) |
| MICROSAR SIP | SIP version string (e.g., `CBD2400275_D00_34.07.03`) |
| GenData | SHA-256 hash of GenData directory |

CI pipeline: `docker run vecu-builder cmake --build .` -> deterministic output.

### SIP Compatibility Matrix

| MICROSAR SIP Version | vecu-core Version | Status |
|---------------------|-------------------|--------|
| SIP 34.7.3 (CBD2400275) | v0.x (current) | Build validated (ZC_D_C0) |
| SIP 35.x | TBD | Planned |
| SIP 34.6.x | TBD | Expected compatible |

Matrix is maintained in `docs/sip-compatibility.md` (per-project).

---

## AUTOSAR Licensing

vecu-core's clean-room type headers are independently authored under
MIT/Apache-2.0. They implement functional interface types documented
in publicly available AUTOSAR SWS specifications.

**For OEM/Tier-1 context:** The organization using vecu-core with MICROSAR
typically holds both an AUTOSAR membership (for specification access) and
a Vector license (for MICROSAR SIP). vecu-core does not change these
licensing relationships. It is a build tool that consumes the project's
already-licensed BSW artifacts.

vecu-core itself requires **no** AUTOSAR membership and **no** Vector
license. The clean-room types are sufficient for standalone development
and testing. When combined with MICROSAR, the project's existing licenses
cover the SIP usage.

---

## Explicit Non-Goals

- Distributing proprietary SIP headers, GenData, or BSW source code
- Providing DaVinci BSWMD files (requires Vector collaboration)
- Replacing DaVinci Configurator or any BSW configuration tool
- Implementing a full AUTOSAR Classic BSW stack in open-source
- Hardware measurement on host (VX1000, ETAS measurement HW)
- Emulating target compiler behavior (GHS pragmas, TASKING intrinsics)
- Supporting non-AUTOSAR-Classic platforms or C++ BSW stacks
- Real-time or cycle-accurate timing simulation
- ASIL-rated test verdicts (QM tool qualification only)
- Replacing HiL testing (SIL complements, does not replace)

---

## Relationship to VTT

| Aspect | VTT (Vector) | vecu-core |
|--------|-------------|-----------|
| License | Proprietary, per-seat | MIT/Apache-2.0 |
| Type ownership | VTT defines types | Clean-room `<stdint.h>` |
| BSW integration | BSWMD (deep, proprietary) | Plugin boundary (open) |
| OS | VttOs (proprietary) | os_mapping (open-source) |
| MCAL | VTT MCAL stubs | vmcal (open-source, full API) |
| Crypto | VTT Crypto | vhsm_adapter + vecu-hsm |
| Host platform | Windows only | Linux, macOS, Windows |
| Builds without vendor license | No | **Yes** |
| Level 3 completeness | Full (DaVinci-integrated) | Full (plugin + gendata-adapt) |

**vecu-core is not a VTT clone. It achieves the same Level 3 result
through a different, open-source architecture: clean-room types +
plugin boundary + automated GenData adaptation.**

---

## Final Statement

> **Real Level 3 means: the complete, unmodified AUTOSAR Classic BSW
> stack compiles on the host AND produces the same observable behavior
> as the series ECU for the same stimuli.**
>
> **vecu-core is a SIL test platform for CI/CD pipelines. It is
> 100% open-source (MIT/Apache-2.0), requires no vendor license to
> build or test, and is qualified as a QM-level test tool per
> ISO 26262-8 Clause 11.**
>
> **MICROSAR is the primary validated BSW stack (SIP 34.7.3).
> The plugin architecture allows switching to ETAS, EB tresos, or
> any other AUTOSAR Classic vendor without changing vecu-core.**
>
> **Behavioral equivalence is validated through a four-level test
> strategy: build -> smoke -> behavioral regression -> project-specific.
> Known limitations (no real-time, no ISR preemption, no HW fidelity)
> are explicitly documented.**
