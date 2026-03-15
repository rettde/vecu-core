# vECU Architecture

This document defines the architecture of the vECU Execution System.
It is normative and implementation‑binding.

## Overview

The system executes virtual ECUs in a deterministic, tool‑independent and
cross‑platform way. All ECU logic is implemented in user‑defined modules.
The system is compatible with OpenSUT semantics but has no dependency
on proprietary tooling.

---

## Architecture Decisions

### ADR‑001: Cross‑Platform vECU Loader for Custom APPL/HSM Modules

**(OpenSUT‑compatible, no VTT dependency)**

**Status:** Accepted

#### Context

The vECU Runtime Environment already provides:

- Execution environment for virtual ECUs
- Deterministic simulation (tick‑based)
- Shared‑memory‑based interaction
- OpenSUT‑compatible semantics for vECU access
- Optional integration with distributed environments (e.g. SIL Kit)

Historically, vECUs were treated as VTT artifacts (`appl.dll`, `hsm.dll`).
This assumption is **no longer valid**.

**Current situation:**

- APPL and HSM are fully custom‑developed modules
- There is no dependency on Vector VTT (neither build, ABI, nor tooling)
- The terms APPL and HSM describe functional roles, not tool artifacts
- OpenSUT compatibility means semantic compatibility, not binary compatibility

What is missing is a clearly specified loader component that:

- loads these custom modules in a platform‑neutral way,
- formalizes the ABI boundary,
- ensures deterministic orchestration,
- and correctly initializes the vECU Runtime Environment.

#### Decision

We specify an independent, platform‑neutral vECU Loader that:

- exclusively loads custom APPL/HSM modules
- has no dependency on VTT or proprietary toolchains
- communicates with modules only through a defined, stable C ABI
- enables OpenSUT‑compatible execution
- works identically on Windows, Linux, and macOS
- is purely orchestrating (no ECU logic)

**The Loader is the only OS‑ and platform‑dependent part of the solution.**

#### Definitions

| Term                   | Meaning                                                     |
| ---------------------- | ----------------------------------------------------------- |
| **APPL**               | Custom vECU application module                              |
| **HSM**                | Custom vECU security / crypto module                        |
| **Loader**             | Orchestrator, lifecycle owner, ABI boundary                 |
| **Runtime Env**        | Existing execution logic (not part of this ADR)             |
| **OpenSUT‑compatible** | Same interaction model, not same binary interface           |

#### Loader Responsibilities

**The Loader is responsible for:**

- CLI / configuration processing (`config.yaml`)
- Loading APPL/HSM modules
- ABI version and capability negotiation
- Initializing the vECU Runtime Environment
- Allocating and handing over shared memory
- Deterministic tick orchestration
- Forwarding I/O between runtime and modules

**The Loader is not responsible for:**

- ECU functionality
- Bus simulation
- Diagnostic logic
- Security policies
- Tool integration

**The Loader has no knowledge of ECU internals.**

#### Platform Neutrality (hard design decision)

The Loader must run without code changes on:

| Platform | Mechanism              |
| -------- | ---------------------- |
| Windows  | `.exe` + `.dll`        |
| Linux    | ELF binary + `.so`     |
| macOS    | Mach‑O binary + `.dylib` |

All OS differences are encapsulated internally.

#### Loader CLI (code‑relevant)

```text
vecu-loader
  --appl <path>
  --hsm <path>
  --config <config.yaml>
  [--mode standalone|distributed]
```

No implicit paths, no tool defaults.

#### Module Loading Concept (without VTT)

**Principles:**

- APPL and HSM are ordinary dynamic libraries
- Naming conventions are purely OS‑driven, not tool‑dependent
- The Loader knows only **one** symbol

**Single mandatory entry point:**

```c
vecu_status_t vecu_get_api(
    vecu_abi_version_t requested,
    vecu_plugin_api_t* out_api
);
```

No second symbol, no magic behavior.

#### ABI Contract (Loader ↔ Modules)

**ABI principles:**

- C ABI (no C++, no Rust ABI)
- Function‑table pattern
- Explicit versioning
- No allocation across module boundaries
- No global state

Goal: stable, language‑neutral, testable.

