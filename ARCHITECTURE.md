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
- Conform exactly to ADR‑001 through ADR‑005

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

## ADR‑005: APPL Module with External C‑Code and AUTOSAR BaseLayer

**(VTT‑equivalent SiL coverage, runtime‑linked BaseLayer, no proprietary toolchain)**

**Status:** Proposed

**Reference:** This ADR extends ADR‑002 (APPL Module) and depends on:

- ADR‑001: vECU Loader ABI
- ADR‑002: APPL & HSM Module Roles
- ADR‑003: Shared Memory Layout
- ADR‑004: OpenSUT API Abstraction

### Context

ADR‑002 defines the APPL module as a **logical role** for ECU application logic.
The current reference implementation is a pure‑Rust echo stub. For production
use, the APPL module must execute **real ECU application C‑code** — the same
source code that runs on the target microcontroller (e.g. Infineon TC4x,
Renesas RH850).

This is the standard approach for Software‑in‑the‑Loop (SiL) testing:

- **Vector VTT** re‑compiles ECU C‑code for Windows x86/x64 and links it
  against VTT‑proprietary BSW stubs to produce `appl.dll`.
- **Synopsys Silver** / **dSPACE VEOS** follow similar patterns.

Our goal: **same functional coverage as VTT `appl.dll`**, but:

- Cross‑platform (Windows, Linux, macOS)
- No proprietary toolchain dependency
- BaseLayer as a runtime‑linked shared library (exchangeable per project)
- Full integration with our HSM module for crypto delegation

### Decision

The APPL module is split into three layers, loaded at runtime:

1. **vECU APPL Bridge** (`vecu-appl`, Rust) — the ABI‑compliant wrapper that
   implements `vecu_get_api()` and delegates to the C‑code layers below.
2. **BaseLayer** (`libbase.so` / `base.dll`) — AUTOSAR BSW stubs compiled for
   the host platform. Provides the BSW API surface that the ECU application
   code expects. Supplied once per AUTOSAR release, reusable across projects.
3. **Application Code** (`libappl_ecu.so` / `appl_ecu.dll`) — the actual ECU
   C/C++ source code, compiled for the host platform and linked against the
   BaseLayer.

