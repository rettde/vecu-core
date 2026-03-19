# OpenBSW Integration PoC

Proof-of-concept demonstrating how **Eclipse OpenBSW** replaces the minimal
stub BaseLayer for a Level-3 vECU build (ADR-001, ADR-005).

## Motivation

ADR-001 states:

> Level-3 vECUs in this project are built exclusively with a 3rd-party
> AUTOSAR BaseLayer (Vector MICROSAR or Eclipse OpenBSW).

This PoC demonstrates the concrete integration path for Eclipse OpenBSW.

## Architecture

```
┌──────────────────────────────────────────────────┐
│  vecu-loader (Rust)                              │
│  ├── vecu-appl  (ABI bridge)                     │
│  │   ├── libopenbsw_base.so  (this PoC)         │
│  │   │   ├── OpenBSW BSW modules (C++ series)   │
│  │   │   ├── openbsw_bridge.cpp (lifecycle)      │
│  │   │   ├── VecuCanTransceiver.cpp (CAN I/O)   │
│  │   │   ├── Virtual-MCAL    (link-time subst.) │
│  │   │   ├── vHsm Adapter    (Crypto_30_vHsm)   │
│  │   │   └── OS-Mapping      (tick dispatch)     │
│  │   └── libappl_ecu.so (application unchanged)  │
│  └── vecu-hsm  (SHE + SHA-256 crypto)           │
└──────────────────────────────────────────────────┘
```

## OpenBSW Architecture (Analysis)

Eclipse OpenBSW (`github.com/eclipse-openbsw/openbsw`) is a **C++14 SDK**
with its own component model.  It is NOT a classic AUTOSAR C BaseLayer.

### Key Differences from Classic AUTOSAR

| Aspect | Classic AUTOSAR | Eclipse OpenBSW |
|--------|----------------|-----------------|
| Language | C11 | C++14 |
| Lifecycle | `EcuM_Init()` flat C | `LifecycleManager` with runlevels + `ILifecycleComponent` interface |
| Scheduler | `SchM_MainFunction()` | FreeRTOS POSIX port + `async` framework |
| CAN | `CanIf` / flat C | `AbstractCANTransceiver` virtual class (C++) |
| Diagnostics | `Dcm` + UDS services | `uds` module (C++ classes) |
| Transport | `CanTp` (ISO 15765-2) | `docan` module (C++) |
| Storage | `NvM` / `Fee` | `storage` module (C++) |
| Templates | ETL (Embedded Template Library) | ETL |

### OpenBSW BSW Modules → vecu-core Mapping

| OpenBSW Module | Classic AUTOSAR Equivalent | vecu-core Integration Point |
|----------------|---------------------------|---------------------------|
| `lifecycle` | EcuM | `Base_Init` / `Base_Step` / `Base_Shutdown` |
| `async` + `asyncImpl` | SchM / Os | OS-Mapping (tick dispatch) |
| `cpp2can` | CanIf | `VecuCanTransceiver` → `vecu_base_context_t` |
| `docan` | CanTp (ISO 15765-2) | Runs on top of `cpp2can` |
| `cpp2ethernet` | EthIf | Future: `VecuEthTransceiver` |
| `doip` | DoIP | Runs on top of `cpp2ethernet` |
| `uds` | Dcm | Runs on top of `transport` |
| `transport` | PduR | Routes between DoCAN/DoIP and UDS |
| `storage` | NvM / Fee | SHM-backed via `vecu_base_context_t.shm_vars` |
| `runtime` | SchM (cyclic tasks) | Timer-based task dispatch |
| `logger` | Det | Bridged to `vecu_base_context_t.log_fn` |

### OpenBSW Lifecycle Flow

```
main()
  └─ app_main()
       └─ app::run()
            ├─ AsyncAdapter::run()  ← starts FreeRTOS scheduler
            └─ startApp()
                 ├─ lifecycleManager.addComponent("can", ...)
                 ├─ lifecycleManager.addComponent("transport", ...)
                 ├─ lifecycleManager.addComponent("uds", ...)
                 └─ lifecycleManager.transitionToLevel(MAX)
                      ├─ Level 1: runtime, safety
                      ├─ Level 2: CAN (platform BSP)
                      ├─ Level 4: transport
                      ├─ Level 5: DoCAN, Ethernet, storage
                      ├─ Level 6: DoIP
                      ├─ Level 7: UDS
                      └─ Level 9: demo application
```

## Integration Strategy (ADR-005 Build Profile)

The only difference between the series ECU build and the vECU build is
**module substitution at link time**:

