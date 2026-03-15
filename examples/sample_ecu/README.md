# Sample ECU — vECU Integration Example

This example demonstrates a complete vECU setup with three Software
Components (SWCs) running on the AUTOSAR-style BaseLayer.

## Architecture

```
┌─────────────────────────────────────────┐
│  vecu-loader (Rust)                     │
│  ├── vecu-appl  (ABI bridge)            │
│  │   ├── libbase.so   (BaseLayer)       │
│  │   │   ├── EcuM, SchM, Os, Det       │
│  │   │   ├── Com, PduR, CanIf, ...     │
│  │   │   ├── Csm, CryIf, Cry           │
│  │   │   ├── Dcm, Dem, NvM, Fee        │
│  │   │   └── CanTp, DoIP               │
│  │   └── libappl_ecu.so (this example) │
│  │       ├── SwcSensor   (CAN → speed) │
│  │       ├── SwcActuator (speed → CAN) │
│  │       └── SwcDiag     (UDS routine) │
│  └── vecu-hsm  (SHE crypto)            │
└─────────────────────────────────────────┘
```

## SWC Summary

| SWC | Function |
|-----|----------|
| **SwcSensor** | Reads `VehicleSpeed` (signal 0) and `BrakeActive` (signal 2) from CAN via Com |
| **SwcActuator** | Computes actuator command (`speed/10`, 0 if brake) and writes to CAN (signal 3) |
| **SwcDiag** | Reports over-speed DTC (0xC10100) and provides RoutineControl 0x0201 (read speed) |

## Quick Start

### Prerequisites

- CMake ≥ 3.16
- C compiler (gcc, clang, or MSVC)
- Rust toolchain (stable)

### Build

```bash
# 1. Build the Rust workspace
cd /path/to/vecu-core
cargo build

# 2. Build the C libraries (BaseLayer + sample ECU)
cd examples/sample_ecu
cmake -B build -S .
cmake --build build

# 3. Run the vECU (standalone mode, 1000 ticks)
cd /path/to/vecu-core
cargo run -- --config examples/sample_ecu/config.yaml
```

### Run Integration Tests

```bash
cd /path/to/vecu-core
cargo test --workspace
```

## Configuration

Edit `config.yaml` to change:
- **tick_count** — number of simulation ticks
- **tick_interval_us** — microseconds per tick
- **silkit** — uncomment to enable Vector SIL Kit co-simulation

## Signal Database (hardcoded defaults)

| Signal | ID | PDU | Bit Pos | Bits | Endian | Direction |
|--------|----|-----|---------|------|--------|-----------|
| VehicleSpeed | 0 | 0x100 | 0 | 16 | LE | RX |
| EngineRpm | 1 | 0x101 | 0 | 16 | BE | RX |
| BrakeActive | 2 | 0x100 | 16 | 1 | LE | RX |
| TxSignal | 3 | 0x200 | 0 | 8 | LE | TX |

## UDS Services

| SID | Service | Notes |
|-----|---------|-------|
| 0x10 | DiagnosticSessionControl | Default / Extended / Programming |
| 0x14 | ClearDTC | Clears all or specific DTCs |
| 0x19 | ReadDTCInformation | By status mask |
| 0x22 | ReadDataByIdentifier | DID 0xF190 (VIN), 0xF101 (data) |
| 0x27 | SecurityAccess | CMAC-based via Csm → HSM |
| 0x2E | WriteDataByIdentifier | DID 0xF190, 0xF101 |
| 0x31 | RoutineControl | 0x0201 (read speed) |
| 0x3E | TesterPresent | Session keep-alive |

## License

MIT OR Apache-2.0