#### Initialization Sequence (exact, implementable)

The Loader must follow this sequence:

1. Load APPL module
2. Load HSM module
3. Call `vecu_get_api()` on both modules
4. Validate:
   - ABI version
   - Module role (APPL/HSM)
   - Capability flags
5. Initialize vECU Runtime Environment
6. Allocate shared memory
7. Pass runtime context to both modules (`init`)

No module sees the other directly.

#### Deterministic Orchestration (mandatory)

The Loader defines the **only valid call order**:

```text
for each simulation tick:
  1. Loader → APPL:  push_frame(inbound)   [0..N]
  2. Loader → HSM:   step(tick)
  3. Loader → APPL:  step(tick)
  4. Loader ← APPL:  poll_frame(outbound)  [0..N]
```

> **Note:** The inbound frames (step 1) logically originate from the
> vECU Runtime Environment (bus, OpenSUT, external sources). The Loader
> forwards them as orchestrator to the APPL module. Likewise, in step 4
> the Loader collects outbound frames and passes them to the runtime.
> Modules only see the Loader, never the runtime directly.

**Properties:**

- Single‑threaded ABI
- Reproducible
- CI‑capable
- Identical behavior on all OS

**Determinism is part of the ABI.**

#### Shared Memory Ownership

- Shared memory is created **exclusively by the Loader**
- Modules receive:
  - Base pointer
  - Size
  - Header with offsets
- Modules:
  - May read/write
  - Must **not** re‑allocate
  - Must not exchange pointers with each other

Prevents hidden coupling.

#### OpenSUT Compatibility (clearly scoped)

The Loader:

- Fulfills the **semantic** expectations of OpenSUT:
  - vECU start/stop
  - Deterministic execution
  - Clearly defined interaction
- Is **not** an OpenSUT framework
- Can be used through an OpenSUT adapter

Compatible, but not dependent.

#### Error and Exit Semantics

**The Loader must abort when:**

- A module cannot be loaded
- `vecu_get_api` is missing
- ABI is incompatible
- Initialization fails

**The Loader may continue when:**

- Optional capabilities are missing
- Modules produce no frames

#### Implementation Guidelines

**Language:**

- Loader: Rust (or C++17)
- Modules: C / C++ / Rust

**Rules:**

- No OS APIs outside the Loader package
- No threads in the ABI
- No exceptions across the ABI
- Logging only through callbacks

#### Rejected Alternatives

| Alternative                      | Reason                          |
| -------------------------------- | ------------------------------- |
| VTT‑compatible binary interface  | Unnecessary, creates tool lock‑in |
| Windows DLL only                 | Contradicts Linux/macOS goal    |
| C++ ABI                          | Unstable                        |
| Callback‑based orchestration     | Not deterministic               |

---

### ADR‑002: Custom APPL and HSM Modules for vECU Loader

**(1:1 compatible with Loader ABI, OpenSUT‑compatible, platform‑neutral)**

**Status:** Accepted

**Reference:** This ADR is binding in combination with ADR‑001.
All modules described here must fulfill the Loader ABI defined there.
Deviations are not permitted, as they would break determinism, portability,
or interchangeability.

#### Context

The vECU Runtime Environment executes two functionally separate, custom‑developed
modules:

- **APPL module:** Vehicle / ECU application logic
- **HSM module:** Security, crypto, and protection functions

These modules:

- Are not tool artifacts
- Are not VTT DLLs
- Are fully under own control
- Are orchestrated exclusively by the vECU Loader

Historical tool boundaries (e.g. CANoe/VTT) no longer apply.
The terms APPL and HSM denote **logical roles**, not origin.

#### Decision

APPL and HSM are defined as platform‑neutral, dynamically loadable modules that:

- Implement exactly the same Loader ABI (ADR‑001)
- Have no direct dependency on each other
- Interact only through shared memory and defined ABI calls
- Have no OS, tool, or framework knowledge
- Operate deterministically and reproducibly

**The Loader is the sole orchestrator.**

#### Common Principles for APPL and HSM

##### 1. ABI Obligation

Every module must:

- Export `vecu_get_api()`
- Return a valid `vecu_plugin_api_t`
- Implement `common.init` / `step` / `shutdown`
- Provide no additional exports

