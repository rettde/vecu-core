# vecu-platform — AUTOSAR Host Overlay Headers

Clean-room AUTOSAR-compatible headers for compiling series BSW code on the
host (macOS/Linux/Windows).  These headers replace target-specific and
VTT-specific originals, enabling Level 3 vECU builds without proprietary
tooling.

Licensed under **MIT OR Apache-2.0**.  No vendor-derived content.

## Headers

| Header | Purpose |
|--------|---------|
| `Platform_Types.h` | `stdint.h`-based AUTOSAR types, `CPU_TYPE_32` for vECU ABI |
| `Compiler.h` | GCC/Clang/MSVC compiler abstraction (`FUNC`, `P2VAR`, `INLINE`, …) |
| `Compiler_Cfg.h` | Memory-class qualifiers → nothing on host |
| `Std_Types.h` | `Std_ReturnType`, `E_OK`/`E_NOT_OK`, `boolean`, `NULL_PTR` |
| `ComStack_Types.h` | `PduInfoType`, `PduIdType`, `BufReq_ReturnType`, … |
| `Can_GeneralTypes.h` | CAN types (`Can_HwType`, `Can_IdType`, controller states) |
| `Eth_GeneralTypes.h` | Ethernet types (`Eth_ModeType`, filter actions, …) |
| `Fr_GeneralTypes.h` | FlexRay types (POC states, channels, slot modes) |
| `Lin_GeneralTypes.h` | LIN types (frame PIDs, status, checksum models) |
| `MemIf_Types.h` | Memory interface types (`MemIf_StatusType`, `MemIf_ModeType`) |
| `Crypto_GeneralTypes.h` | Full AUTOSAR crypto types matching MICROSAR SIP 34.7.3 layout |
| `CanTrcv_GeneralTypes.h` | CAN transceiver types (modes, wakeup reasons) |
| `MemMap.h` | Universal no-op (no include guard — intentional per AUTOSAR) |
| `Os.h` | OSEK/AUTOSAR OS stubs (tasks, ISRs, events, spinlocks, multicore) |
| `Os_Compiler_Cfg.h` | Empty OS compiler config stub |
| `Os_Hal_Cfg.h` | Empty OS HAL config stub |
| `VX1000.h` | Empty VX1000 measurement stub |
| `vecu_prefix.h` | Force-include header: loads Platform_Types + Compiler first, common fixups |

## Include Priority

When building a project vECU, the include search order must be:

```
1. SIP headers        (highest — real AUTOSAR types from MICROSAR/EB/ETAS)
2. GenData headers    (project-specific generated configuration)
3. vecu-platform      (host overlay — fills gaps not covered by SIP)
4. vmcal/include      (Virtual-MCAL API)
5. vhsm_adapter/include
6. baselayer/include  (stub BSW — lowest)
```

This ensures SIP-provided types win over the clean-room versions.  Headers
use `#if !defined(...)` guards so they don't redefine anything already
provided by the SIP.

## CMake Integration

`vecu-platform` is an INTERFACE (header-only) library.  Link to it:

```cmake
add_subdirectory(vecu-platform)
target_link_libraries(your_target PUBLIC vecu_platform)
```

This adds the include path and defines `VECU_BUILD=1`.

## MemMap Stub Generation

Many MICROSAR modules include module-specific `*_MemMap.h` files.  Use the
included script to auto-generate empty stubs:

```bash
python3 tools/generate_memmap_stubs.py \
    --scan-dirs /path/to/SIP /path/to/GenData \
    --output-dir build/memmap_stubs
```

Then add the output directory to your include path (after vecu-platform).

## Force-Include (`vecu_prefix.h`)

Add `-include vecu_prefix.h` (GCC/Clang) or `/FI vecu_prefix.h` (MSVC)
to ensure `Platform_Types.h` and `Compiler.h` are loaded before any BSW
translation unit.  This prevents the SIP's target-specific versions from
being picked up first within `.c` files that `#include` them directly.

## Project Integration (ZC_D Example)

For a real MICROSAR project (e.g., `examples/zc_d_vecu/`), the CMakeLists.txt
wires vecu-platform as priority 2 (after GenData, before project stubs):

```cmake
set(VECU_PLATFORM_INC "${VECU_ROOT}/vecu-platform/include")
set(PROJECT_PLATFORM_INC "${CMAKE_CURRENT_SOURCE_DIR}/platform")

set(VECU_INCLUDE_DIRS
    ${GENDATA_DIR}                      # 1. GenData
    ${VECU_PLATFORM_INC}                # 2. vecu-platform
    ${PROJECT_PLATFORM_INC}             # 3. Project-specific stubs
    ${BSW_DIR}/_Common/Implementation   # 4. SIP _Common
    ${VMCAL_INC} ${VHSM_INC} ...       # 5. vecu-core layers
)

target_compile_options(my_vecu PRIVATE
    "SHELL:-include ${VECU_PLATFORM_INC}/vecu_prefix.h"
)
```

The project's `platform/` directory only needs project-specific stubs
(`VttCntrl*.h`, `*_Cfg.h`, `*_MemMap.h`), not core AUTOSAR headers.

## Crypto Types

`Crypto_GeneralTypes.h` matches the MICROSAR SIP 34.7.3 layout:

- Types are `uint8` with `#define` constants (not C enums)
- `CRYPTO_ALGOFAM_AES = 0x14u` (AUTOSAR standard value)
- `Crypto_JobType` uses nested `Crypto_JobPrimitiveInputOutputType`
- No dependency on `Csm_Generated_Types.h` (GenData)

The `vhsm_adapter` includes this header and dispatches on
`Crypto_ServiceInfoType` (`CRYPTO_ENCRYPT`, `CRYPTO_DECRYPT`,
`CRYPTO_MACGENERATE`, `CRYPTO_MACVERIFY`, `CRYPTO_HASH`).
