# target-microsar — Vector MICROSAR Integration for vecu-core

This directory provides a build target that integrates Vector MICROSAR
as the productive AUTOSAR BSW stack for vecu-core (ADR-001).

> **No proprietary code is stored in this repository.**
> MICROSAR sources must be provided externally via `MICROSAR_ROOT`.

## Architecture

```
vecu-core Runtime (Rust)
        |
        v
+-------------------------------+
|  vecu_microsar_shim           |
|  Base_Init / Step / Shutdown  |
+-------------------------------+
        |               |
        v               v
+---------------+  +----------------+
| MICROSAR BSW  |  | MCAL Bridge    |
| EcuM, BswM,   |  | Virtual-MCAL   |
| SchM, Com,     |  | Can,Eth,Fr,    |
| Dcm, Dem,      |  | Dio,Gpt,Mcu,  |
| NvM, CanTp,    |  | Fls,Port,Spi   |
| Csm, PduR      |  +----------------+
+---------------+          |
                           v
                  +----------------+
                  | ctx->push_tx   |
                  | ctx->pop_rx    |
                  | ctx->hsm_*     |
                  | ctx->shm_vars  |
                  +----------------+
```

## How It Works

- **MCAL Bypass** — MICROSAR's CanIf/EthIf call our Virtual-MCAL
  (Can.c, Eth.c, Fr.c) instead of real hardware MCAL drivers.
  All I/O routes through `vecu_base_context_t` callbacks.
- **Crypto Bypass** — Csm → CryIf → Crypto_30_vHsm is handled by
  our vHsm adapter which delegates to `ctx->hsm_*` callbacks.
- **NvM Backing** — Flash operations go through Virtual-MCAL Fls,
  backed by `ctx->shm_vars` (shared memory).
- **Deterministic Scheduling** — SchM MainFunctions are called
  sequentially in `Base_Step`, no real OS needed.

## Prerequisites

1. **Vector MICROSAR delivery** — from DaVinci Configurator
2. Delivery must contain at minimum:
   - `BSW/` — BSW module sources (EcuM, BswM, Com, Dcm, …)
   - `GenData/` — Generated configuration files
   - `Include/` — Common MICROSAR headers (optional)

## Build

```bash
cmake -S target-microsar -B build/target-microsar \
    -DCMAKE_BUILD_TYPE=Release \
    -DMICROSAR_ROOT=/path/to/microsar/delivery

cmake --build build/target-microsar
```

The resulting `libvecu_microsar_shim.so` (or `.dylib` / `.dll`) is
loaded by vecu-loader as the BaseLayer plugin instead of `libbase.so`.

## Adapting to Your Delivery

Every MICROSAR delivery is project-specific. You may need to:

| Task | Where |
|------|-------|
| Add/remove BSW module init calls | `vecu_microsar_shim.c` |
| Adjust include paths | `CMakeLists.txt` → `MICROSAR_INCLUDE_DIRS` |
| Add GenData sources | `CMakeLists.txt` → `MICROSAR_GENDATA_SOURCES` |
| Add project-specific MCAL stubs | `vecu_microsar_mcal_bridge.c` |
| Configure compiler defines | `CMakeLists.txt` → `target_compile_definitions` |

## Typical MICROSAR Delivery Structure

```
MICROSAR_ROOT/
├── BSW/
│   ├── EcuM/           ← ECU State Manager
│   ├── BswM/           ← BSW Mode Manager
│   ├── SchM/           ← Schedule Manager
│   ├── Com/            ← AUTOSAR COM
│   ├── PduR/           ← PDU Router
│   ├── CanIf/          ← CAN Interface
│   ├── CanTp/          ← CAN Transport Protocol
│   ├── Dcm/            ← Diagnostic Communication Manager
│   ├── Dem/            ← Diagnostic Event Manager
│   ├── NvM/            ← NVRAM Manager
│   ├── Fee/            ← Flash EEPROM Emulation
│   ├── Csm/            ← Crypto Service Manager
│   ├── CryIf/          ← Crypto Interface
│   └── ...
├── GenData/            ← DaVinci-generated configuration
│   ├── Com_Cfg.c
│   ├── Dcm_Cfg.c
│   ├── PduR_Cfg.c
│   └── ...
├── Include/            ← Common headers
└── MCAL/               ← MCAL drivers (REPLACED by Virtual-MCAL)
```

## What Gets Replaced

| Original MICROSAR | Replaced By | Source |
|-------------------|-------------|--------|
| MCAL Can driver | `vmcal/src/Can.c` | Virtual-MCAL |
| MCAL Eth driver | `vmcal/src/Eth.c` | Virtual-MCAL |
| MCAL Fr driver | `vmcal/src/Fr.c` | Virtual-MCAL |
| MCAL Dio driver | `vmcal/src/Dio.c` | Virtual-MCAL |
| MCAL Gpt driver | `vmcal/src/Gpt.c` | Virtual-MCAL |
| MCAL Mcu driver | `vmcal/src/Mcu.c` | Virtual-MCAL |
| MCAL Fls driver | `vmcal/src/Fls.c` | Virtual-MCAL |
| MCAL Port driver | `vmcal/src/Port.c` | Virtual-MCAL |
| MCAL Spi driver | `vmcal/src/Spi.c` | Virtual-MCAL |
| Crypto_30_vHsm | `vhsm_adapter/src/Crypto_30_vHsm.c` | vHsm Adapter |
| Os (real RTOS) | `os_mapping/src/Os_Mapping.c` | OS Mapping |
