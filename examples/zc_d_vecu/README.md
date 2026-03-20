# ZC_D_C0 vECU Integration

Virtualizes the **ZC_D_C0_Appl** Zone Controller ECU using the `vecu-core` stack.

## Architecture

```
┌──────────────────────────────────────────────────┐
│  ZC_D_C0 Application Code                        │
│  (Code/App/*, Code/Int/*)                        │
├──────────────────────────────────────────────────┤
│  Vector MICROSAR BSW (SIP_Appl/Components/)      │
│  + DaVinci GenData (GenData/ZC_D_C0_Appl_Vtt/)   │
├────────────┬─────────────┬───────────────────────┤
│ vmcal      │ vhsm_adapter│ os_mapping            │
│ (Can,Eth,  │ (Crypto_30_ │ (deterministic        │
│  Fr,Dio,   │  vHsm stub) │  tick scheduler)      │
│  Port,Spi, │             │                       │
│  Gpt,Mcu,  │             │                       │
│  Fls,Adc,  │             │                       │
│  Pwm,Wdg,  │             │                       │
│  Lin,Icu)  │             │                       │
├────────────┴─────────────┴───────────────────────┤
│  vecu_base_context_t  (Rust ↔ C bridge)          │
├──────────────────────────────────────────────────┤
│  vecu-loader  →  vecu-runtime  →  vecu-hsm       │
│  (plugin host)   (SIL Kit I/O)   (AES/CMAC/RNG)  │
└──────────────────────────────────────────────────┘
```

## Replacement Strategy

| Real Target          | vECU Replacement     | Notes                              |
|----------------------|----------------------|------------------------------------|
| Mcal_Rh850X2x       | vmcal/               | 15 MCAL modules (incl. Icu)        |
| Crypto_30_vHsm (IPC)| vhsm_adapter/        | Routes to vecu-hsm Rust plugin     |
| MICROSAR OS (5-core) | os_mapping/          | Single-core deterministic dispatch  |
| VTT/CANoe runtime    | vecu-loader          | Native host execution              |
| GenData (real)       | GenData (VTT variant)| Same DaVinci output, VTT flavor    |

## Prerequisites

1. **Full workspace** via Google `repo`:
   ```bash
   cd examples/zc_d_workspace
   repo init -u <manifests-url> -m zc_d_c0_appl.xml
   repo sync
   ```

2. **GenData** from a Windows DaVinci/Bazel build:
   ```bash
   # On Windows, after running GENCODE target:
   tar czf gendata_vtt.tar.gz ZC_D_C0_Appl/GenData/ZC_D_C0_Appl_Vtt/
   # Copy to macOS and extract into workspace
   tar xzf gendata_vtt.tar.gz -C examples/zc_d_workspace/
   ```

## Build

```bash
cd examples/zc_d_vecu
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## Header Include Priority

```
1. GenData         — real VTT configuration from DaVinci Configurator
2. vecu-platform   — clean-room AUTOSAR host overlay (Platform_Types, Os, Compiler, …)
3. platform/       — project-specific stubs (VttCntrl*.h, *_Cfg.h, *_MemMap.h)
4. SIP _Common     — standard AUTOSAR types from MICROSAR SIP
5. vecu-core       — vmcal, vhsm_adapter, os_mapping, ABI
6. SIP BSW + App   — auto-discovered component Implementation/ directories
```

Core AUTOSAR host headers are provided by `vecu-platform/include/` (18 headers):
`Platform_Types.h`, `Compiler.h`, `Std_Types.h`, `Os.h`, `MemMap.h`,
`Crypto_GeneralTypes.h`, `vecu_prefix.h`, etc.

The `platform/` directory contains **project-specific** stubs only:

- **~120 `*_MemMap.h`** — no-op section pragmas for MICROSAR modules
- **~70 project stubs** — `VttCntrl*.h`, `*_30_Vtt*.h`, `*_Cfg.h`, `CANoeApi.h`, etc.

Force-include `vecu_prefix.h` from vecu-platform ensures `Platform_Types.h`
and `Compiler.h` are loaded before any BSW translation unit.

## Status

- [x] Icu MCAL stub added to vmcal (15th module)
- [x] vecu-platform integration (core AUTOSAR headers consolidated)
- [x] CMakeLists.txt with vecu-platform + project platform/ include chain
- [x] BSW source list (bsw_sources.cmake — 274 files)
- [x] GenData auto-discovery with exclude-pattern filtering
- [x] vecu_microsar_bridge.c (Base_Init/Step/Shutdown → EcuM lifecycle)
- [x] Successful build: libzc_d_c0_vecu.dylib on macOS/Clang
- [ ] End-to-end test with vecu-loader
