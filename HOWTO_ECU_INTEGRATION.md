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

**You provide:** ECU C‑code + RTE headers + configuration files.
**We provide:** BaseLayer (BSW stubs), HSM, runtime, bus integration.

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
Csm_Encrypt(jobId, CRYPTO_OPERATIONMODE_SINGLECALL, data, len, result, &resultLen);
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

/* Signal IDs — must match com_config.json */
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

/* Call crypto service */
static inline Std_ReturnType Rte_Call_Csm_Encrypt(
    uint32 jobId, const uint8* data, uint32 len,
    uint8* result, uint32* resultLen)
{
    return Csm_Encrypt(jobId, CRYPTO_OPERATIONMODE_SINGLECALL,
                       data, len, result, resultLen);
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

### BaseLayer Build Options

| CMake Option | Default | Description |
|-------------|---------|-------------|
| `VECU_WITH_CANTP` | ON | Include ISO 15765‑2 transport layer |
| `VECU_WITH_DOIP` | OFF | Include ISO 13400 transport layer |
| `VECU_WITH_WDGM` | ON | Include watchdog manager |
| `VECU_JSON_PARSER` | `builtin` | JSON parser for config (`builtin` or `cjson`) |

---

## 5. Building the ECU Application Library

Your ECU C‑code is compiled into a separate shared library that links
against the BaseLayer.

### CMakeLists.txt (Template)

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_ecu_appl C)

# Find the BaseLayer
set(VECU_BASELAYER_DIR "${CMAKE_SOURCE_DIR}/../vecu-core/baselayer"
    CACHE PATH "Path to BaseLayer source")

