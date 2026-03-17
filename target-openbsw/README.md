# target-openbsw — Eclipse OpenBSW Integration for vecu-core

This directory provides a build target that integrates
[Eclipse OpenBSW](https://github.com/esrlabs/openbsw) as a real BSW stack
for vecu-core, replacing the stub BaseLayer per ADR-001.

## Architecture

```
vecu-core Runtime (Rust)
        |
        v
+-------------------------------+
|  vecu_openbsw_shim            |
|  (Base_Init/Step/Shutdown)    |
+-------------------------------+
        |
        v
+-------------------------------+
|  Eclipse OpenBSW (POSIX)      |
|  - lifecycle                  |
|  - async (FreeRTOS port)      |
|  - uds, docan, doip           |
|  - cpp2can, cpp2ethernet      |
|  - storage, timer, logger     |
+-------------------------------+
        |
        v
+-------------------------------+
|  vecu_openbsw_transport       |
|  CAN/ETH -> ctx->push_tx     |
|  ctx->pop_rx -> CAN/ETH      |
+-------------------------------+
```

## How It Works

- **No clone required** — OpenBSW is fetched at configure time via
  CMake `FetchContent`. It lives only in the build directory (`_deps/`).
- **POSIX + FreeRTOS** — OpenBSW runs on the host using its POSIX
  platform with the FreeRTOS POSIX port (cooperative scheduling).
- **Shim layer** — `vecu_openbsw_shim.cpp` implements the vecu-core
  `Base_Init` / `Base_Step` / `Base_Shutdown` API and bridges to
  OpenBSW's lifecycle and async framework.
- **Transport adapter** — `vecu_openbsw_transport.cpp` routes CAN and
  Ethernet frames between OpenBSW's cpp2can/cpp2ethernet and
  vecu-core's `push_tx_frame` / `pop_rx_frame`.

## Build

```bash
cmake -S target-openbsw -B build/target-openbsw \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build/target-openbsw
```

The resulting `libvecu_openbsw_shim.so` (or `.dylib` / `.dll`) can be
loaded by vecu-loader as the BaseLayer plugin instead of `libbase.so`.

## Prerequisites

- CMake >= 3.28 (required by OpenBSW)
- C++14 compiler (gcc, clang, MSVC)
- Internet access at configure time (for FetchContent)

## Integration Status

| Component | Status | Notes |
|-----------|--------|-------|
| FetchContent pull | Done | OpenBSW fetched at configure time |
| Shim (Base_Init/Step/Shutdown) | Scaffolded | Lifecycle bridge ready for wiring |
| Transport (CAN/ETH) | Scaffolded | Frame routing via ctx callbacks |
| UDS integration | Pending | Needs OpenBSW uds module wiring |
| Storage integration | Pending | Needs SHM-backed storage adapter |
| Full async dispatch | Pending | Needs FreeRTOS tick integration |

## Differences from Stub BaseLayer

| Aspect | Stub BaseLayer | OpenBSW |
|--------|---------------|---------|
| Language | C11 | C++14 |
| State machines | Simplified stubs | Real implementations |
| UDS | 9 services (stub) | Full UDS stack |
| Transport | CanTp stub | Real docan + doip |
| Lifecycle | Simple init/step | Full lifecycle manager |
| Async/Tasks | Sequential | FreeRTOS-based |
| License | MIT/Apache-2.0 | Apache-2.0 |
