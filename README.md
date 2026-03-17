  # vecu-core
  **Deterministic virtual ECU runtime environment** for executing
  AUTOSAR-based ECU C-code on a host PC (Windows, Linux, macOS).

  > *This project is not affiliated with or endorsed by the AUTOSAR
  > consortium. AUTOSAR API names are used solely for interoperability
  > with existing ECU software.*

---

## What Is This?

`vecu-core` replaces the traditional embedded target toolchain (compiler,
debugger, eval board) with a **Software-in-the-Loop (SiL)** environment:

- Your **ECU C-code** (SWCs, RTE, application logic) is compiled as a
  shared library and executed inside a tick-based Rust runtime.
- A bundled **AUTOSAR BaseLayer** (24 BSW module stubs) provides every
  API your code calls: Com, Dcm, NvM, Csm, CanTp, DoIP, …
- A **SHE-compatible HSM** (AES-128 ECB/CBC, CMAC, CSPRNG) performs
  real cryptography — no stubs.
- **Vector SIL Kit** integration enables co-simulation with CANoe,
  Silver, DTS.monaco over CAN, Ethernet, LIN, and FlexRay.

Alternatively, the **real Vector AUTOSAR BSW** (from DaVinci Configurator)
can be used instead of the stub BaseLayer — see
[HOWTO Section 12](HOWTO_ECU_INTEGRATION.md#12-using-the-vector-autosar-bsw-instead-of-stubs).

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│  vECU Runtime (Rust)                     vecu-loader     │
│                                                          │
│  ┌────────────────────────────────────────────────────┐  │
│  │  APPL Module (vecu-appl)                           │  │
│  │  ┌──────────────────────┐  ┌────────────────────┐  │  │
│  │  │  ECU C-Code (SWCs)   │  │  BaseLayer         │  │  │
│  │  │  libappl_ecu.so      │→ │  libbase.so        │  │  │
│  │  │                      │  │  24 BSW modules     │  │  │
│  │  └──────────────────────┘  └────────────────────┘  │  │
│  └────────────────────────────────────────────────────┘  │
│  ┌────────────────────┐  ┌─────────────────────────────┐ │
│  │  HSM (vecu-hsm)    │  │  Bus: SIL Kit / Standalone  │ │
│  │  AES-128, CMAC,    │  │  CAN, ETH, LIN, FlexRay    │ │
│  │  SecurityAccess     │  │  (vecu-silkit)              │ │
│  └────────────────────┘  └─────────────────────────────┘ │
│               ↕ SHM (vecu-shm)                           │
└──────────────────────────────────────────────────────────┘
```

---

## Repository Structure

```
vecu-core/
├── baselayer/               ← AUTOSAR BSW stub library (C11)
│   ├── include/             ← 24 BSW headers (Com.h, Dcm.h, NvM.h, …)
│   ├── src/                 ← Implementations + Base_Entry.c
│   └── CMakeLists.txt
├── crates/
│   ├── vecu-abi/            ← C/Rust ABI: VecuPluginApi, VecuFrame, SHM layout
│   ├── vecu-shm/            ← Shared-memory manager: ring buffers, diag mailbox
│   ├── vecu-runtime/        ← Tick orchestrator, OpenSutApi trait
│   ├── vecu-appl/           ← APPL plugin (cdylib): loads BaseLayer + ECU code
│   ├── vecu-hsm/            ← HSM plugin (cdylib): AES-128, CMAC, SecurityAccess
│   ├── vecu-silkit/         ← Vector SIL Kit FFI: multi-bus controllers
│   └── vecu-loader/         ← CLI: plugin loading, ABI negotiation, simulation
├── docs/
│   ├── adr/                 ← Architecture Decision Records
│   └── PROJECT_DESCRIPTION.md
├── examples/sample_ecu/     ← Reference ECU project (3 SWCs, RTE, CMake, config)
├── HOWTO_ECU_INTEGRATION.md ← Integration guide for ECU projects
└── IMPLEMENTATION_PLAN.md   ← Phase plan (P1–P7, completed)
```

---

## BaseLayer — 24 AUTOSAR BSW Modules

The BaseLayer is a pure C11 library with no external dependencies.
It provides the AUTOSAR API surface that your ECU code compiles against.

| Category | Modules |
|----------|---------|
| **Lifecycle & OS** | EcuM, SchM, Os, Det |
| **Communication** | Com, PduR, CanIf, LinIf, EthIf, FrIf |
| **Transport** | CanTp (ISO 15765-2), DoIP (ISO 13400) |
| **Diagnostics** | Dcm (9 UDS services), Dem (DTC management) |
| **Memory** | NvM (SHM-backed), Fee, MemIf |
| **Crypto** | Cry, CryIf, Csm → delegated to HSM |
| **Safety** | FiM, WdgM |

### Supported UDS Services (Dcm)

| SID | Service |
|-----|---------|
| 0x10 | DiagnosticSessionControl |
| 0x11 | ECUReset |
| 0x14 | ClearDiagnosticInformation |
| 0x19 | ReadDTCInformation |
| 0x22 | ReadDataByIdentifier |
| 0x27 | SecurityAccess (CMAC-based) |
| 0x2E | WriteDataByIdentifier |
| 0x31 | RoutineControl |
| 0x3E | TesterPresent |

---

## Rust Crates

| Crate | Type | Purpose |
|-------|------|---------|
| **vecu-abi** | lib | ABI contract between Rust and C: `VecuPluginApi`, `VecuFrame` (1536 B, multi-bus), `BusType`, SHM layout, capability flags |
| **vecu-shm** | lib | Shared memory: lock-free ring-buffer queues (RX/TX), persistent vars block (NvM), diagnostic mailbox (HSM) |
| **vecu-runtime** | lib | Tick orchestrator with two traits: `RuntimeAdapter` (when ticks happen) + `OpenSutApi` (where frames go) |
| **vecu-appl** | cdylib | APPL plugin: loads `libbase.so` + `libappl_ecu.so` at runtime, injects `vecu_base_context_t` (callbacks for HSM, logging, frame I/O) |
| **vecu-hsm** | cdylib | HSM plugin: 20-slot AES-128 key store, ECB/CBC, CMAC, CSPRNG, SecurityAccess (seed = OsRng, key = CMAC(master, seed)) |
| **vecu-silkit** | lib | Vector SIL Kit FFI: dynamic loading of SIL Kit C API, controllers for CAN/ETH/LIN/FlexRay, shared RX buffer |
| **vecu-loader** | bin | CLI tool: reads `config.yaml`, loads plugins via `libloading`, negotiates ABI version, starts simulation |

---

## Execution Modes

| Mode | Flag | Description |
|------|------|-------------|
| **Standalone** | `--mode standalone` | Fixed tick count, no external dependencies |
| **SIL Kit** | `--mode silkit` | SIL Kit co-simulation, ticks driven by `TimeSyncService` |

---

## Quick Start

```bash
# 1. Build Rust workspace
cargo build --release

# 2. Build BaseLayer
cd baselayer && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
cd ../..

# 3. Build example ECU
cd examples/sample_ecu && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
cd ../../..

# 4. Run simulation
cargo run --release -p vecu-loader -- \
  --config examples/sample_ecu/config.yaml \
  --mode standalone
```

---

## Integrating Your ECU Project

Summary — full details in [HOWTO_ECU_INTEGRATION.md](HOWTO_ECU_INTEGRATION.md):

| Step | Action |
|------|--------|
| 1 | Write `Appl_Entry.c`: `Appl_Init()`, `Appl_MainFunction()`, `Appl_Shutdown()` |
| 2 | Create RTE headers: `Rte_SwcXxx.h` → calls `Com_ReceiveSignal()`, `Csm_Encrypt()`, etc. |
| 3 | Copy `Base_Entry.c` & adapt configuration (signals, PDUs, DIDs, NvM blocks, CanTp IDs) |
| 4 | Compile as shared library (CMake template in HOWTO) |
| 5 | Create `config.yaml` and run `vecu-loader` |

Alternative BaseLayer options (your SWC code stays the same):

| BaseLayer | License | Use case |
|-----------|---------|----------|
| **Our stubs** (default) | MIT / Apache-2.0 | Prototyping, unit tests, CI |
| **Vector AUTOSAR BSW** | Proprietary | Production-accurate validation |
| **Eclipse OpenBSW** | Apache-2.0 | Fully open-source stack with real state machines |

See [HOWTO Section 12](HOWTO_ECU_INTEGRATION.md#12-using-the-vector-autosar-bsw-instead-of-stubs) (Vector)
and [HOWTO Section 13](HOWTO_ECU_INTEGRATION.md#13-using-eclipse-openbsw-as-an-open-source-baselayer) (OpenBSW).

---

## Roadmap

- [ ] **Eclipse OpenBSW shim layer** — AUTOSAR-API wrapper over [OpenBSW](https://github.com/esrlabs/openbsw) (`vecu-openbsw-shim` repo)
- [ ] **Pilot integration** of a real ECU project
- [ ] **ARXML → C-struct converter** for DaVinci-generated configurations
- [ ] **Web dashboard** for simulation results

---

## Bus Integration (SIL Kit)

`VecuFrame` carries a `bus_type` discriminator. All bus types are routed
through the `OpenSutApi` abstraction:

| Bus | SIL Kit Controller | Status |
|-----|--------------------|--------|
| CAN | `SilKit_CanController` | ✅ Complete |
| Ethernet | `SilKit_EthernetController` | ✅ Complete |
| LIN | `SilKit_LinController` | ✅ Master mode |
| FlexRay | `SilKit_FlexrayController` | ✅ Passive (listen) |

### SIL Kit Configuration (in `config.yaml`)

```yaml
silkit:
  registry_uri: silkit://localhost:8500
  participant_name: vECU
  can_network: CAN1
  step_size_ns: 1000000
  coordinated: true
  eth_network: ETH1              # optional
  lin_network: LIN1              # optional
  flexray_network: FR1           # optional
```

---

## Tests & CI

```bash
cargo test --workspace          # 133 tests (unit + integration)
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all -- --check
```

The CI pipeline (`.github/workflows/ci.yml`) runs:
check, test, clippy, fmt, rustdoc, and the BaseLayer/sample_ecu CMake build.

---

## Tech Stack

| Component | Technology |
|-----------|------------|
| Runtime | Rust 1.75+, clippy-pedantic, `unsafe` denied |
| BaseLayer | C11, gcc/clang/MSVC, CMake 3.15+ |
| Crypto | `aes` 0.8, `cmac` 0.7, `cbc` 0.1, `rand` 0.8, `zeroize` 1 |
| IPC | `memmap2` (SHM), lock-free ring buffers |
| Plugin loading | `libloading` 0.8 (`.so`/`.dylib`/`.dll`) |
| Serialization | `serde` + `serde_yaml` |
| SIL Kit | Dynamic FFI (no build-time linking) |

---

## License

Dual-licensed under:

- [Apache License 2.0](LICENSE-APACHE)
- [MIT License](LICENSE-MIT)

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual-licensed as above, without any additional terms
or conditions.

---
---

# 🇩🇪 Deutsche Version

## Was ist das?

`vecu-core` ersetzt die klassische Embedded-Target-Toolchain (Compiler,
Debugger, Eval-Board) durch eine **Software-in-the-Loop (SiL)**-Umgebung:

- Euer **ECU-C-Code** (SWCs, RTE, Applikationslogik) wird als Shared
  Library kompiliert und in einer tick-basierten Rust-Runtime ausgeführt.
- Ein mitgelieferter **AUTOSAR BaseLayer** (24 BSW-Modul-Stubs) liefert
  alle APIs, die euer Code aufruft: Com, Dcm, NvM, Csm, CanTp, DoIP, …
- Ein **SHE-kompatibles HSM** (AES-128 ECB/CBC, CMAC, CSPRNG) führt
  echte Kryptografie aus — keine Stubs.
- Die Anbindung an **Vector SIL Kit** ermöglicht Co-Simulation mit
  CANoe, Silver, DTS.monaco über CAN, Ethernet, LIN und FlexRay.

Alternativ kann statt des Stub-BaseLayers auch der **echte Vector AUTOSAR
BSW** (aus DaVinci Configurator) eingebaut werden — siehe
[HOWTO Section 12](HOWTO_ECU_INTEGRATION.md#12-using-the-vector-autosar-bsw-instead-of-stubs).

## Architektur

Siehe [Architecture](#architecture) oben — das Diagramm ist identisch.

## Repo-Struktur

```
vecu-core/
├── baselayer/               ← AUTOSAR BSW Stub-Library (C11)
│   ├── include/             ← 24 BSW-Header (Com.h, Dcm.h, NvM.h, …)
│   ├── src/                 ← Implementierungen + Base_Entry.c
│   └── CMakeLists.txt
├── crates/
│   ├── vecu-abi/            ← C/Rust ABI: VecuPluginApi, VecuFrame, SHM-Layout
│   ├── vecu-shm/            ← Shared-Memory-Manager: Ring-Buffer, Diag-Mailbox
│   ├── vecu-runtime/        ← Tick-Orchestrator, OpenSutApi-Trait
│   ├── vecu-appl/           ← APPL-Plugin (cdylib): lädt BaseLayer + ECU-Code
│   ├── vecu-hsm/            ← HSM-Plugin (cdylib): AES-128, CMAC, SecurityAccess
│   ├── vecu-silkit/         ← Vector SIL Kit FFI: Multi-Bus-Controller
│   └── vecu-loader/         ← CLI: Plugin-Loading, ABI-Negotiation, Simulation
├── examples/sample_ecu/     ← Referenz-ECU-Projekt (3 SWCs, RTE, CMake, config)
├── HOWTO_ECU_INTEGRATION.md ← Integrationsanleitung für ECU-Projekte
└── IMPLEMENTATION_PLAN.md   ← Phasenplan (P1–P7, abgeschlossen)
```

## BaseLayer — 24 AUTOSAR BSW-Module

Der BaseLayer ist eine reine C11-Bibliothek ohne externe Dependencies.
Er stellt die AUTOSAR-API bereit, gegen die euer ECU-Code kompiliert.

| Kategorie | Module |
|-----------|--------|
| **Lifecycle & OS** | EcuM, SchM, Os, Det |
| **Communication** | Com, PduR, CanIf, LinIf, EthIf, FrIf |
| **Transport** | CanTp (ISO 15765-2), DoIP (ISO 13400) |
| **Diagnostics** | Dcm (9 UDS-Services), Dem (DTC-Management) |
| **Memory** | NvM (SHM-backed), Fee, MemIf |
| **Crypto** | Cry, CryIf, Csm → delegiert an HSM |
| **Safety** | FiM, WdgM |

## Rust-Crates

| Crate | Typ | Aufgabe |
|-------|-----|---------|
| **vecu-abi** | lib | ABI-Kontrakt zwischen Rust und C: `VecuPluginApi`, `VecuFrame` (1536 B, Multi-Bus), `BusType`, SHM-Layout, Capability-Flags |
| **vecu-shm** | lib | Shared-Memory: Lock-free Ring-Buffer-Queues (RX/TX), persistenter Vars-Block (NvM), Diagnostik-Mailbox (HSM) |
| **vecu-runtime** | lib | Tick-Orchestrator mit zwei Traits: `RuntimeAdapter` (wann geticked wird) + `OpenSutApi` (wohin Frames gehen) |
| **vecu-appl** | cdylib | APPL-Plugin: lädt `libbase.so` + `libappl_ecu.so` zur Laufzeit, injiziert `vecu_base_context_t` (Callbacks für HSM, Logging, Frame-I/O) |
| **vecu-hsm** | cdylib | HSM-Plugin: 20-Slot AES-128 Key-Store, ECB/CBC, CMAC, CSPRNG, SecurityAccess (Seed = OsRng, Key = CMAC(master, seed)) |
| **vecu-silkit** | lib | Vector SIL Kit FFI: dynamisches Laden der SIL Kit C-API, Controller für CAN/ETH/LIN/FlexRay, Shared RX-Buffer |
| **vecu-loader** | bin | CLI-Tool: liest `config.yaml`, lädt Plugins via `libloading`, verhandelt ABI-Version, startet Simulation |

## Schnellstart

```bash
# 1. Rust-Workspace bauen
cargo build --release

# 2. BaseLayer bauen
cd baselayer && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
cd ../..

# 3. Beispiel-ECU bauen
cd examples/sample_ecu && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
cd ../../..

# 4. Simulation starten
cargo run --release -p vecu-loader -- \
  --config examples/sample_ecu/config.yaml \
  --mode standalone
```

## Eigenes ECU-Projekt integrieren

Kurzfassung — Details in [HOWTO_ECU_INTEGRATION.md](HOWTO_ECU_INTEGRATION.md):

| Schritt | Was tun |
|---------|---------|
| 1 | `Appl_Entry.c` schreiben: `Appl_Init()`, `Appl_MainFunction()`, `Appl_Shutdown()` |
| 2 | RTE-Header erstellen: `Rte_SwcXxx.h` → ruft `Com_ReceiveSignal()`, `Csm_Encrypt()`, etc. auf |
| 3 | `Base_Entry.c` kopieren & Konfiguration anpassen (Signale, PDUs, DIDs, NvM-Blöcke, CanTp-IDs) |
| 4 | Als Shared Library kompilieren (CMake-Template im HOWTO) |
| 5 | `config.yaml` erstellen und `vecu-loader` starten |

Alternative BaseLayer-Optionen (euer SWC-Code bleibt identisch):

| BaseLayer | Lizenz | Einsatz |
|-----------|--------|---------|
| **Unsere Stubs** (Default) | MIT / Apache-2.0 | Prototyping, Unit-Tests, CI |
| **Vector AUTOSAR BSW** | Proprietär | Produktionsnahe Validierung |
| **Eclipse OpenBSW** | Apache-2.0 | Vollständig offener Stack mit echten Zustandsmaschinen |

Siehe [HOWTO Section 12](HOWTO_ECU_INTEGRATION.md#12-using-the-vector-autosar-bsw-instead-of-stubs) (Vector)
und [HOWTO Section 13](HOWTO_ECU_INTEGRATION.md#13-using-eclipse-openbsw-as-an-open-source-baselayer) (OpenBSW).

## Roadmap

- [ ] **Eclipse OpenBSW Shim-Layer** — AUTOSAR-API-Wrapper über [OpenBSW](https://github.com/esrlabs/openbsw) (`vecu-openbsw-shim` Repo)
- [ ] **Pilot-Integration** eines echten ECU-Projekts
- [ ] **ARXML → C-Struct-Konverter** für DaVinci-generierte Konfigurationen
- [ ] **Web-Dashboard** für Simulationsergebnisse

## Tests & CI

```bash
cargo test --workspace          # 133 Tests (Unit + Integration)
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all -- --check
```

Die CI-Pipeline (`.github/workflows/ci.yml`) prüft:
check, test, clippy, fmt, rustdoc und den BaseLayer/sample_ecu CMake-Build.

## Tech-Stack

| Komponente | Technologie |
|------------|-------------|
| Runtime | Rust 1.75+, clippy-pedantic, `unsafe` denied |
| BaseLayer | C11, gcc/clang/MSVC, CMake 3.15+ |
| Crypto | `aes` 0.8, `cmac` 0.7, `cbc` 0.1, `rand` 0.8, `zeroize` 1 |
| IPC | `memmap2` (SHM), Lock-free Ring-Buffers |
| Plugin-Loading | `libloading` 0.8 (`.so`/`.dylib`/`.dll`) |
| Serialization | `serde` + `serde_yaml` |
| SIL Kit | Dynamisches FFI (kein Build-Time-Linking) |