```text
┌──────────────────────────────────────────────────────────┐
│  vECU Loader (Rust)                                      │
│  ┌────────────────────────────────────────────────────┐  │
│  │  vecu-appl  (Rust, cdylib)                         │  │
│  │  ┌──────────────────────────────────────────────┐  │  │
│  │  │  ABI Bridge                                  │  │  │
│  │  │  vecu_get_api() → init / step / shutdown     │  │  │
│  │  │  push_frame()   → Com_RxIndication()         │  │  │
│  │  │  poll_frame()   ← Com_TriggerTransmit()      │  │  │
│  │  └──────────────┬───────────────────────────────┘  │  │
│  │                 │ dlopen + callback injection       │  │
│  │  ┌──────────────▼───────────────────────────────┐  │  │
│  │  │  libbase.so  (C, AUTOSAR BSW Stubs)          │  │  │
│  │  │  Os │ Com │ Dcm │ Csm │ NvM │ Det │ ...     │  │  │
│  │  └──────────────┬───────────────────────────────┘  │  │
│  │                 │ links against (compile‑time)      │  │
│  │  ┌──────────────▼───────────────────────────────┐  │  │
│  │  │  libappl_ecu.so  (C/C++, ECU Application)    │  │  │
│  │  │  SWC Runnables │ RTE │ Application Logic     │  │  │
│  │  └──────────────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

**The BaseLayer is runtime‑linked, not compiled into vecu‑appl.**

### AUTOSAR BSW Module Coverage (VTT‑Equivalent)

The BaseLayer must provide stubs or functional implementations for the
following AUTOSAR Classic BSW modules. This is the minimum set required
to achieve VTT `appl.dll`‑equivalent coverage.

#### System Services

| BSW Module | AUTOSAR Role | BaseLayer Implementation |
|------------|-------------|--------------------------|
| **EcuM** | ECU State Manager | Maps vECU lifecycle (`init`/`step`/`shutdown`) to AUTOSAR states (STARTUP → RUN → SHUTDOWN). Calls `EcuM_Init()`, `EcuM_MainFunction()`. |
| **BswM** | BSW Mode Manager | Rule‑based mode arbitration. Stub with configurable mode rules. |
| **SchM** | Schedule Manager | Maps to our `step(tick)`. Drives `*_MainFunction()` calls for all BSW modules in deterministic order. |
| **Os** | Operating System | Task activation, alarm management, counters. Single‑threaded emulation — all tasks run sequentially within `step()`. No real OS scheduling. |
| **Det** | Default Error Tracer | Routes `Det_ReportError()` → `vecu_log_fn` callback from `VecuRuntimeContext`. |

#### Communication

| BSW Module | AUTOSAR Role | BaseLayer Implementation |
|------------|-------------|--------------------------|
| **Com** | Communication | Signal‑based I/O. `Com_SendSignal()` packs signals into PDUs. `Com_ReceiveSignal()` unpacks PDUs into signals. Signal database loaded from configuration (JSON/ARXML‑extract). |
| **PduR** | PDU Router | Routes PDUs between Com, Dcm, CanTp, DoIP. Routing table from configuration. |
| **CanIf** | CAN Interface | Maps to `push_frame()` / `poll_frame()` with `BusType::Can`. |
| **LinIf** | LIN Interface | Maps to `push_frame()` / `poll_frame()` with `BusType::Lin`. |
| **EthIf** | Ethernet Interface | Maps to `push_frame()` / `poll_frame()` with `BusType::Eth`. |
| **FrIf** | FlexRay Interface | Maps to `push_frame()` / `poll_frame()` with `BusType::FlexRay`. |
| **CanTp** | CAN Transport | ISO 15765‑2 segmentation and reassembly for UDS over CAN. |
| **DoIP** | Diagnostics over IP | ISO 13400 transport for UDS over Ethernet. |

#### Diagnostics

| BSW Module | AUTOSAR Role | BaseLayer Implementation |
|------------|-------------|--------------------------|
| **Dcm** | Diagnostic Communication Manager | UDS service dispatch (ISO 14229). Handles session management, service routing, response assembly. |
| **Dem** | Diagnostic Event Manager | DTC storage, status management, snapshot/extended data. DTCs stored in SHM variable block or NvM. |
| **FiM** | Function Inhibition Manager | Inhibits SWC functions based on DTC status from Dem. |

#### Crypto (delegation to HSM)

| BSW Module | AUTOSAR Role | BaseLayer Implementation |
|------------|-------------|--------------------------|
| **Csm** | Crypto Service Manager | Delegates all operations to HSM module via callback injection: `Csm_Encrypt()` → `ctx.hsm_encrypt()`, `Csm_MacGenerate()` → `ctx.hsm_generate_mac()`, `Csm_RandomGenerate()` → `ctx.hsm_rng()`. |
| **CryIf** | Crypto Interface | Thin routing layer between Csm and Cry driver (our HSM). |
| **Cry** | Crypto Driver | Direct HSM callback wrappers. |

#### Memory

| BSW Module | AUTOSAR Role | BaseLayer Implementation |
|------------|-------------|--------------------------|
| **NvM** | NV Memory Manager | Block‑based read/write. Backed by SHM variable block (`off_vars`) or optional file persistence. `NvM_ReadBlock()` / `NvM_WriteBlock()` / `NvM_MainFunction()`. |
| **MemIf** | Memory Abstraction | Routes NvM requests to Fee or Ea. |
| **Fee** | Flash EEPROM Emulation | Emulated in RAM (SHM vars block) or backed by host file. |

#### Watchdog

| BSW Module | AUTOSAR Role | BaseLayer Implementation |
|------------|-------------|--------------------------|
| **WdgM** | Watchdog Manager | Alive supervision. Monitors checkpoint sequences from SWCs. Reports to Det on timeout. No real hardware watchdog — purely logical. |

#### Runtime Environment

| BSW Module | AUTOSAR Role | BaseLayer Implementation |
|------------|-------------|--------------------------|
| **Rte** | Runtime Environment | Connects SWC ports to BSW services. `Rte_Read_*()` / `Rte_Write_*()` map to Com signal accessors. `Rte_Call_*()` maps to Csm/Dcm service calls. Generated per‑project from SWC descriptions. |

### Callback Injection (BaseLayer ↔ vECU Runtime)

The BaseLayer cannot call our Rust ABI directly. Instead, `vecu-appl`
injects a **callback context** struct during initialization. This is the
sole coupling point between the C BaseLayer and the vECU runtime.

```c
/* vecu_base_context.h — shipped with vecu-abi */

