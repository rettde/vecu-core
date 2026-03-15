# vecu-core

**Deterministische Virtual-ECU-Laufzeitumgebung** für die Ausführung von
AUTOSAR-basiertem ECU-C-Code auf dem Host-PC (Windows, Linux, macOS).

> *This project is not affiliated with or endorsed by the AUTOSAR
> consortium. AUTOSAR API names are used solely for interoperability
> with existing ECU software.*

---

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

---

## Architektur

```
┌──────────────────────────────────────────────────────────┐
│  vECU Runtime (Rust)                     vecu-loader     │
│                                                          │
│  ┌────────────────────────────────────────────────────┐  │
│  │  APPL Module (vecu-appl)                           │  │
│  │  ┌──────────────────────┐  ┌────────────────────┐  │  │
│  │  │  ECU C-Code (SWCs)   │  │  BaseLayer         │  │  │
│  │  │  libappl_ecu.so      │→ │  libbase.so        │  │  │
│  │  │                      │  │  24 BSW-Module      │  │  │
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

---

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

### Unterstützte UDS-Services (Dcm)

| SID | Service |
|-----|---------|
| 0x10 | DiagnosticSessionControl |
| 0x11 | ECUReset |
| 0x14 | ClearDiagnosticInformation |
| 0x19 | ReadDTCInformation |
| 0x22 | ReadDataByIdentifier |
| 0x27 | SecurityAccess (CMAC-basiert) |
| 0x2E | WriteDataByIdentifier |
| 0x31 | RoutineControl |
| 0x3E | TesterPresent |

---

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

---

## Ausführungsmodi

| Modus | Flag | Beschreibung |
|-------|------|-------------|
| **Standalone** | `--mode standalone` | Feste Tick-Anzahl, keine externen Abhängigkeiten |
| **SIL Kit** | `--mode silkit` | SIL Kit Co-Simulation, Ticks über `TimeSyncService` |

---

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

---

## Eigenes ECU-Projekt integrieren

Kurzfassung — Details in [HOWTO_ECU_INTEGRATION.md](HOWTO_ECU_INTEGRATION.md):

| Schritt | Was tun |
|---------|---------|
| 1 | `Appl_Entry.c` schreiben: `Appl_Init()`, `Appl_MainFunction()`, `Appl_Shutdown()` |
| 2 | RTE-Header erstellen: `Rte_SwcXxx.h` → ruft `Com_ReceiveSignal()`, `Csm_Encrypt()`, etc. auf |
| 3 | `Base_Entry.c` kopieren & Konfiguration anpassen (Signale, PDUs, DIDs, NvM-Blöcke, CanTp-IDs) |
| 4 | Als Shared Library kompilieren (CMake-Template im HOWTO) |
| 5 | `config.yaml` erstellen und `vecu-loader` starten |

Alternativ: Echten **Vector AUTOSAR BSW** statt Stubs verwenden —
siehe [HOWTO Section 12](HOWTO_ECU_INTEGRATION.md#12-using-the-vector-autosar-bsw-instead-of-stubs).

---

## Bus-Anbindung (SIL Kit)

`VecuFrame` trägt einen `bus_type`-Diskriminator. Alle Bus-Typen laufen
durch die `OpenSutApi`-Abstraktion:

| Bus | SIL Kit Controller | Status |
|-----|--------------------|--------|
| CAN | `SilKit_CanController` | ✅ Vollständig |
| Ethernet | `SilKit_EthernetController` | ✅ Vollständig |
| LIN | `SilKit_LinController` | ✅ Master-Mode |
| FlexRay | `SilKit_FlexrayController` | ✅ Passiv (Listen) |

### SIL Kit Konfiguration (in `config.yaml`)

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
cargo test --workspace          # 133 Tests (Unit + Integration)
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all -- --check
```

Die CI-Pipeline (`.github/workflows/ci.yml`) prüft:
check, test, clippy, fmt, rustdoc und den BaseLayer/sample_ecu CMake-Build.

---

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

---

## Lizenz

Dual-lizenziert unter:

- [Apache License 2.0](LICENSE-APACHE)
- [MIT License](LICENSE-MIT)

### Beitrag

Sofern nicht explizit anders angegeben, werden Beiträge unter der
obigen Dual-Lizenz eingereicht, ohne zusätzliche Bedingungen.