**One symbol. One contract.**

##### 2. Platform Neutrality

A module:

- Must not use OS APIs
- Must not start threads
- Must not perform dynamic loading
- Must not allocate memory outside the Loader

A module is pure logic code.

##### 3. Memory and Lifecycle Rules

- Modules own no memory
- All resources come from the Loader:
  - Shared memory
  - Allocator
  - Logger
- A module:
  - Initializes in `init()`
  - Works only in `step()`
  - Releases resources in `shutdown()`

No global state outside the module.

#### APPL Module (Application Logic)

##### Role

The APPL module implements:

- ECU / vehicle application logic
- State machines
- Communication with the outside world (frames, events)
- Functional response to diagnostic and control commands

It is the **functional core** of the vECU.

##### Required Capabilities (ABI)

The APPL module must set the following flags:

```c
VECU_CAP_FRAME_IO
```

Optional:

```c
VECU_CAP_DIAGNOSTICS
```

##### Required Functions (APPL API)

The APPL module must implement the following callbacks:

```c
appl.push_frame(const vecu_frame_t* in);   // Inbound
appl.poll_frame(vecu_frame_t* out);         // Outbound
```

Meaning:

- **`push_frame`** — called by the Loader, delivers inbound events (bus, runtime, OpenSUT)
- **`poll_frame`** — polled by the Loader, delivers outbound events (bus, runtime)

APPL is event‑driven, not actively sending.

##### APPL Execution Model (binding)

APPL code **may only:**

- Read/write shared memory
- Update internal state
- Place frames into internal queues

APPL code **must not:**

- Measure time itself
- Call `sleep()`
- Call other modules directly
- Query system state

Time comes exclusively from the Loader tick.

#### HSM Module (Security / Crypto)

##### Role

The HSM module implements:

- Cryptographic primitives
- SecurityAccess (Seed/Key)
- Signature / verification
- Protection logic for APPL

It is **not a real hardware HSM**, but a deterministic security abstraction
for vECUs.

##### Required Capabilities (ABI)

The HSM module must set at least:

```c
VECU_CAP_HSM_SEED_KEY
```

Optional:

```c
VECU_CAP_SIGN_VERIFY
```

##### Required Functions (HSM API)

The HSM module must implement:

```c
hsm.seed(...)
hsm.key(...)
```

Optional:

```c
hsm.sign(...)
hsm.verify(...)
```

All functions are purely functional (no side effects outside the module).

##### HSM Execution Model

- HSM has no knowledge of buses
- HSM does not communicate directly with APPL
- Interaction occurs:
  - Through shared memory
  - Or through explicit Loader calls

HSM is purely service‑providing.

#### Joint Orchestration (APPL + HSM)

##### Mandatory Tick Sequence

The Loader calls modules **always** in the following order:

```text
1. APPL.push_frame(...)  [0..N]
2. HSM.step(tick)
3. APPL.step(tick)
4. APPL.poll_frame(...)   [0..N]
```

Meaning:

- APPL can prepare security requests
- HSM processes security logic
- APPL uses results in the same tick

**Determinism guaranteed.**

##### Shared Memory Usage

APPL and HSM:

- Receive the same shared memory region
- Know only offsets, no pointers to each other
- Must not claim ownership of memory

Synchronization is implicit through the Loader sequence, not through locks.

#### OpenSUT Compatibility

APPL/HSM are OpenSUT‑compatible because:

- They represent a vECU
- They are deterministically start/stop‑able
- They interact through clearly defined interfaces

They:

- Do not implement OpenSUT
- Have no knowledge of the OpenSUT protocol

The Loader acts as the OpenSUT execution backend.

#### Error Semantics

A module may signal errors, but:

- Must **not** terminate the process
- Must not throw exceptions across the ABI
- Must not create undefined states

The Loader decides on abort or continuation.

#### Implementation Guidelines (directly codeable)

**Recommended languages:**

- Rust (`cdylib`) — preferred
- C / C++

**Mandatory rules:**

- `extern "C"`
- `#[repr(C)]`
- No panics across the ABI
- No threading
- No I/O

#### Rejected Alternatives