| Layer | Series ECU | vECU (this PoC) |
|-------|-----------|-----------------|
| Application + RTE | series code | **same** (unchanged) |
| BSW (lifecycle, uds, ...) | OpenBSW | **same** (unchanged) |
| CAN Transceiver | `SocketCanTransceiver` | **`VecuCanTransceiver`** |
| Crypto | HW-HSM | **vHsm Adapter** → vecu-hsm |
| OS / Threading | FreeRTOS POSIX | **OS-Mapping** (tick dispatch) |

The build profile switch is a single CMake option: `-DVECU_BUILD=ON`.

### CAN Substitution: `VecuCanTransceiver`

OpenBSW's CAN layer uses `AbstractCANTransceiver` (virtual C++ class).
The series build uses `SocketCanTransceiver` (Linux SocketCAN).

Our `VecuCanTransceiver` extends `AbstractCANTransceiver` and routes frames
through `vecu_base_context_t`:

- **TX**: `write(CANFrame)` → `toVecuFrame()` → `ctx->push_tx_frame()`
- **RX**: `poll()` → `ctx->pop_rx_frame()` → `fromVecuFrame()` → `notifyListeners()`

This is a drop-in replacement — no OpenBSW source code changes needed.

## Directory Layout

```
examples/openbsw_poc/
├── CMakeLists.txt                # Build scaffold (C stubs or C++ real)
├── README.md                     # This file
├── config.yaml                   # vecu-loader configuration
├── include/
│   ├── openbsw_shim.h           # C lifecycle bridge header (stubs path)
│   ├── vecu/
│   │   └── VecuCanTransceiver.h  # Virtual CAN transceiver for OpenBSW
│   └── vecu_platform/            # Single-threaded platform stubs
│       ├── async/
│       │   ├── Lock.h            # No-op lock (replaces FreeRTOS interrupt lock)
│       │   ├── ModifiableLock.h  # No-op modifiable lock
│       │   └── Types.h           # Cooperative async types (no FreeRTOS)
│       ├── bsp/timer/
│       │   └── SystemTimer.h     # Tick-based deterministic timer
│       └── interrupts/
│           └── suspendResumeAllInterrupts.h  # No-op (single-threaded)
└── src/
    ├── openbsw_shim.c            # C lifecycle bridge (stubs path)
    ├── openbsw_stubs.c           # Placeholder stubs for CI
    ├── openbsw_bridge.cpp        # C++ lifecycle bridge (real OpenBSW path)
    └── VecuCanTransceiver.cpp    # Virtual CAN transceiver implementation
```

## How to Use

### Path A: Stubs (CI / quick test)

No external dependencies.  The C stubs provide the `Base_*` entry points:

```bash
cd examples/openbsw_poc
cmake -B build -S .
cmake --build build
```

### Path B: Real OpenBSW Sources

#### Step 1: Clone OpenBSW

```bash
git clone --depth 1 https://github.com/eclipse-openbsw/openbsw.git \
    examples/openbsw_poc/openbsw_src
```