# Include BaseLayer headers (AUTOSAR BSW API)
include_directories(
    ${VECU_BASELAYER_DIR}/include
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

# Link against BaseLayer
target_link_libraries(appl_ecu PRIVATE base)

# Platform‑specific settings
if(UNIX)
    target_compile_options(appl_ecu PRIVATE -Wall -Wextra -fPIC)
endif()
```

### Build Commands

```bash
cd my_ecu_project
mkdir build && cd build

# Point to the BaseLayer build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DVECU_BASELAYER_DIR=/path/to/vecu-core/baselayer

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

appl:
  # Rust ABI bridge (built by cargo)
  bridge: "target/release/libvecu_appl.dylib"
  # AUTOSAR BaseLayer
  base_layer: "baselayer/build/libbase.dylib"
  # Your ECU application code
  ecu_code: "my_ecu_project/build/libappl_ecu.dylib"
  # BaseLayer configuration
  base_config: "my_ecu_project/base_config.json"

hsm:
  path: "target/release/libvecu_hsm.dylib"

shm:
  queue_capacity: 64
  vars_size: 65536    # 64 KiB for NvM

simulation:
  tick_interval_us: 1000   # 1 ms tick
  tick_count: 10000        # 10 seconds of simulation

# Optional: SIL Kit co‑simulation
# silkit:
#   registry: "silkit://localhost:8500"
#   participant: "MyECU"
#   can_network: "CAN1"
#   can_controller: "CAN1_Ctrl"
```

### 6.2 Signal Database (`com_config.json`)

Defines all CAN/LIN/ETH signals and PDUs:

```json
{
  "signals": [
    {
      "name": "VehicleSpeed",
      "id": 0,
      "pdu_id": 0,
      "bit_position": 0,
      "bit_length": 16,
      "endianness": "little",
      "factor": 0.01,
      "offset": 0,
      "init_value": 0,
      "unit": "km/h"
    },
    {
      "name": "DoorStatus",
      "id": 1,
      "pdu_id": 0,
      "bit_position": 16,
      "bit_length": 8,
      "endianness": "little",
      "factor": 1.0,
      "offset": 0,
      "init_value": 0,
      "unit": ""
    },
    {
      "name": "LightCommand",
      "id": 2,
      "pdu_id": 1,
      "bit_position": 0,
      "bit_length": 8,
      "endianness": "little",
      "factor": 1.0,
      "offset": 0,
      "init_value": 0,
      "unit": ""
    }
  ],
  "pdus": [
    {
      "id": 0,
      "name": "PDU_VehicleData",
      "can_id": 256,
      "dlc": 8,
      "direction": "rx",
      "bus_type": "can",
      "cycle_time_ms": 10
    },
    {
      "id": 1,
      "name": "PDU_LightCtrl",
      "can_id": 512,
      "dlc": 8,
      "direction": "tx",
      "bus_type": "can",
      "cycle_time_ms": 100
    }
  ]
}
```

### 6.3 Diagnostic Configuration (`dcm_config.json`)

Defines supported UDS services and DIDs:

```json
{
  "sessions": [
    { "id": 1, "name": "Default", "timeout_ms": 5000 },
    { "id": 3, "name": "Extended", "timeout_ms": 5000 },
    { "id": 2, "name": "Programming", "timeout_ms": 5000, "security_level": 1 }
  ],
  "security_levels": [
    {
      "level": 1,
      "seed_size": 16,
      "key_algorithm": "cmac_aes128",
      "key_slot": 0
    }
  ],
  "dids": [
    {
      "id": "0xF190",
      "name": "VIN",
      "size": 17,
      "access": "read",
      "session": ["Default", "Extended"],
      "nvm_block": 1
    },
    {
      "id": "0xF186",
      "name": "ActiveDiagnosticSession",
      "size": 1,
      "access": "read",
      "session": ["Default", "Extended"],
      "source": "dcm_internal"
    },
    {
      "id": "0x0100",
      "name": "CalibrationValue1",
      "size": 4,
      "access": "readwrite",
      "session": ["Extended"],
      "security_level": 1,
      "nvm_block": 2
    }
  ],
  "routines": [
    {
      "id": "0xFF00",
      "name": "ResetLearnValues",
      "session": ["Extended"],
      "security_level": 1,
      "callback": "SwcDiag_RoutineResetLearnValues"
    }
  ]
}
```

### 6.4 NvM Configuration (`nvm_config.json`)

Defines persistent data blocks stored in the SHM variable block:

```json
{
  "blocks": [
    {
      "id": 1,
      "name": "VIN",
      "offset": 0,
      "size": 17,
      "default_value": "00000000000000000"
    },
    {
      "id": 2,
      "name": "CalibrationValue1",
      "offset": 32,
      "size": 4,
      "default_value": [0, 0, 0, 0]
    },
    {
      "id": 3,
      "name": "DtcStorage",
      "offset": 64,
      "size": 1024,
      "default_value": null
    }
  ],
  "total_size": 4096,
  "checksum": true
}
```

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

### Expected Output

```
[INFO] Loading APPL bridge: target/release/libvecu_appl.dylib
[INFO] Loading BaseLayer: baselayer/build/libbase.dylib
[INFO] Loading ECU code: my_ecu_project/build/libappl_ecu.dylib
[INFO] Loading HSM: target/release/libvecu_hsm.dylib
[INFO] ABI version: 1.1 (compatible)
[INFO] APPL capabilities: FRAME_IO | DIAGNOSTICS
[INFO] HSM capabilities: HSM_SEED_KEY | SIGN_VERIFY | HSM_ENCRYPT | HSM_RNG
[INFO] SHM allocated: 265728 bytes
[INFO] EcuM: STARTUP → RUN
[INFO] Simulation started: tick_interval=1000 µs
[INFO] tick 0: Com_MainFunction, Dcm_MainFunction, NvM_MainFunction
[INFO] tick 1: SwcBodyCtrl_10ms, SwcDiag (idle)
...
[INFO] tick 9999: simulation complete
[INFO] EcuM: RUN → SHUTDOWN
[INFO] NvM: WriteAll complete (3 blocks)
[INFO] Simulation finished: 10000 ticks in 1.23s
```

---

## 8. Diagnostic Testing (UDS)

### Sending UDS Requests via CAN

In SIL Kit mode, you can use CANoe, CANalyzer, or any UDS tester.
The vECU responds to UDS requests on the configured diagnostic CAN IDs.

### Typical Diagnostic CAN IDs

| ID | Direction | Content |
|----|-----------|---------|
| `0x600` | RX (tester → ECU) | UDS request |
| `0x601` | TX (ECU → tester) | UDS response |

### Example: Read VIN (DID 0xF190)

**Request:**
```
CAN ID 0x600: [02 22 F1 90 00 00 00 00]
```

**Response:**
```
CAN ID 0x601: [10 14 62 F1 90 30 30 30]  (First Frame)
CAN ID 0x601: [21 30 30 30 30 30 30 30]  (Consecutive Frame 1)
CAN ID 0x601: [22 30 30 30 30 30 30 00]  (Consecutive Frame 2)
```

### Example: SecurityAccess (SID 0x27)

```
Step 1: Request Seed
  TX: [02 27 01 00 00 00 00 00]
  RX: [12 67 01 <16 bytes seed>]   (multi‑frame via CanTp)

Step 2: Send Key
  TX: [12 27 02 <16 bytes CMAC>]   (multi‑frame via CanTp)
  RX: [02 67 02 00 00 00 00 00]    (positive response = unlocked)
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
| `Com_ReceiveSignal` returns 0 | No frames arriving | Check `com_config.json` CAN ID matches bus traffic |
| Signal value wrong | Endianness mismatch | Verify `endianness` in `com_config.json` matches DBC |
| TX frame not sent | PDU direction wrong | Set `"direction": "tx"` in PDU config |

### Diagnostic Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| NRC 0x7F (serviceNotSupported) | SID not configured | Add SID to `dcm_config.json` |
| NRC 0x33 (securityAccessDenied) | Wrong session | Switch to Extended session first (0x10 03) |
| NRC 0x35 (invalidKey) | Key computation wrong | Verify CMAC computation matches HSM master key |
| No response | Diagnostic CAN IDs wrong | Check CanTp config matches tester CAN IDs |

### Build Issues

| Error | Cause | Fix |
|-------|-------|-----|
| `undefined reference to Com_*` | Not linking BaseLayer | Add `target_link_libraries(appl_ecu PRIVATE base)` |
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
6. **Write `com_config.json`** from your DBC / ARXML signal database
7. **Build and run** (see sections 4–7)

Typical migration effort: **1–3 days** per ECU project, depending on
complexity and number of hardware‑dependent code sections.

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
| `VECU_BASE_CONFIG` | Path to BaseLayer config | `/path/to/base_config.json` |

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