| Alternative                  | Reason                        |
| ---------------------------- | ----------------------------- |
| VTT binary compatibility     | Unnecessary                   |
| Direct APPL ↔ HSM calls     | Breaks encapsulation          |
| Thread‑based modules         | Not deterministic             |
| Tool‑dependent APIs          | Lock‑in                       |

#### Result

APPL and HSM are defined as:

> **Deterministic, platform‑neutral functional modules orchestrated
> exclusively by the vECU Loader.**

In combination with ADR‑001, the architecture is:

- Fully specified
- Tool‑free
- Platform‑neutral
- Directly implementable

### ADR‑003: Shared Memory Layout & Runtime Interaction Contract

**Status:** Accepted

**Reference:** This ADR is binding together with:

- ADR‑001: vECU Loader
- ADR‑002: APPL & HSM Modules

Without compliance with this ADR, determinism, interchangeability, and
OpenSUT compatibility are not guaranteed.

#### Context

The vECU Runtime Environment uses shared memory as:

- The sole shared data space between Loader, APPL, and HSM
- A decoupling mechanism (no direct function calls APPL↔HSM)
- The foundation for deterministic simulation

Previously, shared memory was only implicitly described.
This ADR makes it **explicit, versioned, and implementable**.

#### Decision

We define an offset‑based, versioned shared memory layout that:

- Is allocated and initialized by the Loader
- Is only read/written by APPL and HSM
- Does not allow pointer exchange between modules
- Is deterministic (no locks required)
- Remains ABI‑stable across versions

#### Core Principles

- **Offsets instead of struct growth** → ABI‑stable
- **Single‑writer rules per region** → no locking
- **Loader orchestrates** → implicit synchronization
- **Header with magic + version** → robust against misconfiguration

#### Shared Memory Top‑Level Layout

```text
+----------------------------------------------------+
| vecu_shm_header                                    |
+----------------------------------------------------+
| RX Frame Queue   (Runtime → APPL)                  |
+----------------------------------------------------+
| TX Frame Queue   (APPL → Runtime)                  |
+----------------------------------------------------+
| Diagnostic Mailbox (APPL ↔ HSM ↔ Runtime)          |
+----------------------------------------------------+
| Variable / State Block (APPL & HSM)                |
+----------------------------------------------------+
| Reserved / Future Extensions                       |
+----------------------------------------------------+
```

#### Mandatory Header (`vecu_shm_header_t`)

```c
typedef struct vecu_shm_header_t {
    uint32_t magic;          // 'VECU'
    uint16_t abi_major;
    uint16_t abi_minor;
    uint64_t total_size;

    // Offsets (from base)
    uint64_t off_rx_frames;
    uint64_t off_tx_frames;
    uint64_t off_diag_mb;
    uint64_t off_vars;

    // Sizes
    uint32_t size_rx_frames;
    uint32_t size_tx_frames;
    uint32_t size_diag_mb;
    uint32_t size_vars;

    uint32_t flags;          // future use
    uint32_t reserved;
} vecu_shm_header_t;
```

**Rules:**

- `magic` must be validated
- Offsets must never change, only be extended
- Versions govern backward compatibility

#### RX / TX Frame Queues

**Model:**

- Ring buffer
- Single writer / single reader
- No locks required

**Ownership:**

| Region    | Writer           | Reader           |
| --------- | ---------------- | ---------------- |
| RX Frames | Runtime / Loader | APPL             |
| TX Frames | APPL             | Runtime / Loader |

#### Diagnostic Mailbox

**Purpose:**

- Diagnostic requests
- Security requests (Seed/Key)
- Status responses

**Interaction:**

- APPL writes request
- HSM processes
- Result is placed in the same region
- Loader guarantees tick order

No direct function call APPL→HSM.

#### Variable / State Block

**Purpose:**

- ECU‑internal state
- Debug / trace information
- DIO / signal simulation

**Rules:**

- Structure is project‑specific
- Layout is version‑dependent
- Loader does not know the contents

#### Synchronization Model

- No mutexes
- No atomics required
- Synchronization occurs exclusively through:
  - Loader tick sequence (ADR‑001)
  - Clear ownership per region

**Determinism is guaranteed.**

#### Error Cases

The Loader must abort when:

- `magic` is incorrect
- ABI version is incompatible
- Offsets lie outside the shared memory size

#### Result

Shared memory is defined as:

> **A stable, versioned data exchange space that fully decouples APPL
> and HSM and enables deterministic simulation.**

---

## C4 Architecture

### L1: System Context

```text
+------------------------------------------------------------------+
|                                                                  |
|  User / CI / OpenSUT Client                                      |
|  --------------------------------------------------------------  |
|  - Developer                                                     |
|  - CI Pipeline                                                   |
|  - OpenSUT-compatible Tooling                                    |
|                                                                  |
|                |                                                 |
|                | executes / controls                              |
|                v                                                 |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |                                                            |  |
|  |  vECU Execution System                                     |  |
|  |  --------------------------------------------------------  |  |
|  |  Deterministic, cross-platform execution of virtual ECUs   |  |
|  |  OpenSUT-compatible                                        |  |
|  |  No proprietary tool dependencies                           |  |
|  |                                                            |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### L2: Container Diagram

```text
+----------------------------------------------------------------------------------+
|                                                                                  |
|  vECU Execution System                                                           |
|                                                                                  |
|  +---------------------------+        +---------------------------+              |
|  |                           |        |                           |              |
|  |  vECU Loader              |------->|  vECU Runtime Environment |              |
|  |                           |        |                           |              |
|  |  - CLI / config.yaml      |        |  - Simulation loop        |              |
|  |  - Dynamic module loading |        |  - Bus abstraction        |              |
|  |  - ABI negotiation        |        |  - OpenSUT semantics      |              |
|  |  - Tick orchestration     |        |                           |              |
|  |                           |        +---------------------------+              |
|  +---------------------------+                                                   |
|           |          |                                                           |
|           | loads    | loads                                                     |
|           v          v                                                           |
|                                                                                  |
|  +---------------------------+        +---------------------------+              |
|  |                           |        |                           |              |
|  |  APPL Module              |<------>|  Shared Memory            |              |
|  |                           |        |                           |              |
|  |  - ECU application logic  |        |  - RX frame queue         |              |
|  |  - State machines         |        |  - TX frame queue         |              |
|  |  - Event handling         |        |  - Diagnostic mailbox     |              |
|  |                           |        |  - Variable/state block   |              |
|  +---------------------------+        +---------------------------+              |
|                                                ^                                 |
|                                                | reads/writes                    |
|                                                |                                 |
|  +---------------------------+                 |                                  |
|  |                           |-----------------+                                 |
|  |  HSM Module               |                                                   |
|  |                           |  (no direct APPL↔HSM dependency;                  |
|  |  - Crypto primitives      |   interaction only via Shared Memory)             |
|  |  - SecurityAccess         |                                                   |
|  |  - Sign / verify          |                                                   |
|  |                           |                                                   |
|  +---------------------------+                                                   |
|                                                                                  |
+----------------------------------------------------------------------------------+
```

### L3: Component Diagram – vECU Loader

```text
+--------------------------------------------------------------------------+
|                                                                          |
|  vECU Loader                                                             |
|                                                                          |
|  +----------------------+                                                |
|  | CLI / Config Parser  |                                                |
|  |----------------------|                                                |
|  | - parses config.yaml |                                                |
|  | - resolves module    |                                                |
|  |   paths              |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Dynamic Module Loader|                                                |
|  |----------------------|                                                |
|  | - LoadLibrary /      |                                                |
|  |   dlopen             |                                                |
|  | - resolves           |                                                |
|  |   vecu_get_api       |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | ABI Negotiator       |                                                |
|  |----------------------|                                                |
|  | - checks ABI version |                                                |
|  | - validates role     |                                                |
|  | - reads capabilities |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Shared Memory Manager|                                                |
|  |----------------------|                                                |
|  | - allocates memory   |                                                |
|  | - initializes header |                                                |
|  | - validates layout   |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Tick Orchestrator    |                                                |
|  |----------------------|                                                |
|  | - deterministic loop |                                                |
|  | - enforces call      |                                                |
|  |   order              |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Runtime Adapter      |                                                |
|  |----------------------|                                                |
|  | - connects to vECU   |                                                |
|  |   Runtime Env        |                                                |
|  | - OpenSUT semantics  |                                                |
|  +----------------------+                                                |
|                                                                          |
+--------------------------------------------------------------------------+
```

### L4: Component Diagram – APPL & HSM Modules

**APPL Module:**

```text
+------------------------------------------------------+
|                                                      |
|  APPL Module                                        |
|                                                      |
|  +--------------------------+                        |
|  | Event Ingress             |                        |
|  |--------------------------|                        |
|  | - push_frame()            |                        |
|  | - inbound events          |                        |
|  +--------------------------+                        |
|              |                                       |
|              v                                       |
|  +--------------------------+                        |
|  | Application Logic         |                        |
|  |--------------------------|                        |
|  | - state machines          |                        |
|  | - ECU behavior            |                        |
|  +--------------------------+                        |
|              |                                       |
|              v                                       |
|  +--------------------------+                        |
|  | Event Egress              |                        |
|  |--------------------------|                        |
|  | - poll_frame()            |                        |
|  | - outbound events         |                        |
|  +--------------------------+                        |
|                                                      |
+------------------------------------------------------+
```

**HSM Module:**

```text
+------------------------------------------------------+
|                                                      |
|  HSM Module                                         |
|                                                      |
|  +--------------------------+                        |
|  | Security Interface        |                        |
|  |--------------------------|                        |
|  | - seed()                  |                        |
|  | - key()                   |                        |
|  +--------------------------+                        |
|              |                                       |
|              v                                       |
|  +--------------------------+                        |
|  | Crypto Engine              |                        |
|  |--------------------------|                        |
|  | - signing                 |                        |
|  | - verification            |                        |
|  +--------------------------+                        |
|                                                      |
+------------------------------------------------------+
```

### C4 Summary

- **L1** shows: Who uses the system and for what purpose
- **L2** shows: Which containers exist and how they interact
- **L3** shows: How the Loader is structured internally
- **L4** shows: How APPL and HSM are logically organized internally

All levels:

- Are tool‑free
- Are platform‑neutral
- Are deterministic
- Conform exactly to ADR‑001 through ADR‑003

---

## ADR‑004: OpenSUT API Abstraction & Runtime Adapter Traits

**Status:** Accepted

### Context

The vECU Runtime Environment requires two orthogonal abstractions:

1. **Tick source:** When are ticks triggered? (Timer loop vs. SIL Kit virtual time sync)
2. **Frame routing:** Where are frames sent? (SHM queues, SIL Kit CAN, hardware)

Previously, both aspects were implicitly coupled in the Loader/Runtime. For OpenSUT‑compatible
co‑simulation and alternative backends they must be **explicitly separated**.

### Decision

Two independent traits in `vecu-runtime`:

#### `RuntimeAdapter` — Tick Source

```rust
pub trait RuntimeAdapter {
    fn run(&mut self, runtime: &mut Runtime) -> Result<(), RuntimeError>;
}
```

| Implementation      | Crate           | Description                          |
|---------------------|-----------------|--------------------------------------|
| `StandaloneAdapter` | `vecu-runtime`  | Fixed tick count in tight loop       |
| `SilKitAdapter`     | `vecu-silkit`   | SIL Kit lifecycle + `TimeSyncService`|

#### `OpenSutApi` — Frame Routing (Bus Abstraction)

```rust
pub trait OpenSutApi: Send {
    fn recv_inbound(&mut self, out: &mut Vec<VecuFrame>) -> Result<(), RuntimeError>;
    fn dispatch_outbound(&mut self, frames: &[VecuFrame]) -> Result<(), RuntimeError>;
    fn on_start(&mut self) -> Result<(), RuntimeError>;
    fn on_stop(&mut self) -> Result<(), RuntimeError>;
}
```

| Implementation         | Crate          | Description                                    |
|------------------------|----------------|------------------------------------------------|
| `NullBus`              | `vecu-runtime` | No‑op (test / standalone without external buses)|
| `SilKitBus`            | `vecu-silkit`  | SIL Kit multi‑bus controllers via shared RX buffer |
| *(SHM fallback)*       | `vecu-runtime` | When no bus is set: SHM RX/TX queues directly  |

#### Per‑Module Bus Assignment

Each module can receive its **own** `OpenSutApi` bus:

```rust
runtime.set_appl_bus(Box::new(silkit_bus));   // APPL ↔ CAN+ETH+LIN+FR
runtime.set_hsm_bus(Box::new(rbs_bus));       // HSM  ↔ RBS
```

- `set_appl_bus()` — bus for the APPL module (SHM fallback when not set)
- `set_hsm_bus()` — bus for the HSM module (no fallback: without bus no frame I/O for HSM)
- `set_bus()` — convenience alias for `set_appl_bus()` (backward‑compatible)

#### Integration in `Runtime::tick()`

```text
for each simulation tick:
  1a. appl_bus.recv_inbound()      ──→ inbound_buf   (or SHM RX fallback)
  1b. APPL.push_frame(inbound)     ─── inbound_buf
  2a. hsm_bus.recv_inbound()       ──→ inbound_buf   (only if hsm_bus is set)
  2b. HSM.push_frame(inbound)      ─── inbound_buf
  3.  HSM.step(tick)
  4.  APPL.step(tick)
  5a. HSM.poll_frame()             ──→ outbound_buf
  5b. hsm_bus.dispatch_outbound()  ─── outbound_buf  (only if hsm_bus is set)
  6a. APPL.poll_frame()            ──→ outbound_buf
  6b. appl_bus.dispatch_outbound() ─── outbound_buf  (or SHM TX fallback)