#ifndef VECU_BASE_CONTEXT_H
#define VECU_BASE_CONTEXT_H

#include <stdint.h>

typedef struct vecu_frame_t vecu_frame_t;  /* from vecu-abi */

typedef struct vecu_base_context_t {
    /* ── Frame I/O (BaseLayer → Loader) ─────────────────────── */
    int (*push_tx_frame)(const vecu_frame_t* frame);
    int (*pop_rx_frame)(vecu_frame_t* frame);

    /* ── HSM Crypto Delegation ──────────────────────────────── */
    int (*hsm_encrypt)(uint32_t slot, uint32_t mode,
                       const uint8_t* data, uint32_t data_len,
                       const uint8_t* iv,
                       uint8_t* out, uint32_t* out_len);
    int (*hsm_decrypt)(uint32_t slot, uint32_t mode,
                       const uint8_t* data, uint32_t data_len,
                       const uint8_t* iv,
                       uint8_t* out, uint32_t* out_len);
    int (*hsm_generate_mac)(uint32_t slot,
                            const uint8_t* data, uint32_t data_len,
                            uint8_t* out_mac, uint32_t* out_mac_len);
    int (*hsm_verify_mac)(uint32_t slot,
                          const uint8_t* data, uint32_t data_len,
                          const uint8_t* mac, uint32_t mac_len);
    int (*hsm_seed)(uint8_t* out_seed, uint32_t* out_len);
    int (*hsm_key)(const uint8_t* key_buf, uint32_t key_len);
    int (*hsm_rng)(uint8_t* out_buf, uint32_t buf_len);

    /* ── Shared Memory ──────────────────────────────────────── */
    void*    shm_vars;         /* pointer to SHM variable block    */
    uint32_t shm_vars_size;    /* size of variable block in bytes  */

    /* ── Logging ────────────────────────────────────────────── */
    void (*log_fn)(uint32_t level, const char* msg);

    /* ── Time ───────────────────────────────────────────────── */
    uint64_t tick_interval_us; /* microseconds per tick            */
} vecu_base_context_t;

/* BaseLayer must export these symbols */
void Base_Init(const vecu_base_context_t* ctx);
void Base_Step(uint64_t tick);
void Base_Shutdown(void);

/* Application code must export these symbols */
void Appl_Init(void);
void Appl_MainFunction(void);  /* called each tick by SchM */
void Appl_Shutdown(void);