(The directory `openbsw_src/` is in `.gitignore` and won't be committed.)

#### Step 2: Configure with OPENBSW_ROOT

```bash
cd examples/openbsw_poc
cmake -B build -S . \
    -DVECU_BUILD=ON \
    -DOPENBSW_ROOT="$(pwd)/openbsw_src"
```

When `OPENBSW_ROOT` is set, the CMake build:
- Enables C++ (C++14) alongside C11
- Compiles `openbsw_bridge.cpp` + `VecuCanTransceiver.cpp` instead of stubs
- Compiles only the targeted OpenBSW cpp2can sources (5 files):
  `AbstractCANTransceiver.cpp`, `CANFrame.cpp`, `BitFieldFilter.cpp`,
  `IntervalFilter.cpp`, `AbstractStaticBitFieldFilter.cpp`
- Puts `vecu_platform/` first in include path so single-threaded stubs
  override FreeRTOS-specific headers (`Lock.h`, `Types.h`, `SystemTimer.h`)
- Sets up include paths for ETL, platform BSP, and OpenBSW BSW modules

#### Step 3: Build

```bash
cmake --build build
```

This produces `libopenbsw_base.{so,dylib,dll}` containing:
- OpenBSW cpp2can layer (real C++ CAN abstraction)
- `VecuCanTransceiver` (virtual CAN)
- Virtual-MCAL (C)
- vHsm Adapter (C)
- OS-Mapping (C)
- Lifecycle bridge (`openbsw_bridge.cpp`)
- vecu_platform stubs (single-threaded async, timer, interrupts)

#### Step 4: Run with vecu-loader

```bash
cd /path/to/vecu-core
cargo run -- --config examples/openbsw_poc/config.yaml
```

## Bridge Components

### `openbsw_bridge.cpp` (C++ lifecycle bridge)

Maps our `Base_*` C ABI to OpenBSW's C++ `LifecycleManager`:

| vecu ABI | Bridge Action |
|----------|--------------|
| `Base_Init(ctx)` | `VMcal_Init(ctx)` → `Os_Init()` → create `VecuCanTransceiver` → open CAN |
| `Base_Step(tick)` | `Os_Tick(tick)` → `VecuCanTransceiver::poll()` (RX) |
| `Base_Shutdown()` | `VecuCanTransceiver::shutdown()` → `Os_Shutdown()` |

The bridge provides `vecuPlatformLifecycleAdd()` as a weak symbol.
A real vECU project overrides it to register its `ILifecycleComponent`s
with the `LifecycleManager` at the appropriate runlevels.

### `VecuCanTransceiver` (virtual CAN transceiver)

Extends OpenBSW's `AbstractCANTransceiver`:

| Method | Implementation |
|--------|---------------|
| `init()` / `open()` / `close()` / `shutdown()` | State management |
| `write(CANFrame)` | Convert to `vecu_frame_t`, call `ctx->push_tx_frame()` |
| `poll(maxRx)` | Call `ctx->pop_rx_frame()`, convert to `CANFrame`, call `notifyListeners()` |
| `mute()` / `unmute()` | Suppress TX |

## vecu_platform: Single-Threaded Platform Stubs

OpenBSW's cpp2can layer depends on FreeRTOS threading primitives.
The `include/vecu_platform/` directory provides single-threaded replacements:

| Header | Replaces | Implementation |
|--------|----------|----------------|
| `async/Lock.h` | FreeRTOS interrupt-suspension lock | No-op (single-threaded) |
| `async/ModifiableLock.h` | FreeRTOS modifiable lock | No-op (single-threaded) |
| `async/Types.h` | FreeRTOS-specific async types | Cooperative types, `TimeoutType` calls `IRunnable::execute()` |
| `bsp/timer/SystemTimer.h` | Platform BSP hardware timer | Tick-based deterministic timer |
| `interrupts/suspendResumeAllInterrupts.h` | FreeRTOS interrupt suspension | No-op (single-threaded) |

These stubs are placed **first** in the include path, so they override
the FreeRTOS-specific headers from `asyncFreeRtos/` without modifying
any OpenBSW source code.

## Remaining Work for Full Integration

### Must-Have

1. **ETL configuration**: OpenBSW uses the Embedded Template Library.
   An `etl_profile.h` must be provided (see `executables/referenceApp/etl_profile/`).

2. **Application registration**: Override `vecuPlatformLifecycleAdd()` to
   register the actual ECU application `ILifecycleComponent`s.

3. **Expand compiled modules**: Currently only cpp2can is compiled.
   Adding more modules (lifecycle, docan, transport, uds) requires
   additional vecu_platform stubs for each module's dependencies.

### Nice-to-Have

4. **VecuEthTransceiver**: Virtual Ethernet transceiver extending
   OpenBSW's Ethernet driver interface (similar pattern to CAN).

5. **VecuStorageBackend**: Bridge OpenBSW's `storage` module to
   `vecu_base_context_t.shm_vars` for persistent NvM data.

6. **Logger bridge**: Route OpenBSW's `logger` output through
   `vecu_base_context_t.log_fn` for unified tracing.

7. **Full async adapter**: Extend the cooperative async stubs to support
   `async::execute()` and `async::schedule()` for modules that need
   deferred execution.

## What This PoC Validates

1. **Real C++ compilation**: OpenBSW's cpp2can layer (5 source files)
   compiles against Virtual-MCAL headers with vecu_platform stubs
2. **ABI boundary**: `vecu_base_context_t` is the sole interface between
   Rust runtime and C/C++ BaseLayer
3. **Link-time substitution**: CAN transceiver, HSM, OS, and async
   framework are swapped without modifying any OpenBSW source code
4. **CAN routing**: `VecuCanTransceiver` provides a concrete implementation
   of OpenBSW's `AbstractCANTransceiver` backed by vecu-core's frame I/O
5. **Lifecycle compatibility**: `Base_Init` / `Base_Step` / `Base_Shutdown`
   map cleanly to OpenBSW's `LifecycleManager`
6. **Platform independence**: Single-threaded vecu_platform stubs eliminate
   FreeRTOS dependency for deterministic simulation

## License

MIT OR Apache-2.0
