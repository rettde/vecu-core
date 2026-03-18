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
│  │   │   ├── OpenBSW BSW modules (series code)  │
│  │   │   ├── openbsw_shim.c  (lifecycle bridge) │
│  │   │   ├── Virtual-MCAL    (link-time subst.) │
│  │   │   ├── vHsm Adapter    (Crypto_30_vHsm)   │
│  │   │   └── OS-Mapping      (tick dispatch)     │
│  │   └── libappl_ecu.so (application unchanged)  │
│  └── vecu-hsm  (SHE + SHA-256 crypto)           │
└──────────────────────────────────────────────────┘
```

## Integration Strategy (ADR-005 Build Profile)

The only difference between the series ECU build and the vECU build is
**module substitution at link time**:

| Layer | Series ECU | vECU (this PoC) |
|-------|-----------|-----------------|
| Application + RTE | series code | **same** (unchanged) |
| BSW (Com, PduR, Dcm, ...) | OpenBSW | **same** (unchanged) |
| MCAL (Can, Eth, Fr, ...) | HW-MCAL (e.g. Rh850) | **Virtual-MCAL** |
| Crypto (Crypto_30_vHsm) | HW-HSM | **vHsm Adapter** |
| OS | AUTOSAR OS | **OS-Mapping** |

The build profile switch is a single CMake option: `-DVECU_BUILD=ON`.

## Directory Layout

```
examples/openbsw_poc/
├── CMakeLists.txt           # Build scaffold
├── README.md                # This file
├── include/
│   └── openbsw_shim.h      # Lifecycle bridge header
└── src/
    ├── openbsw_shim.c       # Base_Init/Step/Shutdown → OpenBSW lifecycle
    └── openbsw_stubs.c      # Placeholder stubs (replace with real OpenBSW)
```

## How to Use

### Step 1: Obtain OpenBSW Sources

Clone Eclipse OpenBSW into a local directory:

```bash
git clone https://github.com/Eclipse-SDV-Hackathon-Accenture/2025_OpenBSW.git \
    /path/to/openbsw
```

### Step 2: Configure the Build

```bash
cd examples/openbsw_poc
cmake -B build -S . \
    -DVECU_BUILD=ON \
    -DOPENBSW_ROOT=/path/to/openbsw
```

When `OPENBSW_ROOT` is set, the real OpenBSW sources are compiled.
When it is **not** set, the included `openbsw_stubs.c` is used as a
self-contained placeholder for CI/testing.

### Step 3: Build

```bash
cmake --build build
```

This produces `libopenbsw_base.so` (or `.dylib` / `.dll`) containing:
- OpenBSW BSW modules (or stubs)
- Virtual-MCAL
- vHsm Adapter
- OS-Mapping
- Lifecycle shim (`openbsw_shim.c`)

### Step 4: Run with vecu-loader

```bash
cd /path/to/vecu-core
cargo run -- --config examples/openbsw_poc/config.yaml
```

## Integration Shim (`openbsw_shim.c`)

The shim provides the three mandatory `Base_*` entry points that `vecu-appl`
expects (see `vecu_base_context.h`):

| Function | Purpose |
|----------|---------|
| `Base_Init(ctx)` | Stores ctx, calls `EcuM_Init()` from OpenBSW |
| `Base_Step(tick)` | Calls `EcuM_MainFunction()` / `SchM_MainFunction()` |
| `Base_Shutdown()` | Calls `EcuM_GoSleep()` / `EcuM_GoOff()` |

The shim also stores the `vecu_base_context_t` pointer so that Virtual-MCAL
modules can access the frame I/O and crypto callbacks.

## MCAL Header Compatibility

OpenBSW expects standard AUTOSAR MCAL headers (`Can.h`, `Eth.h`, `Fr.h`, etc.).
Our Virtual-MCAL headers in `vmcal/include/` are designed to be API-compatible
drop-in replacements. The CMake configuration adds `vmcal/include` to the
include path **instead of** the series MCAL include directory.

## What This PoC Validates

1. **Build path**: OpenBSW BSW code compiles against Virtual-MCAL headers
2. **ABI boundary**: `vecu_base_context_t` is the sole interface between
   Rust runtime and C BaseLayer
3. **Link-time substitution**: MCAL, HSM, and OS are swapped without
   modifying any BSW source code
4. **Lifecycle compatibility**: `Base_Init` / `Base_Step` / `Base_Shutdown`
   map cleanly to OpenBSW's `EcuM` lifecycle

## Limitations

- This PoC uses stubs when `OPENBSW_ROOT` is not set
- Full OpenBSW integration requires matching the BSW configuration
  (ARXML-derived) to the Virtual-MCAL capabilities
- See ADR-002 Section 4 for MCAL configuration synchronization

## License

MIT OR Apache-2.0