#endif /* VECU_BASE_CONTEXT_H */
```

**Rules:**

- All callback pointers are set by `vecu-appl` before calling `Base_Init()`
- The BaseLayer must **never** call OS APIs, allocate heap, or start threads
- All memory comes from the vECU Loader (SHM, callback context)
- The BaseLayer stores the context pointer internally for the duration of
  `Base_Init()` through `Base_Shutdown()`

### Lifecycle Mapping

```text
┌───────────────────────────────────────────────────────────────┐
│ vECU Loader Lifecycle          AUTOSAR EcuM States            │
│                                                               │
│ vecu_get_api()                                                │
│     │                                                         │
│     ▼                                                         │
│ appl_init(ctx)                                                │
│     │── dlopen(libbase.so)                                    │
│     │── dlopen(libappl_ecu.so)                                │
│     │── Build vecu_base_context_t with callbacks              │
│     │── Base_Init(ctx)        ──→  EcuM_Init()                │
│     │       │── Os_Init()          │  ECUM_STATE_STARTUP      │
│     │       │── Com_Init()         │                          │
│     │       │── Dcm_Init()         │                          │
│     │       │── NvM_Init()         │                          │
│     │       │── Csm_Init()         │                          │
│     │       └── ...                │                          │
│     │── Appl_Init()           ──→  Rte_Start()                │
│     │                              ECUM_STATE_APP_RUN         │
│     ▼                                                         │
│ appl_step(tick)  [repeated]                                   │
│     │── SchM_MainFunction()   ──→  EcuM_MainFunction()        │
│     │       │── Com_MainFunction()                            │
│     │       │── Dcm_MainFunction()                            │
│     │       │── NvM_MainFunction()                            │
│     │       │── WdgM_MainFunction()                           │
│     │       └── ...                                           │
│     │── Appl_MainFunction()   ──→  SWC Runnables              │
│     │                              (via Rte_Read / Rte_Write) │
│     ▼                                                         │
│ appl_shutdown()                                               │
│     │── Appl_Shutdown()       ──→  Rte_Stop()                 │
│     │── Base_Shutdown()       ──→  EcuM_GoSleep()             │
│     │       │── NvM_WriteAll()     ECUM_STATE_SHUTDOWN        │
│     │       │── Com_DeInit()                                  │
│     │       └── Os_Shutdown()                                 │
│     │── dlclose(libappl_ecu.so)                               │
│     └── dlclose(libbase.so)                                   │
└───────────────────────────────────────────────────────────────┘
```

### Frame I/O Mapping

The BaseLayer interface modules (CanIf, LinIf, EthIf, FrIf) do **not**
access hardware or OS sockets. They delegate to the callback context:

```text
ECU C‑Code                BaseLayer               vecu-appl
──────────                ─────────               ─────────
Com_SendSignal()
  → Com packs PDU
    → PduR routes
      → CanIf_Transmit()
        → ctx.push_tx_frame()  ─────→  poll_frame() to Loader

push_frame() from Loader  ─────→  ctx.pop_rx_frame()
  → CanIf_RxIndication()
    → PduR routes
      → Com_RxIndication()
        → Rte_Read_*() returns updated signal
```

**All four bus types (CAN, LIN, Ethernet, FlexRay) follow this pattern.**

The `BusType` field in `VecuFrame` determines routing to the correct
interface module (CanIf, LinIf, EthIf, FrIf).

### Diagnostic Integration (UDS via DiagMailbox + Direct HSM)

Two parallel diagnostic paths exist:

#### Path A: Full Dcm Stack (production‑equivalent)

```text
CAN/DoIP Frame (UDS Request)
  → CanTp / DoIP reassembly
    → PduR → Dcm
      → Dcm dispatches SID:
          0x10 DiagnosticSessionControl  → Dcm internal
          0x11 ECUReset                  → EcuM
          0x22 ReadDataByIdentifier      → Rte / NvM
          0x27 SecurityAccess            → Csm → ctx.hsm_seed() / ctx.hsm_key()
          0x2E WriteDataByIdentifier     → Rte / NvM
          0x31 RoutineControl            → SWC Runnables
          0x34 RequestDownload           → Memory services
          0x3E TesterPresent             → Dcm internal
      → Dcm builds response PDU
    → PduR → CanTp / DoIP
  → ctx.push_tx_frame()
```

#### Path B: DiagMailbox Shortcut (simplified, existing)

```text
APPL writes DiagMailbox.request_pending = 1
  → HSM.step() processes → DiagMailbox.response_ready = 1
  → APPL reads response in same tick
```

Path A is the production‑equivalent path. Path B remains as a lightweight
alternative for simple seed/key without full UDS framing.

### Loader Configuration Extension

The loader config (`config.yaml`) is extended with paths to the
BaseLayer and application libraries:

```yaml
appl:
  bridge: "target/release/libvecu_appl.dylib"   # Rust ABI bridge
  base_layer: "vendor/libbase.so"                # AUTOSAR BSW stubs
  ecu_code: "vendor/libappl_ecu.so"              # ECU application code
  base_config: "vendor/base_config.json"         # BaseLayer configuration

