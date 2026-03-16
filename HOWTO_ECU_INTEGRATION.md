# How To: ECU / AUTOSAR Software Integration

This guide explains how to integrate real ECU application C‑code into the
vECU execution system. It targets embedded software engineers who have
AUTOSAR Classic ECU source code and want to run it as a virtual ECU.

**Prerequisites:**

- ECU C/C++ source code (SWC runnables, application logic)
- Generated or hand‑written RTE headers for the target ECU
- Basic knowledge of AUTOSAR BSW module APIs
- C compiler (gcc, clang, or MSVC)
- CMake ≥ 3.16
- Rust toolchain (for building the vECU runtime)

---

## Table of Contents

1. [Overview](#1-overview)
2. [What You Need from the ECU Project](#2-what-you-need-from-the-ecu-project)
3. [Step‑by‑Step Integration](#3-step-by-step-integration)
4. [Building the BaseLayer](#4-building-the-baselayer)
5. [Building the ECU Application Library](#5-building-the-ecu-application-library)
6. [Configuration Files](#6-configuration-files)
7. [Running the vECU](#7-running-the-vecu)
8. [Diagnostic Testing (UDS)](#8-diagnostic-testing-uds)
9. [SecurityAccess with HSM](#9-securityaccess-with-hsm)
10. [Troubleshooting](#10-troubleshooting)
11. [Differences from Vector VTT](#11-differences-from-vector-vtt)
12. [Using the Vector AUTOSAR BSW Instead of Stubs](#12-using-the-vector-autosar-bsw-instead-of-stubs)
13. [Using Eclipse OpenBSW as an Open‑Source BaseLayer](#13-using-eclipse-openbsw-as-an-open-source-baselayer)

---

## 1. Overview

The vECU execution system runs your ECU application code on a host PC
(Windows, Linux, macOS) in a deterministic, tick‑based simulation.
The architecture has three layers:

```
┌─────────────────────────────────────────────┐
│  vECU Runtime (Rust)                        │
│  ┌───────────────────────────────────────┐  │
│  │  vecu-appl (ABI Bridge)               │  │
│  │  ┌─────────────────────────────────┐  │  │
│  │  │  YOUR ECU C‑CODE                │  │  │  ← you provide this
│  │  │  (libappl_ecu.so)               │  │  │
│  │  └─────────────┬───────────────────┘  │  │
│  │                │ calls BSW APIs        │  │
│  │  ┌─────────────▼───────────────────┐  │  │
│  │  │  BaseLayer (libbase.so)         │  │  │  ← we provide this
│  │  │  AUTOSAR BSW stubs              │  │  │
│  │  └─────────────────────────────────┘  │  │
│  └───────────────────────────────────────┘  │
│  ┌───────────────────────────────────────┐  │
│  │  vecu-hsm (SHE‑compatible crypto)     │  │  ← included
│  └───────────────────────────────────────┘  │
└─────────────────────────────────────────────┘
```

| You provide | We provide |
|-------------|------------|
| `SwcXxx.c` — your SWC application logic | `libbase.so` — 24 AUTOSAR BSW module stubs |
| `Rte_SwcXxx.h` — RTE headers (hand‑written or generated) | `libvecu_appl.so` — Rust ABI bridge |
| `Appl_Entry.c` — lifecycle hooks (init / main / shutdown) | `libvecu_hsm.so` — SHE‑compatible AES‑128 crypto |
| `Base_Entry.c` — signal, NvM, Dcm, CanTp configuration | `vecu‑loader` — tick‑based runtime + optional SIL Kit |

---

## 2. What You Need from the ECU Project

### Required Files

| What | Source | Example |
|------|--------|---------|
| **SWC source files** | Your ECU project | `SwcBodyCtrl.c`, `SwcDiag.c`, `SwcComm.c` |
| **SWC headers** | Your ECU project | `SwcBodyCtrl.h`, `SwcDiag.h` |
| **RTE headers** | AUTOSAR RTE generator or hand‑written | `Rte_SwcBodyCtrl.h`, `Rte_SwcDiag.h` |
| **Application entry** | You create (template provided) | `Appl_Entry.c` |

### What You Do NOT Need

- AUTOSAR BSW source code (we provide stubs)
- MCAL drivers (replaced by BaseLayer)
- OS configuration (replaced by deterministic tick)
- Compiler‑specific startup code
- Linker scripts
- Any Vector tooling

### Preparing Your C‑Code

Your ECU code typically calls these AUTOSAR APIs. The BaseLayer provides
all of them:

```c
/* Communication — reading/writing signals */
Com_ReceiveSignal(SignalId, &value);
Com_SendSignal(SignalId, &value);

/* Diagnostics — handled by Dcm */
/* (usually called via Rte_Call, not directly) */

/* Crypto — delegated to HSM */
Csm_Encrypt(jobId, 0 /* ECB=0, CBC=1 */, data, len, result, &resultLen);
Csm_MacGenerate(jobId, data, len, mac, &macLen);
Csm_RandomGenerate(jobId, result, &resultLen);

/* NV Memory — persistent storage */
NvM_ReadBlock(blockId, destPtr);
NvM_WriteBlock(blockId, srcPtr);

/* Error reporting */
Det_ReportError(moduleId, instanceId, apiId, errorId);
```

### Code Modifications Checklist

| # | Check | Action |
|---|-------|--------|
| 1 | **No direct hardware access** | Remove or `#ifdef` out any register access (`*(volatile uint32_t*)0xF0000000 = ...`) |
| 2 | **No inline assembly** | Remove or stub `__asm` / `asm()` blocks |
| 3 | **No compiler intrinsics** | Replace `__builtin_*` with portable equivalents or stubs |
| 4 | **No OS task creation** | Remove `Os_ActivateTask()` calls if they bypass the scheduler |
| 5 | **Standard C types** | Use `<stdint.h>` types or include our `Std_Types.h` |
| 6 | **No `#pragma` for memory sections** | Remove or make conditional with `#ifndef VECU_SIL` |

**Tip:** Use a preprocessor define `VECU_SIL` to conditionally compile
SiL‑incompatible code:

```c
#ifdef VECU_SIL
  /* Host‑compatible stub */
  #define HW_READ_REG(addr) (0u)
#else
  /* Real hardware access */
  #define HW_READ_REG(addr) (*(volatile uint32_t*)(addr))
#endif
```

---

## 3. Step‑by‑Step Integration

### Step 1: Create the Application Entry Point

Create `Appl_Entry.c` — this file connects your SWCs to the BaseLayer:

```c
/* Appl_Entry.c — Application entry point for vECU */

#include "Rte_SwcBodyCtrl.h"
#include "Rte_SwcDiag.h"
/* Include all your SWC headers */

void Appl_Init(void)
{
    /* Initialize your SWCs */
    SwcBodyCtrl_Init();
    SwcDiag_Init();
}

void Appl_MainFunction(void)
{
    /* Call your SWC runnables in the correct order.
       This is called once per tick (e.g., every 1 ms). */

    /* 10ms runnables — called every tick if tick = 1ms,
       or use a counter for slower rates */
    SwcBodyCtrl_MainFunction_10ms();

    /* 100ms runnables */
    static uint32_t counter = 0;
    counter++;
    if (counter % 100 == 0) {
        SwcDiag_MainFunction_100ms();
    }
}

void Appl_Shutdown(void)
{
    /* Cleanup your SWCs */
    SwcBodyCtrl_DeInit();
    SwcDiag_DeInit();
}
```

### Step 2: Create / Adapt RTE Headers

RTE headers define how your SWCs access signals and services.
Each SWC needs an RTE header:

```c
/* Rte_SwcBodyCtrl.h */
#ifndef RTE_SWCBODYCTRL_H
#define RTE_SWCBODYCTRL_H

#include "Std_Types.h"
#include "Com.h"

/* Signal IDs — must match Com_SignalConfigType in Base_Entry.c */
#define Rte_SignalId_VehicleSpeed    0
#define Rte_SignalId_DoorStatus      1
#define Rte_SignalId_LightCommand    2

/* Read a signal value (RX) */
static inline Std_ReturnType Rte_Read_VehicleSpeed(uint16* value)
{
    return Com_ReceiveSignal(Rte_SignalId_VehicleSpeed, value);
}

/* Write a signal value (TX) */
static inline Std_ReturnType Rte_Write_LightCommand(const uint8* value)
{
    return Com_SendSignal(Rte_SignalId_LightCommand, value);
}

/* Call crypto service (mode: 0 = ECB, 1 = CBC) */
static inline Std_ReturnType Rte_Call_Csm_Encrypt(
    uint32 jobId, uint32 mode, const uint8* data, uint32 len,
    uint8* result, uint32* resultLen)
{
    return Csm_Encrypt(jobId, mode, data, len, result, resultLen);
}

#endif /* RTE_SWCBODYCTRL_H */
```

**Note:** In a real AUTOSAR project, these headers are generated by the
RTE generator. For vECU integration, you can either:
- Use the generated headers directly (if they have no tool dependencies)
- Write them by hand (as shown above) — simpler, full control

### Step 3: Organize Your Source Files

```
my_ecu_project/
├── CMakeLists.txt
├── include/
│   ├── Rte_SwcBodyCtrl.h
│   ├── Rte_SwcDiag.h
│   ├── SwcBodyCtrl.h
│   └── SwcDiag.h
└── src/
    ├── Appl_Entry.c
    ├── SwcBodyCtrl.c
    └── SwcDiag.c
```

### Step 4: Build

See sections 4 and 5 below.

### Step 5: Configure

See section 6 below.

### Step 6: Run

See section 7 below.

---

## 4. Building the BaseLayer

The BaseLayer is provided as C source code and builds into a shared
library. You typically build it once and reuse it across ECU projects.

```bash
# Clone the repository
git clone https://github.com/rettde/vecu-core.git
cd vecu-core

# Build the Rust workspace (runtime, ABI bridge, HSM)
cargo build --release

# Build the BaseLayer
cd baselayer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Result:
#   Linux:   libbase.so
#   macOS:   libbase.dylib
#   Windows: base.dll
```

All 24 BSW modules (EcuM, SchM, Os, Det, Com, PduR, CanIf, LinIf, EthIf,
FrIf, Cry, CryIf, Csm, NvM, Fee, MemIf, Dem, Dcm, FiM, WdgM, CanTp, DoIP)
are always compiled. There are no optional CMake flags.

---

## 5. Building the ECU Application Library

Your ECU C‑code is compiled into a separate shared library. It is
**not** linked against the BaseLayer at build time — symbols are resolved
at runtime when the loader loads both libraries into the same process.

### CMakeLists.txt (Template)

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_ecu_appl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Path to vecu‑core checkout
set(VECU_CORE_DIR "${CMAKE_SOURCE_DIR}/../vecu-core"
    CACHE PATH "Path to vecu-core repository")

# Include BaseLayer headers (AUTOSAR BSW API) + ABI headers
include_directories(
    ${VECU_CORE_DIR}/baselayer/include
    ${VECU_CORE_DIR}/crates/vecu-abi/include
    ${CMAKE_SOURCE_DIR}/include
)

# Your ECU source files
add_library(appl_ecu SHARED
    src/Appl_Entry.c
    src/SwcBodyCtrl.c
    src/SwcDiag.c
    # Add all your SWC source files here
)

# SiL build flag — enables host‑compatible stubs
target_compile_definitions(appl_ecu PRIVATE VECU_SIL)

# Platform‑specific settings
if(WIN32)
    target_compile_definitions(appl_ecu PRIVATE BUILDING_DLL)
else()
    target_compile_options(appl_ecu PRIVATE -Wall -Wextra -fPIC -fvisibility=default)
endif()
```

### Build Commands

```bash
cd my_ecu_project
mkdir build && cd build

# Point to the vecu-core checkout
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DVECU_CORE_DIR=/path/to/vecu-core

cmake --build .

# Result:
#   Linux:   libappl_ecu.so
#   macOS:   libappl_ecu.dylib
#   Windows: appl_ecu.dll
```

---

## 6. Configuration Files

### 6.1 Loader Configuration (`config.yaml`)

This is the main configuration file for the vECU loader:

```yaml
# config.yaml — vECU Loader Configuration

simulation:
  tick_count: 10000        # 10 seconds of simulation
  tick_interval_us: 1000   # 1 ms tick

shm:
  size: 65536              # 64 KiB for NvM / queues

appl:
  bridge: "target/release/libvecu_appl.dylib"
  base_layer: "baselayer/build/libbase.dylib"
  ecu_code: "my_ecu_project/build/libappl_ecu.dylib"

# Optional: SIL Kit co‑simulation
# silkit:
#   registry_uri: "silkit://localhost:8500"
#   participant_name: "MyECU"
#   can_network: "CAN1"
#   can_controller: "CAN1_Ctrl"
```

### 6.2 BaseLayer Configuration (C structs in `Base_Entry.c`)

The BaseLayer uses **hardcoded C configuration structs** rather than
external JSON files. The reference `Base_Entry.c` ships with sensible
defaults. To customise for your ECU project, **copy `Base_Entry.c` into
your project** and modify the configuration arrays.

#### Signal / PDU Configuration

Signals and PDUs are defined as `Com_SignalConfigType` and
`Com_PduConfigType` arrays:

```c
/* Signals */
static const Com_SignalConfigType g_signals[] = {
    { .signal_id = 0, .pdu_id = 0x100, .bit_position = 0,
      .bit_length = 16, .endianness = COM_LITTLE_ENDIAN,
      .direction = COM_DIRECTION_RX, .init_value = 0 },
    { .signal_id = 1, .pdu_id = 0x200, .bit_position = 0,
      .bit_length = 8, .endianness = COM_LITTLE_ENDIAN,
      .direction = COM_DIRECTION_TX, .init_value = 0 },
};

/* PDUs */
static const Com_PduConfigType g_pdus[] = {
    { .pdu_id = 0x100, .frame_id = 0x600, .dlc = 8,
      .direction = COM_DIRECTION_RX, .bus_type = VECU_BUS_CAN },
    { .pdu_id = 0x200, .frame_id = 0x700, .dlc = 8,
      .direction = COM_DIRECTION_TX, .bus_type = VECU_BUS_CAN },
};

static const Com_ConfigType g_com_config = {
    .signals = g_signals, .num_signals = 2,
    .pdus    = g_pdus,    .num_pdus    = 2,
};
```

#### NvM Block Configuration

NvM blocks map to offsets in the SHM vars region:

```c
static const NvM_BlockDescriptorType g_nvm_blocks[] = {
    { .blockId = 0, .length = 17, .shmOffset = 0  },  /* VIN */
    { .blockId = 1, .length = 4,  .shmOffset = 32 },  /* Calibration */
};

static const NvM_ConfigType g_nvm_config = {
    .blocks = g_nvm_blocks, .numBlocks = 2,
};
```

#### Dcm DID / Routine Configuration

DIDs are configured with read/write callback functions:

```c
/* DID 0xF190 = VIN (17 bytes), backed by NvM block 0 */
static Std_ReturnType did_f190_read(uint8* data, uint16* length) {
    *length = 17;
    return NvM_ReadBlock(0, data);
}
static Std_ReturnType did_f190_write(const uint8* data, uint16 length) {
    (void)length;
    return NvM_WriteBlock(0, data);
}

static const Dcm_DidEntryType g_dids[] = {
    { .did = 0xF190u, .length = 17,
      .readFn = did_f190_read, .writeFn = did_f190_write },
};

static const Dcm_ConfigType g_dcm_config = {
    .dids = g_dids, .numDids = 1,
    .routines = NULL, .numRoutines = 0,
    .sessionTimeoutMs = 5000,
};
```

#### CanTp Channel Configuration

```c
static const CanTp_ChannelConfigType g_cantp_channels[] = {
    { .rxId = 0x641, .txId = 0x642, .fcId = 0x641,
      .blockSize = 0, .stMin = 0 },
};

static const CanTp_ConfigType g_cantp_config = {
    .channels = g_cantp_channels, .numChannels = 1,
};
```

**Tip:** See `baselayer/src/Base_Entry.c` for the complete reference
configuration and `examples/sample_ecu/` for a working example project.

---

## 7. Running the vECU

### Build the vECU Runtime

```bash
cd vecu-core
cargo build --release
```

### Run Standalone

```bash
# Run 10 seconds of simulation (10,000 ticks at 1 ms)
vecu-loader \
  --config my_ecu_project/config.yaml \
  --mode standalone
```

### Run with SIL Kit

```bash
# Start SIL Kit registry first
sil-kit-registry --listen-uri silkit://localhost:8500

# Run vECU as SIL Kit participant
vecu-loader \
  --config my_ecu_project/config.yaml \
  --mode distributed
```

### Expected Output (approximate)

```
[INFO] ABI version 1 — APPL capabilities: FRAME_IO | DIAGNOSTICS
[INFO] HSM capabilities: HSM_SEED_KEY | SIGN_VERIFY | HSM_ENCRYPT | HSM_RNG
[INFO] SHM region: 65536 bytes
[INFO] Running 10000 ticks (interval 1000 µs) …
[INFO] Simulation finished: 10000 ticks
```

The BaseLayer itself does not log per‑tick output. If a `Det_ReportError`
is triggered, you will see the error forwarded through the log callback.

---

## 8. Diagnostic Testing (UDS)

### Sending UDS Requests via CAN

In SIL Kit mode, you can use CANoe, CANalyzer, or any UDS tester.
The vECU responds to UDS requests on the configured diagnostic CAN IDs.

### Typical Diagnostic CAN IDs

| ID | Direction | Content |
|----|-----------|---------|
| `0x641` | RX (tester → ECU) | UDS request |
| `0x642` | TX (ECU → tester) | UDS response |

### Example: Read VIN (DID 0xF190)

**Request:**
```
CAN ID 0x641: [02 22 F1 90 00 00 00 00]
```

**Response:**
```
CAN ID 0x642: [10 14 62 F1 90 30 30 30]  (First Frame)
CAN ID 0x642: [21 30 30 30 30 30 30 30]  (Consecutive Frame 1)
CAN ID 0x642: [22 30 30 30 30 30 30 00]  (Consecutive Frame 2)
```

### Example: SecurityAccess (SID 0x27)

```
Step 1: Request Seed (on CAN ID 0x641)
  TX: [02 27 01 00 00 00 00 00]
  RX: [12 67 01 <16 bytes seed>]   (multi‑frame via CanTp on 0x642)

Step 2: Send Key (on CAN ID 0x641)
  TX: [12 27 02 <16 bytes CMAC>]   (multi‑frame via CanTp)
  RX: [02 67 02 00 00 00 00 00]    (positive response on 0x642 = unlocked)
```

The key is computed as `CMAC(master_key, seed)` — the HSM handles this
automatically when `Csm_MacGenerate()` is called.

---

## 9. SecurityAccess with HSM

The vECU HSM provides real AES‑128 cryptography (not stubs).
SecurityAccess works end‑to‑end:

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Tester   │     │   Dcm    │     │   Csm    │     │   HSM    │
│  (CANoe)  │     │ BaseLayer│     │ BaseLayer│     │ vecu-hsm │
└────┬─────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
     │ 27 01          │                │                │
     │───────────────>│                │                │
     │                │ RequestSeed    │                │
     │                │───────────────>│                │
     │                │                │ hsm_seed()     │
     │                │                │───────────────>│
     │                │                │   16B seed     │
     │                │                │<───────────────│
     │                │  seed          │                │
     │                │<───────────────│                │
     │ 67 01 + seed   │                │                │
     │<───────────────│                │                │
     │                │                │                │
     │ 27 02 + key    │                │                │
     │───────────────>│                │                │
     │                │ ValidateKey    │                │
     │                │───────────────>│                │
     │                │                │ hsm_key()      │
     │                │                │───────────────>│
     │                │                │   OK / FAIL    │
     │                │                │<───────────────│
     │                │  result        │                │
     │                │<───────────────│                │
     │ 67 02 (OK)     │                │                │
     │<───────────────│                │                │
```

### Key Computation (for test tools)

If you need to compute the correct key in your test tool:

```python
# Python example (for CANoe CAPL or test script)
import hmac
from cryptography.hazmat.primitives.cmac import CMAC
from cryptography.hazmat.primitives.ciphers import algorithms

# Master key (slot 0, default)
master_key = bytes(range(16))  # [0x00, 0x01, ..., 0x0F]

# Seed received from ECU
seed = bytes.fromhex("aabbccdd...")  # 16 bytes from response

# Compute key = AES-128-CMAC(master_key, seed)
c = CMAC(algorithms.AES128(master_key))
c.update(seed)
key = c.finalize()

# Send key in SecurityAccess request
```

---

## 10. Troubleshooting

### Library Loading Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `cannot open shared object file` | Library not found | Check `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH` (macOS) |
| `symbol not found: Base_Init` | BaseLayer missing export | Ensure `Base_Init`, `Base_Step`, `Base_Shutdown` are not `static` |
| `symbol not found: Appl_Init` | Application missing export | Ensure `Appl_Init`, `Appl_MainFunction`, `Appl_Shutdown` are exported |
| `ABI version mismatch` | Incompatible vecu‑abi version | Rebuild BaseLayer and application against current headers |

### Signal I/O Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Com_ReceiveSignal` returns 0 | No frames arriving | Check `Com_PduConfigType.frame_id` matches bus traffic |
| Signal value wrong | Endianness mismatch | Verify `endianness` field in `Com_SignalConfigType` matches DBC |
| TX frame not sent | PDU direction wrong | Set `direction = COM_DIRECTION_TX` in PDU config |

### Diagnostic Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| NRC 0x7F (serviceNotSupported) | SID not in Dcm dispatch | All 9 SIDs are built‑in; check request format |
| NRC 0x33 (securityAccessDenied) | Wrong session | Switch to Extended session first (0x10 03) |
| NRC 0x35 (invalidKey) | Key computation wrong | Verify CMAC computation matches HSM master key |
| No response | Diagnostic CAN IDs wrong | Check CanTp config matches tester CAN IDs |

### Build Issues

| Error | Cause | Fix |
|-------|-------|-----|
| `undefined reference to Com_*` | Symbols not resolved at runtime | Ensure BaseLayer is loaded first; on macOS use `-undefined dynamic_lookup` |
| `implicit declaration of function` | Missing include | Add `#include "Com.h"` (or respective BSW header) |
| `incompatible pointer type` | `Std_Types.h` mismatch | Use BaseLayer's `Std_Types.h`, not a different version |

---

## 11. Differences from Vector VTT

If you are migrating from VTT, here are the key differences:

| Aspect | Vector VTT | vECU (this system) |
|--------|-----------|-------------------|
| **Project setup** | VTT project wizard in CANoe | `CMakeLists.txt` + `config.yaml` |
| **BSW stubs** | VTT‑internal, not editable | Open C source, fully customizable |
| **Compiler** | MSVC only | gcc, clang, MSVC |
| **Platform** | Windows only | Windows, Linux, macOS |
| **HSM** | Stub (no real crypto) | Real AES‑128, CMAC, CSPRNG |
| **Bus integration** | CANoe internal | SIL Kit (open), standalone, or custom |
| **Diagnostics** | VTT Dcm stub | Full Dcm with session management |
| **NvM** | File‑backed | SHM vars block (RAM) + optional file |
| **RTE** | Generated by DaVinci | Hand‑written or generated (your choice) |
| **License** | Proprietary (per seat) | MIT / Apache‑2.0 (free) |
| **CI integration** | Complex (needs CANoe license) | Simple (`cmake` + `cargo test`) |

### Migration Steps from VTT

1. **Extract SWC source files** from your VTT project
2. **Keep the generated RTE headers** (usually compatible as‑is)
3. **Remove VTT‑specific `#include`s** (`VttCntrl.h`, `VttOs.h`, etc.)
4. **Add `#define VECU_SIL`** to replace VTT‑specific `#ifdef VTT_BUILD`
5. **Create `Appl_Entry.c`** with init / main / shutdown (see Step 1)
6. **Edit `Base_Entry.c`** signal/PDU config from your DBC / ARXML
7. **Build and run** (see sections 4‑7)

Typical migration effort: **1–3 days** per ECU project, depending on
complexity and number of hardware‑dependent code sections.

---

## 12. Using the Vector AUTOSAR BSW Instead of Stubs

If you have a licensed Vector AUTOSAR BSW (from a DaVinci Configurator
project / SIP), you can use it **instead of our stub BaseLayer**. This
gives you production‑grade BSW behaviour (real scheduling, full NvM state
machine, etc.) while still running on the host PC via the vECU runtime.

### Concept

```
┌─────────────────────────────────────────────┐
│  vECU Runtime (Rust)                        │
│  ┌───────────────────────────────────────┐  │
│  │  vecu-appl (ABI Bridge)               │  │
│  │  ┌─────────────────────────────────┐  │  │
│  │  │  YOUR ECU C‑CODE (SWCs)        │  │  │
│  │  └─────────────┬───────────────────┘  │  │
│  │                │ calls BSW APIs        │  │
│  │  ┌─────────────▼───────────────────┐  │  │
│  │  │  Vector AUTOSAR BSW            │  │  │  ← replaces libbase.so
│  │  │  (Com, Dcm, NvM, SchM, …)     │  │  │
│  │  │  + MCAL SiL stubs             │  │  │
│  │  │  + Base_Entry.c (adapter)      │  │  │
│  │  └─────────────────────────────────┘  │  │
│  └───────────────────────────────────────┘  │
└─────────────────────────────────────────────┘
```

The key difference: **the Vector BSW replaces `libbase.so`** entirely.
Your SWC code stays the same — it calls the same AUTOSAR APIs either way.

### Prerequisites

- **DaVinci Configurator** project with generated BSW source code
- **Vector SIP** (Software Integration Package) for your µC family
- **MCAL SiL stubs** from Vector (VTT MCAL) or your own stubs
- A compiler that can build the Vector BSW for the host (gcc/clang/MSVC)

### Step 1: Create the Adapter (`Base_Entry.c`)

The vECU runtime expects three exported functions. You write a thin
adapter that maps them to the Vector BSW lifecycle:

```c
/* Base_Entry.c — Adapter for Vector AUTOSAR BSW */
#include "vecu_base_context.h"
#include "EcuM.h"
#include "SchM.h"
#include "BswM.h"

static const vecu_base_context_t* g_ctx = NULL;

const vecu_base_context_t* Base_GetCtx(void) { return g_ctx; }

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Base_Init(const vecu_base_context_t* ctx) {
    g_ctx = ctx;
    /* Vector BSW init sequence (order from EcuM_Init / EcuM_StartupTwo) */
    EcuM_Init();          /* calls SchM_Init, Det_Init, BswM_Init, … */
    /* EcuM_StartupTwo is typically called by the Os — trigger it: */
    EcuM_StartupTwo();
}

EXPORT void Base_Step(uint64_t tick) {
    /* Drive the SchM main‑function schedule.
     * In production, the Os does this via tasks.
     * Here we call one "tick" worth of processing: */
    SchM_MainFunction();
    BswM_MainFunction();
    /* Add any other periodic MainFunctions your config needs */
}

EXPORT void Base_Shutdown(void) {
    EcuM_GoSleep();
    EcuM_GoDown();
    g_ctx = NULL;
}
```

> **Note:** The exact init sequence depends on your DaVinci configuration.
> Check your `EcuM_Callout_Stubs.c` for the correct order.

### Step 2: Provide MCAL SiL Stubs

The Vector BSW calls MCAL drivers (CAN driver, SPI, GPT, etc.) which
don't exist on the host PC. You need stub implementations:

| MCAL Module | What the Stub Does |
|-------------|-------------------|
| `Can` | Routes TX frames via `g_ctx->push_tx_frame()` |
| `CanTrcv` | Returns E_OK (transceiver always on) |
| `Gpt` | Maps to host timer or `Os_GetTick()` |
| `Spi` | No‑op or returns E_OK |
| `Fls` | Maps to SHM vars block (like our NvM) |
| `Port`, `Dio` | No‑op stubs |

Vector provides **VTT MCAL stubs** for exactly this purpose. If you have
a VTT license, use `Vtt_Can.c`, `Vtt_Fls.c`, etc. Otherwise, write
minimal stubs yourself — typically 5–20 lines per module.

**Critical:** The CAN driver stub must bridge to `vecu_base_context_t`:

```c
/* Can_SiL.c — CAN driver stub for host simulation */
#include "vecu_base_context.h"
#include "Can.h"

extern const vecu_base_context_t* Base_GetCtx(void);

Std_ReturnType Can_Write(Can_HwHandleType hth,
                         const Can_PduType* pduInfo)
{
    const vecu_base_context_t* ctx = Base_GetCtx();
    if (ctx == NULL || ctx->push_tx_frame == NULL) return E_NOT_OK;

    vecu_frame_t frame = {0};
    frame.id   = pduInfo->id;
    frame.dlc  = pduInfo->length;
    frame.bus  = VECU_BUS_CAN;
    if (pduInfo->length > 0 && pduInfo->length <= MAX_FRAME_DATA) {
        memcpy(frame.data, pduInfo->sdu, pduInfo->length);
    }
    ctx->push_tx_frame(&frame);
    return E_OK;
}
```

### Step 3: Build as Shared Library

```cmake
cmake_minimum_required(VERSION 3.16)
project(vector_base C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Vector BSW generated sources
file(GLOB VECTOR_BSW_SOURCES
    "${DAVINCI_GEN_DIR}/source/*.c"
)

# MCAL SiL stubs
set(MCAL_STUBS
    stubs/Can_SiL.c
    stubs/Fls_SiL.c
    stubs/Gpt_SiL.c
    # …
)

add_library(base SHARED
    Base_Entry.c          # our adapter
    ${VECTOR_BSW_SOURCES}
    ${MCAL_STUBS}
)

target_include_directories(base PRIVATE
    ${DAVINCI_GEN_DIR}/include
    ${VECTOR_SIP_DIR}/BSW/include
    ${VECU_CORE_DIR}/crates/vecu-abi/include
)

target_compile_options(base PRIVATE -fvisibility=default)
```

### Step 4: Use in config.yaml

```yaml
appl:
  bridge: "vecu-core/target/release/libvecu_appl.dylib"
  base_layer: "vector_base/build/libbase.dylib"   # ← Vector BSW
  ecu_code: "my_ecu/build/libappl_ecu.dylib"
```

The rest of the workflow (SWC code, RTE headers, `config.yaml`,
`vecu‑loader`) stays **exactly the same**.

### When to Use Stubs vs. Vector BSW

| Scenario | Recommendation |
|----------|---------------|
| Early development / prototyping | **Stubs** — faster build, no license needed |
| SWC unit testing | **Stubs** — deterministic, simple |
| CI / automated testing | **Stubs** — no Vector license on build server |
| System‑level integration testing | **Vector BSW** — production‑accurate behaviour |
| Pre‑SiL validation close to target | **Vector BSW** — real state machines |
| Customer demos / acceptance tests | **Vector BSW** — matches target ECU |

> **Tip:** You can maintain both setups in parallel. The SWC code and RTE
> headers are identical — only `libbase.so` is swapped.

---

## 13. Using Eclipse OpenBSW as an Open‑Source BaseLayer

[Eclipse OpenBSW](https://github.com/esrlabs/openbsw) is a professional,
open‑source (Apache‑2.0) software platform for automotive microcontrollers
developed by [ESRLabs](https://www.esrlabs.com/) under the Eclipse
Foundation. It provides real BSW‑level functionality (async runtime,
CAN, diagnostics, timers) and already supports a **POSIX target** that
runs on the host PC.

OpenBSW can serve as a fully open‑source alternative to both our stub
BaseLayer and the proprietary Vector BSW.

### Key Differences from Our Stub BaseLayer

| Aspect | Our Stubs | Eclipse OpenBSW |
|--------|-----------|-----------------|
| **Language** | C11 | C++ (with Rust integration via Embassy) |
| **API surface** | AUTOSAR function names (`Com_*`, `Dcm_*`, …) | Own C++ API (not AUTOSAR‑named) |
| **Complexity** | Minimal stubs (state stored in arrays) | Real state machines, cooperative scheduling |
| **License** | MIT / Apache‑2.0 | Apache‑2.0 |
| **POSIX target** | Built for host‑PC from day one | Supported (`cmake --preset posix`) |
| **Maintenance** | Part of vecu‑core | Eclipse Foundation + ESRLabs |

### Integration Concept

Because OpenBSW uses its own API (not AUTOSAR function names), an
**AUTOSAR shim layer** is needed between your SWC code and OpenBSW:

```
┌───────────────────────────────────────────────────────┐
│  vECU Runtime (Rust)                                  │
│  ┌─────────────────────────────────────────────────┐  │
│  │  vecu-appl (ABI Bridge)                         │  │
│  │  ┌────────────────┐  ┌───────────────────────┐  │  │
│  │  │  ECU C‑Code    │  │  AUTOSAR Shim (C)     │  │  │
│  │  │  (SWCs)        │→ │  Com_*() → OpenBSW    │  │  │
│  │  │                │  │  Dcm_*() → OpenBSW    │  │  │
│  │  │                │  │  NvM_*() → OpenBSW    │  │  │
│  │  └────────────────┘  └──────────┬────────────┘  │  │
│  │                                 ↓                │  │
│  │                      ┌────────────────────────┐  │  │
│  │                      │  Eclipse OpenBSW       │  │  │
│  │                      │  (C++, Apache‑2.0)     │  │  │
│  │                      │  + Base_Entry.cpp       │  │  │
│  │                      └────────────────────────┘  │  │
│  └─────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────┘
```

### Step 1: Clone and Build OpenBSW for POSIX

```bash
git clone https://github.com/esrlabs/openbsw.git
cd openbsw

# Build for host PC (POSIX target)
cmake --preset posix
cmake --build --preset posix
```

### Step 2: Create the Adapter (`Base_Entry.cpp`)

The adapter maps our three lifecycle functions to the OpenBSW runtime:

```cpp
// Base_Entry.cpp — Adapter for Eclipse OpenBSW
extern "C" {
#include "vecu_base_context.h"
}

// OpenBSW headers
#include "async/AsyncBinding.h"
#include "lifecycle/LifecycleManager.h"

static const vecu_base_context_t* g_ctx = nullptr;

extern "C" {

const vecu_base_context_t* Base_GetCtx(void) { return g_ctx; }

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Base_Init(const vecu_base_context_t* ctx) {
    g_ctx = ctx;
    // Initialize OpenBSW lifecycle
    ::lifecycle::LifecycleManager::init();
}

EXPORT void Base_Step(uint64_t tick) {
    (void)tick;
    // Drive OpenBSW cooperative scheduling for one cycle
    ::async::execute();
}

EXPORT void Base_Shutdown(void) {
    ::lifecycle::LifecycleManager::shutdown();
    g_ctx = nullptr;
}

} // extern "C"
```

> **Note:** The exact OpenBSW API calls depend on your configuration.
> Refer to the [OpenBSW documentation](https://eclipse-openbsw.github.io/openbsw)
> for the current lifecycle and async API.

### Step 3: Create the AUTOSAR Shim Layer

The shim provides AUTOSAR‑named C functions that delegate to OpenBSW
internals. Example for `Com_ReceiveSignal`:

```c
// autosar_shim/Com_Shim.c
#include "Com.h"
#include "openbsw_signal_bridge.h"  // your bridge header

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType id, void* value) {
    return openbsw_signal_read(id, value);  // calls into OpenBSW
}

Std_ReturnType Com_SendSignal(Com_SignalIdType id, const void* value) {
    return openbsw_signal_write(id, value);
}
```

You need one shim file per BSW module your SWCs actually call. Typical
set: `Com_Shim.c`, `Dcm_Shim.c`, `NvM_Shim.c`, `Dem_Shim.c`.

### Step 4: Build as Shared Library

```cmake
cmake_minimum_required(VERSION 3.16)
project(openbsw_base CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# OpenBSW sources (built as POSIX target)
add_subdirectory(${OPENBSW_DIR} openbsw_build)

# AUTOSAR shim layer
set(SHIM_SOURCES
    autosar_shim/Com_Shim.c
    autosar_shim/Dcm_Shim.c
    autosar_shim/NvM_Shim.c
)

add_library(base SHARED
    Base_Entry.cpp
    ${SHIM_SOURCES}
)

target_link_libraries(base PRIVATE openbsw::platform)

target_include_directories(base PRIVATE
    ${OPENBSW_DIR}/libs
    ${VECU_CORE_DIR}/crates/vecu-abi/include
    ${VECU_CORE_DIR}/baselayer/include   # for Std_Types.h
    autosar_shim
)

target_compile_options(base PRIVATE -fvisibility=default)
```

### Step 5: Use in config.yaml

```yaml
appl:
  bridge: "vecu-core/target/release/libvecu_appl.dylib"
  base_layer: "openbsw_base/build/libbase.dylib"   # ← OpenBSW
  ecu_code: "my_ecu/build/libappl_ecu.dylib"
```

### When to Use Which BaseLayer

| Scenario | Stubs | Vector BSW | OpenBSW |
|----------|-------|------------|---------|
| Quick prototyping | ✅ Best | — | — |
| Unit tests / CI | ✅ Best | — | ✅ Good |
| Fully open‑source stack | ✅ Good | ❌ No | ✅ Best |
| Production‑accurate BSW | — | ✅ Best | ✅ Good |
| No AUTOSAR license at all | ✅ | ❌ | ✅ |
| Existing Vector project | — | ✅ Best | — |
| Rust + C++ mixed codebase | — | — | ✅ Best |

> **Status:** OpenBSW integration is on the roadmap. The shim layer
> (`vecu‑openbsw‑shim`) is planned as a separate repository. Contributions
> welcome.

---

## Quick Reference

### Required Exports from Your Application Library

```c
/* These three functions MUST be exported (non‑static, extern "C") */
void Appl_Init(void);
void Appl_MainFunction(void);
void Appl_Shutdown(void);
```

### Required Exports from BaseLayer

```c
/* Provided by our reference BaseLayer — do not modify */
void Base_Init(const vecu_base_context_t* ctx);
void Base_Step(uint64_t tick);
void Base_Shutdown(void);
```

### Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `VECU_BASE_LIB` | Path to BaseLayer library | `/path/to/libbase.so` |
| `VECU_APPL_LIB` | Path to application library | `/path/to/libappl_ecu.so` |

### Useful Commands

```bash
# Build everything
cd vecu-core && cargo build --release
cd baselayer/build && cmake --build .
cd my_ecu_project/build && cmake --build .

# Run tests
cargo test --workspace

# Run simulation
vecu-loader --config config.yaml --mode standalone

# Check library exports (Linux)
nm -D libappl_ecu.so | grep -E "Base_|Appl_"

# Check library exports (macOS)
nm -g libappl_ecu.dylib | grep -E "Base_|Appl_"
```
