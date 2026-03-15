# vecu-core

Cross-platform, deterministic virtual ECU execution system with
multi-bus SIL Kit integration (CAN, Ethernet, LIN, FlexRay).

## Crate Overview

| Crate | Description |
|-------|-------------|
| `vecu-abi` | ABI definitions: `VecuPluginApi`, `VecuFrame`, `BusType`, SHM layout |
| `vecu-shm` | Shared-memory manager (ADR-003): ring-buffer queues, diagnostic mailbox |
| `vecu-runtime` | Deterministic tick orchestrator, `RuntimeAdapter` + `OpenSutApi` traits |
| `vecu-appl` | Reference APPL module (`cdylib`): frame I/O echo |
| `vecu-hsm` | Reference HSM module (`cdylib`): seed/key, sign/verify stubs |
| `vecu-silkit` | Vector SIL Kit adapter: dynamic FFI, multi-bus controllers |
| `vecu-loader` | CLI loader & orchestrator: plugin loading, ABI negotiation, simulation |

## Architecture

```text
                    SIL Kit Registry
                         |
          +--------------+--------------+
          |              |              |
       CANoe         Silver        DTS.monaco
          |              |              |
          +--------------+--------------+
                         |
              SIL Kit Bus (CAN/ETH/LIN/FR)
                         |
    +--------------------+--------------------+
    |           vECU Runtime Env               |
    |                                          |
    |   APPL.dll ──→ VecuPluginApi             |
    |       ↕         push_frame / poll_frame  |
    |   shared memory                          |
    |       ↕                                  |
    |   HSM.dll  ──→ VecuPluginApi             |
    |       ↕         seed / key / diag        |
    |   loader  ──→ SilKitAdapter ──→ SIL Kit  |
    +------------------------------------------+
```

## Execution Modes

- **Standalone** (`--mode standalone`): Fixed tick count, no external dependencies.
- **SIL Kit** (`--mode silkit`): Participant in a SIL Kit co-simulation.
  Ticks driven by `TimeSyncService`.

## Bus Type Support

`VecuFrame` carries a `bus_type` discriminator (`BusType` enum).
All bus types are routed through the `OpenSutApi` abstraction layer.

| Value | Bus | SIL Kit Controller | Status |
|-------|-----|--------------------|--------|
| 0 | CAN | `SilKit_CanController` | Implemented |
| 1 | Ethernet | `SilKit_EthernetController` | Implemented |
| 2 | LIN | `SilKit_LinController` | Implemented (master mode) |
| 3 | FlexRay | `SilKit_FlexrayController` | Implemented (passive listen) |

> **Note:** FlexRay requires external cluster/node configuration via
> `SilKit_FlexrayController_Configure()` before active participation.
> Without configuration the controller registers a frame handler but
> does not execute `RUN`.

### SIL Kit YAML Configuration

```yaml
appl: target/debug/libvecu_appl.dylib
hsm: target/debug/libvecu_hsm.dylib
mode: silkit
silkit:
  registry_uri: silkit://localhost:8500
  participant_name: vECU
  can_network: CAN1
  step_size_ns: 1000000
  coordinated: true
  # Optional multi-bus controllers
  eth_network: ETH1
  eth_controller_name: EthernetController
  lin_network: LIN1
  lin_controller_name: LinController
  flexray_network: FR1
  flexray_controller_name: FlexRayController
```

## OpenSUT API

The `OpenSutApi` trait (`vecu_runtime::OpenSutApi`) is the formal abstraction
layer between the vECU runtime and external communication infrastructure:

```text
┌─────────────────────────────────────────────────────┐
│  vECU Runtime Env                                   │
│                                                     │
│  APPL.dll ──┐                                       │
│             ├──→ [OpenSutApi] ──→ CAN/ETH/LIN/FR ──┼──→ SIL Kit
│  HSM.dll  ──┘                                       │
└─────────────────────────────────────────────────────┘
```

| Trait | Responsibility |
|-------|---------------|
| `RuntimeAdapter` | **When** ticks happen (timer, SIL Kit `TimeSyncService`) |
| `OpenSutApi` | **Where** frames go (SIL Kit, hardware, mock) |

### Implementations

| Type | Crate | Description |
|------|-------|-------------|
| `NullBus` | `vecu-runtime` | No-op (test / standalone) |
| `SilKitBus` | `vecu-silkit` | Multi-bus SIL Kit controllers via shared RX buffer |
| *(SHM fallback)* | `vecu-runtime` | When no bus is set, `Runtime::tick()` uses SHM queues |

## Building

```sh
cargo build --workspace
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
```

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or
  <http://www.apache.org/licenses/LICENSE-2.0>)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or
  <http://opensource.org/licenses/MIT>)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