```

#### Lifecycle Hooks

- `on_start()` is called in `Runtime::init_all()` on **both** buses (after module init)
- `on_stop()` is called in `Runtime::shutdown_all()` on **both** buses (before module shutdown)

### Separation of Concerns

```text
┌─────────────────────────────────────────────────────────────┐
│  vECU Runtime Env                                           │
│                                                             │
│  RuntimeAdapter (WHEN)                                      │
│  ┌──────────────────┐                                       │
│  │ StandaloneAdapter │                                       │
│  │ SilKitAdapter     │                                       │
│  └───────┴──────────┘                                       │
│          │                                                   │
│          └──────── Runtime::tick() ──────────────┐          │
│                    │                        │          │
│          ┌────────┴────────┐    ┌────────┴─────┐    │
│          │  APPL             │    │  HSM            │    │
│          └────────┴────────┘    └────────┴─────┘    │
│                  │                        │          │
│          ┌────────┴────────┐    ┌────────┴─────┐    │
│          │  appl_bus         │    │  hsm_bus        │    │
│          │  (OpenSutApi)     │    │  (OpenSutApi)   │    │
│          └─────────────────┘    └──────────────┘    │
│               │                     │               │
│          CAN/ETH/LIN/FR          CAN/LIN/FR           │
└─────────────────────────────────────────────────────────────┘
```

### Consequences

- **Per‑module bus assignment**: APPL and HSM can each have their own `OpenSutApi` instances
- New bus backends (TAP bridge, RBS, hardware) only need to implement `OpenSutApi`
- New tick sources (HiL, replay) only need to implement `RuntimeAdapter`
- Existing standalone mode is unchanged (SHM fallback for APPL)
- HSM frame I/O is only active when `hsm_bus` is set (no SHM fallback for HSM)
- `SilKitBus` uses `Arc<Mutex<Vec<VecuFrame>>>` as a thread‑safe RX buffer
- Multi‑bus: CAN, Ethernet, LIN, and FlexRay controllers implemented

---

## Key Properties

- Cross‑platform (Windows / Linux / macOS)
- Deterministic execution
- OpenSUT‑compatible
- No proprietary dependencies
- CI‑ready
- Rust/C/C++ friendly

---

## Non‑Goals

- Binary compatibility with VTT
- Tool‑specific APIs
- Threaded execution in modules

---

## Summary

The vECU architecture cleanly separates:
- orchestration (Loader),
- execution (Runtime),
- tick source (RuntimeAdapter),
- bus I/O (OpenSutApi),
- logic (APPL),
- security (HSM),
using a stable ABI, shared memory and two orthogonal trait abstractions.

This enables scalable, license‑free and reproducible vECU execution
across standalone, SIL Kit co‑simulation and future hardware backends.