hsm:
  path: "target/release/libvecu_hsm.dylib"

shm:
  queue_capacity: 64
  vars_size: 65536                               # 64 KiB for NvM blocks
```

The `base_config.json` contains project‑specific configuration:

- Com signal database (signal name → PDU ID, bit position, length, endianness)
- PduR routing table
- Dcm service configuration (supported SIDs, sub‑functions)
- NvM block descriptors (block ID → offset in vars block, size)
- DEM DTC configuration
- EcuM / BswM mode rules

### Build Pipeline

```text
┌─────────────────────────────────────────────────────────┐
│  Build Step 1: BaseLayer (once per AUTOSAR release)     │
│                                                         │
│  vecu_base_context.h                                    │
│  + AUTOSAR BSW stub sources (.c/.h)                     │
│  + base_config.json                                     │
│     │                                                   │
│     │  cc -shared -fPIC -o libbase.so                   │
│     ▼                                                   │
│  libbase.so / base.dll                                  │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  Build Step 2: ECU Application (per project)            │
│                                                         │
│  ECU C/C++ sources (.c/.h)                              │
│  + Generated RTE headers                                │
│  + libbase.so (link dependency)                         │
│     │                                                   │
│     │  cc -shared -fPIC -lbase -o libappl_ecu.so        │
│     ▼                                                   │
│  libappl_ecu.so / appl_ecu.dll                          │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  Build Step 3: vECU APPL Bridge (cargo build)           │
│                                                         │
│  vecu-appl crate (Rust)                                 │
│     │                                                   │
│     │  cargo build --release                            │
│     ▼                                                   │
│  libvecu_appl.so / vecu_appl.dll                        │
└─────────────────────────────────────────────────────────┘
```

**Key property:** Steps 1 and 3 are project‑independent. Only step 2
changes per ECU project. This mirrors VTT's model where the VTT runtime
is fixed and only the application code varies.

### C4 Component Diagram (Updated APPL Module)

```text
+----------------------------------------------------------------------+
|                                                                      |
|  APPL Module (ADR-005)                                               |
|                                                                      |
|  +--------------------------+                                        |
|  | ABI Bridge (Rust)        |                                        |
|  |--------------------------|                                        |
|  | vecu_get_api()           |                                        |
|  | init / step / shutdown   |                                        |
|  | push_frame / poll_frame  |                                        |
|  | dlopen(libbase, libappl) |                                        |
|  +-----------+--------------+                                        |
|              | vecu_base_context_t                                    |
|              v                                                       |
|  +--------------------------+     +-----------------------------+    |
|  | BaseLayer (C)            |     | ECU Application (C/C++)     |    |
|  |--------------------------|     |-----------------------------|    |
|  | EcuM  BswM  SchM  Os    |     | SWC Runnables               |    |
|  | Com   PduR  CanIf LinIf |     | Application State Machines  |    |
|  | EthIf FrIf  CanTp DoIP  |<--->| Rte_Read / Rte_Write        |    |
|  | Dcm   Dem   FiM         |     | Rte_Call (Csm, Dcm, ...)    |    |
|  | NvM   Fee   MemIf       |     |                             |    |
|  | Csm   CryIf Cry         |     +-----------------------------+    |
|  | Det   WdgM              |                                        |
|  +-----------+--------------+                                        |
|              | ctx.hsm_*() callbacks                                 |
|              v                                                       |
|  +--------------------------+                                        |
|  | HSM Module (vecu-hsm)    |                                        |
|  |--------------------------|                                        |
|  | AES-128 ECB/CBC          |                                        |
|  | CMAC Generate/Verify     |                                        |
|  | SecurityAccess           |                                        |
|  | Key Store (20 slots)     |                                        |
|  | CSPRNG                   |                                        |
|  +--------------------------+                                        |
|                                                                      |
+----------------------------------------------------------------------+
```

### Comparison with VTT

| Aspect | Vector VTT | vecu‑core (ADR‑005) |
|--------|-----------|----------------------|
| **ECU C‑code** | Re‑compiled for Windows x86 | Re‑compiled for host (Win/Linux/macOS) |
| **BSW Stubs** | VTT‑proprietary, closed source | Open BaseLayer, exchangeable per project |
| **HSM** | VTT HSM stub (simplified) or re‑compiled firmware | Full SHE‑compatible HSM (ADR‑002, real AES‑128) |
| **Bus Integration** | CANoe / VN hardware | OpenSutApi (SIL Kit, standalone, future HW) |
| **Diagnostics** | Dcm from real BSW or VTT stub | Dcm in BaseLayer + DiagMailbox fallback |
| **NvM** | File‑based emulation | SHM vars block + optional file persistence |
| **Platform** | Windows only | Windows, Linux, macOS |
| **Toolchain** | Visual Studio + VTT | gcc / clang / MSVC + cargo |
| **License** | Proprietary (per seat) | Open‑source (MIT / Apache‑2.0) |
| **Build model** | VTT project wizard | Makefile / CMake + cargo |

### Constraints

1. **No OS APIs in BaseLayer** — all I/O goes through callbacks
2. **No threads** — everything runs within `step()`, deterministic
3. **No heap allocation in BaseLayer** — use SHM vars block or
   pre‑allocated buffers sized at compile time
4. **C ABI only** — `extern "C"`, no C++ exceptions across boundaries
5. **No direct APPL↔HSM calls** — crypto goes through callback context
6. **BaseLayer must be stateless across ticks** — all persistent state
   in SHM vars block (survives only within a simulation run) or NvM
   file (survives across runs)
7. **RTE is project‑specific** — must be generated or hand‑written per
   ECU project to match SWC port definitions

### Rejected Alternatives

| Alternative | Reason |
|-------------|--------|
| Write all BSW stubs in Rust | Incompatible with existing C ECU code that expects AUTOSAR C headers |
| Static linking of BaseLayer into vecu‑appl | Prevents exchanging BaseLayer without recompiling the bridge |
| VTT binary compatibility | Creates proprietary lock‑in, Windows‑only |
| Single monolithic DLL | Cannot reuse BaseLayer across projects |
| Python/Lua scripting layer | Performance overhead, type safety loss, not compatible with real C code |
| Direct function calls APPL→HSM | Breaks ADR‑002 encapsulation principle |

### Implementation Roadmap

| Phase | Deliverable | Description |
|-------|-------------|-------------|
| **P1** | `vecu_base_context.h` | C header defining the callback context — the contract between Rust bridge and C BaseLayer |
| **P2** | `vecu-appl` FFI bridge | Rust code: `dlopen` + symbol resolution + callback injection + lifecycle delegation |
| **P3** | Reference BaseLayer | Minimal C BaseLayer with EcuM, SchM, Os, Com, Det, Dcm stubs — proves the architecture |
| **P4** | Crypto integration | Csm/CryIf/Cry stubs delegating to HSM callbacks — validates end‑to‑end SecurityAccess |
| **P5** | NvM + Dem | Persistent storage via SHM vars block, DTC management |
| **P6** | Transport layers | CanTp (ISO 15765‑2) and DoIP (ISO 13400) for segmented UDS |
| **P7** | Example project | Complete integration with a sample ECU C‑code package |

### Result

The APPL module with external C‑code and runtime‑linked BaseLayer is defined as:

> **A cross‑platform, VTT‑equivalent SiL execution environment that runs
> real ECU application C‑code against AUTOSAR BSW stubs, with full HSM
> crypto delegation, deterministic tick execution, and no proprietary
> toolchain dependency.**

In combination with ADR‑001 through ADR‑004, this completes the
architecture for production‑grade vECU simulation.

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
- logic (APPL + external C‑code + AUTOSAR BaseLayer),
- security (HSM with SHE‑compatible crypto),
using a stable ABI, shared memory, two orthogonal trait abstractions,
and runtime‑linked BaseLayer for VTT‑equivalent SiL coverage.

This enables scalable, license‑free and reproducible vECU execution
across standalone, SIL Kit co‑simulation and future hardware backends.
