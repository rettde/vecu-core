# How To: ECU / AUTOSAR Software Integration

This guide explains how to integrate real ECU application C‑code into the
vECU execution system. It targets embedded software engineers who have
AUTOSAR Classic ECU source code and want to run it as a virtual ECU.

**Prerequisites:**

- ECU C/C++ source code (SWC runnables, application logic)
- Generated or hand‑written RTE headers for the target ECU
- Basic knowledge of AUTOSAR BSW module APIs
- C compiler (gcc, clang, or MSVC)
- CMake ≥ 3.16 (≥ 3.28 for OpenBSW target)
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
12. [Using Vector MICROSAR as BaseLayer (Detailed)](#12-using-vector-microsar-as-baselayer-detailed)
13. [Using Eclipse OpenBSW as BaseLayer](#13-using-eclipse-openbsw-as-baselayer)
14. [Level‑3 Architecture: Virtual‑MCAL, vHsm, OS‑Mapping](#14-level-3-architecture-virtual-mcal-vhsm-os-mapping)

---

## 1. Overview

The vECU execution system runs your ECU application code on a host PC
(Windows, Linux, macOS) in a deterministic, tick‑based simulation.
The Level‑3 architecture has four layers:

```
┌──────────────────────────────────────────────────────────┐
│  vECU Runtime (Rust)                                     │
│  vecu-loader, vecu-runtime, vecu-shm, vecu-silkit        │
├──────────────────────────────────────────────────────────┤
│  ABI Bridge (vecu-appl)                                  │
│  ┌────────────────────────────────────────────────────┐  │
│  │  YOUR ECU C‑CODE (SWCs + RTE)                      │  │  ← you provide
│  │  libappl_ecu.so                                    │  │
│  └───────────────────┬────────────────────────────────┘  │
│                      │ calls AUTOSAR BSW APIs             │
│  ┌───────────────────▼────────────────────────────────┐  │
│  │  BaseLayer  ← choose one:                          │  │
│  │    a) Stub BaseLayer      (baselayer/)             │  │  ← we provide
│  │    b) Vector MICROSAR     (target-microsar/)       │  │  ← you provide
│  │    c) Eclipse OpenBSW     (target-openbsw/)        │  │  ← open source
│  └───────────────────┬────────────────────────────────┘  │
│                      │ calls MCAL / Crypto / OS APIs      │
│  ┌───────────────────▼────────────────────────────────┐  │
│  │  Level‑3 Layers (ADR‑002…004)                      │  │
│  │    Virtual‑MCAL    (vmcal/)       ← 9 drivers      │  │
│  │    vHsm Adapter    (vhsm_adapter/) ← crypto        │  │
│  │    OS‑Mapping      (os_mapping/)  ← scheduling     │  │
│  └───────────────────┬────────────────────────────────┘  │
│                      │ routes through vecu_base_context_t │
│  ┌───────────────────▼────────────────────────────────┐  │
│  │  vecu-hsm (AES‑128, CMAC, SecurityAccess, RNG)     │  │
│  └────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

### Three BaseLayer Options

| Option | Language | License | Best For |
|--------|----------|---------|----------|
| **Stub BaseLayer** | C11 | MIT/Apache‑2.0 | Prototyping, CI, unit tests |
| **Vector MICROSAR** | C11 | Proprietary | Production‑accurate BSW, existing DaVinci projects |
| **Eclipse OpenBSW** | C++14 | Apache‑2.0 | Open‑source, modern C++, POSIX host |

All three options share the **same Level‑3 layers** below them:

| Layer | Location | Replaces |
|-------|----------|----------|
| Virtual‑MCAL (9 drivers) | `vmcal/` | Hardware MCAL (Can, Eth, Fr, Dio, Port, Spi, Gpt, Mcu, Fls) |
| vHsm Adapter | `vhsm_adapter/` | Crypto_30_vHsm hardware module |
| OS‑Semantics Mapping | `os_mapping/` | AUTOSAR OS (tasks, alarms, counters, events) |

### What You Provide vs. What We Provide

| You provide | We provide |
|-------------|------------|
| `SwcXxx.c` — your SWC application logic | BaseLayer (stub / MICROSAR shim / OpenBSW shim) |
| `Rte_SwcXxx.h` — RTE headers (generated or hand‑written) | Virtual‑MCAL, vHsm Adapter, OS‑Mapping |
| `Appl_Entry.c` — lifecycle hooks (init / main / shutdown) | `libvecu_appl.so` — Rust ABI bridge |
| `Base_Entry.c` — signal/NvM/Dcm configuration (stub only) | `libvecu_hsm.so` — SHE‑compatible AES‑128 crypto |
| MICROSAR delivery (optional, if using MICROSAR) | `vecu‑loader` — tick‑based runtime + optional SIL Kit |

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

- AUTOSAR BSW source code (we provide stubs, or use MICROSAR/OpenBSW)
- MCAL drivers (replaced by Virtual‑MCAL — `vmcal/`)
- Crypto hardware (replaced by vHsm Adapter — `vhsm_adapter/`)
- OS configuration (replaced by OS‑Mapping — `os_mapping/`)
- Compiler‑specific startup code
- Linker scripts
- Vector tooling (unless using MICROSAR, see Section 12)

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

You have three BaseLayer options. Each produces a shared library
(`libbase.so` / `.dylib` / `.dll`) that the vECU loader loads at runtime.

### Option A: Stub BaseLayer (Default)

Best for prototyping, CI, and unit testing. No external dependencies.

```bash
git clone https://github.com/rettde/vecu-core.git
cd vecu-core

# Build the Rust workspace
cargo build --release

# Build the stub BaseLayer
cmake -S examples/sample_ecu -B build/sample_ecu -DCMAKE_BUILD_TYPE=Release
cmake --build build/sample_ecu
```

All 24 BSW modules are always compiled. No optional flags.

### Option B: Vector MICROSAR

Best for production‑accurate BSW behaviour. Requires a licensed MICROSAR
delivery (see [Section 12](#12-using-vector-microsar-as-baselayer-detailed)).

```bash
cmake -S target-microsar -B build/target-microsar \
    -DCMAKE_BUILD_TYPE=Release \
    -DMICROSAR_ROOT=/path/to/microsar/delivery

cmake --build build/target-microsar
# Result: libvecu_microsar_shim.so — use as base_layer in config.yaml
```

### Option C: Eclipse OpenBSW

Best for fully open‑source stacks. Requires CMake ≥ 3.28, internet access
at configure time (see [Section 13](#13-using-eclipse-openbsw-as-baselayer)).

```bash
cmake -S target-openbsw -B build/target-openbsw \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build/target-openbsw
# Result: libvecu_openbsw_shim.so — use as base_layer in config.yaml
```

### Building the Level‑3 Layers (all options)

The Virtual‑MCAL, vHsm Adapter, and OS‑Mapping are built automatically
when using `target-microsar` or `target-openbsw`. For standalone use:

```bash
cmake -S . -B build/vecu -DVECU_BUILD=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/vecu
```

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

## 12. Using Vector MICROSAR as BaseLayer (Detailed)

This section provides a **complete, step‑by‑step guide** for integrating
a Vector MICROSAR BSW delivery into vecu‑core. It covers the full
workflow from DaVinci Configurator to running SWCs on the host PC.

> **No proprietary code is stored in the vecu‑core repository.**
> MICROSAR sources must be provided via the `MICROSAR_ROOT` build variable.

### 12.1 Architecture

```
┌──────────────────────────────────────────────────────────┐
│  vECU Runtime (Rust)                                     │
│  vecu-loader, vecu-runtime, vecu-shm                     │
├──────────────────────────────────────────────────────────┤
│  ABI Bridge (vecu-appl)                                  │
│                                                          │
│  ┌────────────────────────────────────────────────────┐  │
│  │  ECU Application (libappl_ecu.so)                  │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐           │  │
│  │  │ SwcBody  │ │ SwcDiag  │ │ SwcComm  │  ← SWCs   │  │
│  │  │ Ctrl     │ │          │ │          │           │  │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘           │  │
│  │       │ Rte_Read    │ Rte_Call    │ Rte_Send        │  │
│  │  ┌────▼─────────────▼────────────▼─────────────┐   │  │
│  │  │  RTE (Generated by DaVinci RTE Generator)   │   │  │
│  │  │  Rte_SwcBodyCtrl.h, Rte_SwcDiag.h, …       │   │  │
│  │  └────┬────────────────────────────────────────┘   │  │
│  └───────│────────────────────────────────────────────┘  │
│          │ Com_ReceiveSignal, Dcm_*, NvM_*, Csm_*        │
│  ┌───────▼────────────────────────────────────────────┐  │
│  │  MICROSAR BSW (libvecu_microsar_shim.so)           │  │
│  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    │  │
│  │  │ EcuM │ │ Com  │ │ Dcm  │ │ NvM  │ │ Csm  │    │  │
│  │  │ BswM │ │ PduR │ │ Dem  │ │ Fee  │ │ CryIf│    │  │
│  │  │ SchM │ │ CanIf│ │ CanTp│ │ MemIf│ │      │    │  │
│  │  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘    │  │
│  │     │ MCAL   │ MCAL   │        │ MCAL   │ Crypto  │  │
│  └─────│────────│────────│────────│────────│─────────┘  │
│        ▼        ▼        ▼        ▼        ▼            │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Level‑3 Layers (Virtual‑MCAL + vHsm + OS‑Map)  │   │
│  │  Can.c  Eth.c  Fr.c   Fls.c  Crypto_30_vHsm.c  │   │
│  │  Dio.c  Port.c Spi.c  Gpt.c  Mcu.c             │   │
│  └──────────────────────┬───────────────────────────┘   │
│                         │ vecu_base_context_t            │
│  ┌──────────────────────▼───────────────────────────┐   │
│  │  vecu-hsm (AES‑128, CMAC, RNG, SecurityAccess)  │   │
│  └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

### 12.2 Prerequisites

| Requirement | Details |
|-------------|---------|
| **DaVinci Configurator** | Version ≥ 5.x, project with valid ECUC |
| **MICROSAR SIP** | Software Integration Package for your µC family |
| **Generated BSW code** | Run "Generate" in DaVinci — produces `BSW/` and `GenData/` |
| **DaVinci RTE Generator** | For generating `Rte_*.h` and `Rte_*.c` from SWC descriptions |
| **Compiler** | gcc, clang, or MSVC (host‑PC, not cross‑compiler) |
| **CMake** | ≥ 3.15 |
| **vecu‑core checkout** | `git clone https://github.com/rettde/vecu-core.git` |

### 12.3 DaVinci Configurator Workflow

#### Step 1: Create or Open DaVinci Project

Open your existing DaVinci Configurator project (or create a new one
for a SiL target). The project must have a valid ECUC configuration.

#### Step 2: Configure BSW Modules for SiL

Adjust the following settings for host‑PC execution:

| Module | Setting | Value for SiL |
|--------|---------|---------------|
| **EcuM** | `EcuMGeneral/EcuMMainFunctionPeriod` | Match your tick interval (e.g. 1 ms) |
| **Os** | not needed | Replaced by OS‑Mapping (`os_mapping/`) |
| **Can** | `CanGeneral/CanHardwareObjectCount` | Keep as‑is (Virtual‑MCAL handles all HTH/HRH) |
| **NvM** | `NvMCommon/NvMDevicePath` | irrelevant (SHM‑backed via Virtual‑MCAL Fls) |
| **Csm** | `CsmJobs` | Keep as‑is (vHsm Adapter handles all jobs) |
| **Dcm** | `DcmDsd/DcmDsdServiceTable` | Keep all DIDs/routines |
| **Dem** | DTC configuration | Keep as‑is |

#### Step 3: Generate BSW Code

In DaVinci Configurator:
1. **Validate** — resolve all errors
2. **Generate** — produces `BSW/` and `GenData/` directories
3. The output should look like:

```
MICROSAR_ROOT/
├── BSW/
│   ├── EcuM/
│   │   ├── EcuM.c
│   │   ├── EcuM.h
│   │   ├── EcuM_Cbk.h
│   │   └── EcuM_Callout_Stubs.c
│   ├── BswM/
│   ├── SchM/
│   ├── Com/
│   ├── PduR/
│   ├── CanIf/
│   ├── CanTp/
│   ├── Dcm/
│   ├── Dem/
│   ├── NvM/
│   ├── Fee/
│   ├── MemIf/
│   ├── Csm/
│   ├── CryIf/
│   ├── Det/
│   ├── FiM/
│   └── WdgM/
├── GenData/
│   ├── Com_Cfg.c
│   ├── Com_Cfg.h
│   ├── Dcm_Cfg.c
│   ├── PduR_Cfg.c
│   ├── CanIf_Cfg.c
│   ├── NvM_Cfg.c
│   ├── EcuM_Cfg.c
│   ├── BswM_Cfg.c
│   ├── SchM_*.h           ← generated scheduler headers
│   └── ...
├── Include/                ← common MICROSAR headers
│   ├── Std_Types.h
│   ├── ComStack_Types.h
│   ├── MemMap.h
│   └── Compiler.h
└── MCAL/                   ← REPLACED by Virtual‑MCAL
```

#### Step 4: Generate RTE Code

In DaVinci RTE Generator (or embedded in DaVinci Developer):
1. Import your **SWC descriptions** (`.arxml` files)
2. Map SWC ports to BSW module services
3. Generate — produces `Rte_*.h` and `Rte_*.c` files

### 12.4 SWC Integration (Detailed)

#### Typical SWC Structure

An AUTOSAR SWC has **Runnables** (callable functions) connected to the
RTE via **Ports** (sender/receiver for signals, client/server for services).

```
┌─────────────────────────────────────────────────────┐
│  SwcBodyCtrl                                         │
│                                                      │
│  Ports:                                              │
│    [R] RpVehicleSpeed    ← Rte_Read (from Com)       │
│    [R] RpDoorStatus      ← Rte_Read (from Com)       │
│    [P] PpLightCommand    ← Rte_Write (to Com)        │
│    [CS] CsCsmEncrypt     ← Rte_Call (to Csm)         │
│    [CS] CsNvmRead        ← Rte_Call (to NvM)         │
│                                                      │
│  Runnables:                                          │
│    SwcBodyCtrl_Init()             ← called once       │
│    SwcBodyCtrl_MainFunction()     ← cyclic (10 ms)    │
│    SwcBodyCtrl_OnDoorEvent()      ← event‑triggered   │
└─────────────────────────────────────────────────────┘
```

#### SWC Source File

```c
/* SwcBodyCtrl.c — Body Controller SWC */
#include "Rte_SwcBodyCtrl.h"

static uint8 g_lightState = 0;

void SwcBodyCtrl_Init(void)
{
    g_lightState = 0;
}

void SwcBodyCtrl_MainFunction(void)
{
    uint16 vehicleSpeed = 0;
    uint8  doorStatus   = 0;

    /* Read signals via RTE (→ Com → CanIf → Virtual‑MCAL Can) */
    (void)Rte_Read_RpVehicleSpeed_VehicleSpeed(&vehicleSpeed);
    (void)Rte_Read_RpDoorStatus_DoorStatus(&doorStatus);

    /* Application logic */
    if (vehicleSpeed > 10 && doorStatus != 0) {
        g_lightState = 1; /* Warning light ON */
    } else {
        g_lightState = 0;
    }

    /* Write signal via RTE (→ Com → CanIf → Virtual‑MCAL Can → SIL Kit) */
    (void)Rte_Write_PpLightCommand_LightCommand(&g_lightState);
}

void SwcBodyCtrl_OnDoorEvent(void)
{
    uint8 doorStatus = 0;
    (void)Rte_Read_RpDoorStatus_DoorStatus(&doorStatus);
    /* … event handling … */
}
```

#### Generated RTE Header (from DaVinci)

```c
/* Rte_SwcBodyCtrl.h — Generated by DaVinci RTE Generator */
#ifndef RTE_SWCBODYCTRL_H
#define RTE_SWCBODYCTRL_H

#include "Rte_Type.h"
#include "Rte_DataHandleType.h"

/* Signal Read (Sender/Receiver) */
#define Rte_Read_RpVehicleSpeed_VehicleSpeed(data) \
    (Rte_Read_SwcBodyCtrl_RpVehicleSpeed_VehicleSpeed(data))
extern Std_ReturnType Rte_Read_SwcBodyCtrl_RpVehicleSpeed_VehicleSpeed(
    uint16* data);

#define Rte_Read_RpDoorStatus_DoorStatus(data) \
    (Rte_Read_SwcBodyCtrl_RpDoorStatus_DoorStatus(data))
extern Std_ReturnType Rte_Read_SwcBodyCtrl_RpDoorStatus_DoorStatus(
    uint8* data);

/* Signal Write */
#define Rte_Write_PpLightCommand_LightCommand(data) \
    (Rte_Write_SwcBodyCtrl_PpLightCommand_LightCommand(data))
extern Std_ReturnType Rte_Write_SwcBodyCtrl_PpLightCommand_LightCommand(
    const uint8* data);

/* Client/Server: Crypto */
#define Rte_Call_CsCsmEncrypt_Encrypt(jobId, mode, in, inLen, out, outLen) \
    (Csm_Encrypt((jobId), (mode), (in), (inLen), (out), (outLen)))

/* Client/Server: NvM */
#define Rte_Call_CsNvmRead_ReadBlock(blockId, dst) \
    (NvM_ReadBlock((blockId), (dst)))

#endif /* RTE_SWCBODYCTRL_H */
```

> **Tip:** If you don't have DaVinci RTE Generator, you can write these
> headers by hand. The macros simply map `Rte_Read_*` / `Rte_Write_*` /
> `Rte_Call_*` to the underlying BSW API (`Com_*`, `Csm_*`, `NvM_*`).

#### Diagnostic SWC Example

```c
/* SwcDiag.c — Diagnostic SWC with SecurityAccess */
#include "Rte_SwcDiag.h"

void SwcDiag_Init(void) { /* nothing */ }

void SwcDiag_MainFunction(void)
{
    /* Dcm processes requests internally via CanTp → PduR → Dcm.
     * The SWC only needs to provide DID read/write callbacks
     * and routine control callbacks, registered via Dcm_Cfg.c. */
}

/* DID 0xF190 — VIN Read (called by Dcm) */
Std_ReturnType SwcDiag_ReadVin(uint8* data, uint16* length)
{
    *length = 17;
    return Rte_Call_CsNvmRead_ReadBlock(0, data);
}

/* DID 0xF190 — VIN Write (called by Dcm) */
Std_ReturnType SwcDiag_WriteVin(const uint8* data, uint16 length)
{
    (void)length;
    return Rte_Call_CsNvmWrite_WriteBlock(0, data);
}
```

#### Communication SWC Example

```c
/* SwcComm.c — Communication Manager SWC */
#include "Rte_SwcComm.h"

static uint8 g_commMode = 0; /* 0=OFF, 1=SILENT, 2=FULL */

void SwcComm_Init(void)
{
    g_commMode = 2; /* FULL communication */
}

void SwcComm_MainFunction(void)
{
    /* Monitor communication state, handle bus‑off recovery, etc. */
    /* In vECU: Virtual‑MCAL Can driver handles bus state internally */
}
```

### 12.5 Application Entry Point with MICROSAR

```c
/* Appl_Entry.c — Lifecycle hooks for MICROSAR‑based vECU */
#include "Rte_SwcBodyCtrl.h"
#include "Rte_SwcDiag.h"
#include "Rte_SwcComm.h"

void Appl_Init(void)
{
    SwcBodyCtrl_Init();
    SwcDiag_Init();
    SwcComm_Init();
}

void Appl_MainFunction(void)
{
    static uint32 tick_counter = 0;
    tick_counter++;

    /* 10 ms runnables (every tick if tick = 10 ms) */
    SwcBodyCtrl_MainFunction();
    SwcComm_MainFunction();

    /* 100 ms runnables */
    if (tick_counter % 10 == 0) {
        SwcDiag_MainFunction();
    }
}

void Appl_Shutdown(void)
{
    /* Cleanup — NvM write‑all is triggered by EcuM_GoSleep */
}
```

### 12.6 Build with target‑microsar

The `target-microsar/` directory is ready to use. Point it to your delivery:

```bash
cd vecu-core

cmake -S target-microsar -B build/target-microsar \
    -DCMAKE_BUILD_TYPE=Release \
    -DMICROSAR_ROOT=/path/to/your/microsar/delivery

cmake --build build/target-microsar
```

This produces `libvecu_microsar_shim.so` which:
- Initializes MICROSAR BSW (EcuM → SchM → BswM → Com → Dcm → …)
- Calls `MCALBridge_Init()` to set up Virtual‑MCAL
- Each tick: polls RX, drives all BSW MainFunctions
- Shutdown: Com_DeInit → BswM_Deinit → EcuM_GoSleep

### 12.7 What Gets Replaced (MCAL Bypass)

The key insight: **MICROSAR's MCAL directory is entirely skipped**.
Our Virtual‑MCAL provides API‑compatible replacements:

| MICROSAR MCAL Module | Virtual‑MCAL Replacement | Backing |
|---------------------|-------------------------|---------|
| `Can` driver (HW) | `vmcal/src/Can.c` | `ctx->push_tx_frame` / `pop_rx_frame` |
| `Eth` driver (HW) | `vmcal/src/Eth.c` | `ctx->push_tx_frame` / `pop_rx_frame` |
| `Fr` driver (HW) | `vmcal/src/Fr.c` | `ctx->push_tx_frame` / `pop_rx_frame` |
| `Fls` driver (HW) | `vmcal/src/Fls.c` | `ctx->shm_vars` (RAM‑backed) |
| `Gpt` driver (HW) | `vmcal/src/Gpt.c` | Tick‑based timers |
| `Mcu` driver (HW) | `vmcal/src/Mcu.c` | Init stubs |
| `Dio` driver (HW) | `vmcal/src/Dio.c` | RAM‑backed channels |
| `Port` driver (HW) | `vmcal/src/Port.c` | Init‑semantics only |
| `Spi` driver (HW) | `vmcal/src/Spi.c` | Loopback / no‑op |
| `Crypto_30_vHsm` | `vhsm_adapter/src/Crypto_30_vHsm.c` | `ctx->hsm_*` callbacks |
| AUTOSAR OS | `os_mapping/src/Os_Mapping.c` | Deterministic tick dispatch |

### 12.8 Data Flow Examples

#### CAN Signal Reception (End‑to‑End)

```
SIL Kit / Standalone
    │
    ▼
vecu-runtime (Rust)  →  ctx->pop_rx_frame()
    │
    ▼
Virtual‑MCAL Can.c   →  Can_MainFunction_Read()
    │                     stores frame in RX buffer
    ▼
MICROSAR CanIf       →  CanIf_RxIndication()
    │
    ▼
MICROSAR PduR        →  PduR_CanIfRxIndication()
    │
    ▼
MICROSAR Com         →  Com_RxIndication() → signal buffer update
    │
    ▼
RTE                  →  Rte_Read_RpVehicleSpeed_VehicleSpeed()
    │
    ▼
SWC                  →  SwcBodyCtrl_MainFunction() reads value
```

#### UDS Diagnostic Request (End‑to‑End)

```
Tester (CANoe / SIL Kit)
    │  CAN ID 0x641: [02 22 F1 90 ...]
    ▼
Virtual‑MCAL Can.c   →  pop_rx_frame()
    │
    ▼
MICROSAR CanIf       →  CanIf_RxIndication()
    │
    ▼
MICROSAR CanTp       →  CanTp_RxIndication() → reassembly
    │
    ▼
MICROSAR PduR        →  PduR_CanTpRxIndication()
    │
    ▼
MICROSAR Dcm         →  Dcm_MainFunction() → DID 0xF190 dispatch
    │
    ▼
SwcDiag              →  SwcDiag_ReadVin() → NvM_ReadBlock()
    │
    ▼
MICROSAR NvM         →  NvM_MainFunction() → Fee → MemIf
    │
    ▼
Virtual‑MCAL Fls.c   →  reads from ctx->shm_vars
    │
    ▼  (response flows back through Dcm → CanTp → Can → SIL Kit)
```

#### Crypto Operation (End‑to‑End)

```
SWC calls Rte_Call_CsCsmEncrypt_Encrypt(...)
    │
    ▼
MICROSAR Csm         →  Csm_Encrypt() → job dispatch
    │
    ▼
MICROSAR CryIf       →  CryIf_ProcessJob()
    │
    ▼
vHsm Adapter         →  Crypto_30_vHsm_ProcessJob()
    │                     delegates to ctx->hsm_encrypt()
    ▼
vecu-hsm (Rust)      →  AES‑128 encrypt (real crypto)
    │
    ▼  (result returned through callback chain)
```

### 12.9 Adapting to Your Specific Delivery

Every MICROSAR delivery is project‑specific. Common adjustments:

| Task | File | What to Change |
|------|------|---------------|
| BSW init order | `vecu_microsar_shim.c` | Match your `EcuM_Callout_Stubs.c` sequence |
| Add BSW modules | `vecu_microsar_shim.c` | Add init/MainFunction calls for LinIf, FrIf, DoIP, … |
| Extra include paths | `CMakeLists.txt` | Add paths to your delivery's subdirectories |
| Compiler defines | `CMakeLists.txt` | Add `MICROSAR_*` defines from your DaVinci project |
| GenData sources | `CMakeLists.txt` | Adjust `GLOB` pattern for your delivery structure |
| MemMap.h | Your delivery | Provide empty `#define`s (no memory sections on host) |
| Compiler.h | Your delivery | Map to host compiler (gcc/clang/MSVC) |
| SchM_*.h | GenData | Ensure `SchM_Enter_*` / `SchM_Exit_*` are empty macros |

### 12.10 When to Use MICROSAR vs. Stubs

| Scenario | Recommendation |
|----------|---------------|
| Early SWC development | **Stubs** — faster build, no license needed |
| SWC unit testing in CI | **Stubs** — no Vector license on build server |
| System‑level integration | **MICROSAR** — production‑accurate BSW behaviour |
| Pre‑SiL validation | **MICROSAR** — real state machines, real Dcm/Com |
| Customer acceptance | **MICROSAR** — matches target ECU exactly |
| NvM persistence testing | **MICROSAR** — full NvM/Fee state machine |
| Full diagnostic stack | **MICROSAR** — real Dcm with all services |

> **Tip:** Maintain both setups in parallel. The SWC code and RTE headers
> are identical — only the BaseLayer library is swapped in `config.yaml`.

---

## 13. Using Eclipse OpenBSW as BaseLayer

[Eclipse OpenBSW](https://github.com/esrlabs/openbsw) is a professional,
open‑source (Apache‑2.0) software platform for automotive microcontrollers
developed by [ESRLabs](https://www.esrlabs.com/) under the Eclipse
Foundation. It provides real BSW‑level functionality (async runtime,
CAN, diagnostics, timers) and supports a **POSIX target** for host‑PC
execution.

The `target-openbsw/` directory provides a ready‑to‑use build that
fetches OpenBSW via CMake `FetchContent` — **no manual clone needed**.

### 13.1 Key Differences

| Aspect | Stub BaseLayer | Eclipse OpenBSW | MICROSAR |
|--------|---------------|-----------------|----------|
| **Language** | C11 | C++14 | C11 |
| **API surface** | AUTOSAR names | Own C++ API | AUTOSAR names |
| **Complexity** | Minimal stubs | Real state machines | Production BSW |
| **License** | MIT/Apache‑2.0 | Apache‑2.0 | Proprietary |
| **Build** | Simple CMake | FetchContent | External delivery |

### 13.2 Architecture

```
vecu-core Runtime (Rust)
        │
        ▼
┌──────────────────────────────────┐
│  vecu_openbsw_shim               │
│  Base_Init / Base_Step / Shutdown│
├──────────────────────────────────┤
│  Eclipse OpenBSW (POSIX target)  │
│  lifecycle, async, uds, docan,   │
│  doip, cpp2can, cpp2ethernet,    │
│  storage, timer, logger          │
├──────────────────────────────────┤
│  vecu_openbsw_transport          │
│  CAN/ETH → ctx->push_tx_frame   │
│  ctx->pop_rx_frame → CAN/ETH    │
└──────────────────────────────────┘
```

### 13.3 Build

```bash
cd vecu-core

cmake -S target-openbsw -B build/target-openbsw \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build/target-openbsw
```

CMake `FetchContent` pulls OpenBSW at configure time into `_deps/`.
Requires CMake ≥ 3.28 and internet access at first configure.

Result: `libvecu_openbsw_shim.so` (or `.dylib` / `.dll`)

### 13.4 How It Works

The shim layer (`target-openbsw/src/vecu_openbsw_shim.cpp`) bridges
vecu‑core lifecycle calls to OpenBSW's internal lifecycle and async
framework:

| vecu‑core Call | OpenBSW Action |
|---------------|----------------|
| `Base_Init(ctx)` | Store ctx, init lifecycle manager, start async runtime |
| `Base_Step(tick)` | Execute one async cycle, poll RX frames, drive timers |
| `Base_Shutdown()` | Shutdown lifecycle, stop async runtime |

The transport adapter (`target-openbsw/src/vecu_openbsw_transport.cpp`)
routes CAN and Ethernet frames:

| Direction | Flow |
|-----------|------|
| **TX** | OpenBSW cpp2can → `OpenBsw_TransmitCan()` → `ctx->push_tx_frame()` |
| **RX** | `ctx->pop_rx_frame()` → `OpenBsw_PollRxFrames()` → OpenBSW cpp2can |
| **ETH TX** | OpenBSW cpp2ethernet → `OpenBsw_TransmitEth()` → `ctx->push_tx_frame()` |
| **ETH RX** | `ctx->pop_rx_frame()` → `OpenBsw_PollRxFrames()` → OpenBSW cpp2ethernet |

### 13.5 OpenBSW Module Mapping

OpenBSW uses its own C++ API. If your SWCs use AUTOSAR function names
(`Com_ReceiveSignal`, `Dcm_*`, etc.), you need an AUTOSAR shim layer:

| AUTOSAR API | OpenBSW Equivalent |
|-------------|-------------------|
| `Com_ReceiveSignal()` | `::com::SignalManager::read()` |
| `Com_SendSignal()` | `::com::SignalManager::write()` |
| `Dcm_*` (diagnostics) | `::uds::UdsServer` |
| `NvM_ReadBlock()` | `::storage::StorageManager::read()` |
| `CanTp_*` | `::docan::DoCanTransport` |
| `DoIP_*` | `::doip::DoIpServer` |

### 13.6 Use in config.yaml

```yaml
appl:
  bridge: "vecu-core/target/release/libvecu_appl.dylib"
  base_layer: "build/target-openbsw/libvecu_openbsw_shim.dylib"
  ecu_code: "my_ecu/build/libappl_ecu.dylib"
```

### 13.7 When to Use Which BaseLayer

| Scenario | Stubs | MICROSAR | OpenBSW |
|----------|-------|----------|---------|
| Quick prototyping | ✅ Best | — | ✅ Good |
| Unit tests / CI | ✅ Best | — | ✅ Good |
| Fully open‑source | ✅ Good | ❌ No | ✅ Best |
| Production‑accurate BSW | — | ✅ Best | ✅ Good |
| No AUTOSAR license | ✅ | ❌ | ✅ |
| Existing Vector project | — | ✅ Best | — |
| Modern C++ codebase | — | — | ✅ Best |

---

## 14. Level‑3 Architecture: Virtual‑MCAL, vHsm, OS‑Mapping

All three BaseLayer options share the same **Level‑3 layers** below
them (per ADR‑002, ADR‑003, ADR‑004). These layers abstract away hardware
and OS dependencies so the BSW stack can run on a host PC.

### 14.1 Virtual‑MCAL (`vmcal/`)

9 AUTOSAR MCAL‑compatible drivers that route I/O through
`vecu_base_context_t` instead of real hardware registers.

| Module | Source | What It Does |
|--------|--------|-------------|
| **Can** | `vmcal/src/Can.c` | CAN TX/RX via `push_tx_frame` / `pop_rx_frame` |
| **Eth** | `vmcal/src/Eth.c` | Ethernet TX/RX via `push_tx_frame` / `pop_rx_frame` |
| **Fr** | `vmcal/src/Fr.c` | FlexRay TX/RX via `push_tx_frame` / `pop_rx_frame` |
| **Dio** | `vmcal/src/Dio.c` | RAM‑backed digital I/O channels (64 channels) |
| **Port** | `vmcal/src/Port.c` | Init‑semantics only (no real pin mux) |
| **Spi** | `vmcal/src/Spi.c` | Loopback (TX→RX) or no‑op |
| **Gpt** | `vmcal/src/Gpt.c` | Tick‑based timers with callback support |
| **Mcu** | `vmcal/src/Mcu.c` | Init stubs (PLL, clock = no‑op on host) |
| **Fls** | `vmcal/src/Fls.c` | Flash emulation via `ctx->shm_vars` (RAM) |

**Usage:** The Virtual‑MCAL is initialized via `VMcal_SetCtx(ctx)` which
stores the `vecu_base_context_t*` pointer. All modules then route through
this context's function pointers.

### 14.2 vHsm Adapter (`vhsm_adapter/`)

A `Crypto_30_vHsm`‑compatible API that delegates all crypto operations
to `vecu_base_context_t` HSM callbacks → `vecu-hsm` (real AES‑128).

| Function | Delegates To | Operation |
|----------|-------------|-----------|
| `Crypto_30_vHsm_ProcessJob()` | `ctx->hsm_encrypt` | AES‑128 ECB/CBC encrypt |
| | `ctx->hsm_decrypt` | AES‑128 ECB/CBC decrypt |
| | `ctx->hsm_generate_mac` | AES‑128 CMAC generation |
| | `ctx->hsm_verify_mac` | AES‑128 CMAC verification |
| | `ctx->hsm_rng` | CSPRNG random bytes |

The adapter supports AUTOSAR Crypto job types (`CRYPTO_ENCRYPT`,
`CRYPTO_DECRYPT`, `CRYPTO_MACGENERATE`, `CRYPTO_MACVERIFY`,
`CRYPTO_RANDOMGENERATE`) and routes them through the standard
MICROSAR crypto stack: `Csm → CryIf → Crypto_30_vHsm → vecu-hsm`.

### 14.3 OS‑Semantics Mapping (`os_mapping/`)

Replaces the AUTOSAR OS with deterministic, tick‑based scheduling.

| Feature | Implementation |
|---------|---------------|
| **Cyclic Tasks** | Dispatched by period (ms) at each tick |
| **Event Tasks** | Triggered by `Os_Mapping_SetEvent()` |
| **Counters** | Incremented each tick, trigger alarms at expiry |
| **Alarms** | Cyclic or one‑shot, invoke callbacks |
| **Resources** | `GetResource` / `ReleaseResource` (no real locking needed) |
| **Lifecycle** | `STARTUP → RUN → SHUTDOWN` phases |

**Key point:** All scheduling is **deterministic and sequential**.
There are no threads, no preemption, no race conditions. This is ideal
for reproducible test results.

### 14.4 Build

The Level‑3 layers are built with the `-DVECU_BUILD=ON` flag:

```bash
cmake -S . -B build/vecu -DVECU_BUILD=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/vecu
```

When using `target-microsar` or `target-openbsw`, the layers are
linked automatically — no separate build step needed.

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
/* Provided by all three BaseLayer options */
void Base_Init(const vecu_base_context_t* ctx);
void Base_Step(uint64_t tick);
void Base_Shutdown(void);
```

### BaseLayer Library Names

| Option | Library | Build Directory |
|--------|---------|----------------|
| Stub | `libbase.so` | `build/sample_ecu/` |
| MICROSAR | `libvecu_microsar_shim.so` | `build/target-microsar/` |
| OpenBSW | `libvecu_openbsw_shim.so` | `build/target-openbsw/` |

### Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `VECU_BASE_LIB` | Path to BaseLayer library | `/path/to/libbase.so` |
| `VECU_APPL_LIB` | Path to application library | `/path/to/libappl_ecu.so` |

### Useful Commands

```bash
# Build Rust workspace
cargo build --release

# Build stub BaseLayer + sample ECU
cmake -S examples/sample_ecu -B build/sample_ecu && cmake --build build/sample_ecu

# Build MICROSAR target
cmake -S target-microsar -B build/target-microsar \
    -DMICROSAR_ROOT=/path/to/delivery && cmake --build build/target-microsar

# Build OpenBSW target
cmake -S target-openbsw -B build/target-openbsw && cmake --build build/target-openbsw

# Build Level‑3 layers standalone
cmake -S . -B build/vecu -DVECU_BUILD=ON && cmake --build build/vecu

# Run tests
cargo test --workspace

# Run simulation
vecu-loader --config config.yaml --mode standalone

# Check library exports (Linux)
nm -D libappl_ecu.so | grep -E "Base_|Appl_"

# Check library exports (macOS)
nm -g libappl_ecu.dylib | grep -E "Base_|Appl_"
```
