# ARXML → Virtual-MCAL Configuration Concept

ADR-002 §4 requires that Virtual-MCAL configuration is **derived from the
series MCAL configuration** (ARXML), not manually maintained.

This document defines the concept, rules, and tooling approach.

## Problem Statement

Series ECU projects use DaVinci Configurator to generate MCAL configuration
from ARXML descriptions.  The generated configuration includes:

- CAN controller IDs, baud rates, hardware object handles (HOH)
- Ethernet controller MACs, VLAN configs, buffer sizes
- FlexRay cluster parameters, slot assignments
- DIO channel/port mappings
- GPT channel periods
- Flash sector layouts
- SPI sequences, jobs, channels

For a Level-3 vECU, the Virtual-MCAL must use **the same logical IDs and
channel mappings** so that the BSW (Com, PduR, CanIf, EthIf, etc.) works
without modification.

## Configuration Rules

### Taken Over (1:1 from ARXML)

| Parameter | ARXML Path | Virtual-MCAL Usage |
|-----------|-----------|-------------------|
| CAN Controller ID | `/CanController/CanControllerId` | Controller index in `Can_Init` |
| CAN HOH ID | `/CanHardwareObject/CanObjectId` | Mailbox routing in `Can_Write` |
| CAN Baud Rate | `/CanController/CanControllerBaudRate` | Logged, not enforced |
| ETH Controller ID | `/EthCtrlConfig/EthCtrlIdx` | Controller index |
| ETH MAC Address | `/EthCtrlConfig/EthCtrlPhyAddress` | `Eth_GetPhysAddr` return value |
| FR Controller ID | `/FrController/FrCtrlIdx` | Controller index |
| FR Slot ID | `/FrAbsoluteTimer/FrAbsTimerIdx` | Frame slot routing |
| DIO Channel ID | `/DioChannel/DioChannelId` | RAM-backed channel index |
| GPT Channel ID | `/GptChannel/GptChannelId` | Timer channel index |
| FLS Sector Layout | `/FlsSector/*` | SHM offset calculations |

### Ignored (not relevant for virtualization)

| Parameter | Reason |
|-----------|--------|
| Register addresses | No HW registers in Virtual-MCAL |
| Interrupt priorities | No interrupt emulation |
| DMA configurations | No DMA in host execution |
| Clock settings | Host clock is used |
| Pin mux / pad configs | No physical pins |
| Baud rate prescalers | No physical bus timing |

### Virtualized (mapped to Virtual-MCAL semantics)

| Parameter | Series Meaning | Virtual Meaning |
|-----------|---------------|----------------|
| CAN RX FIFO size | HW FIFO depth | `CAN_RX_QUEUE_SIZE` |
| ETH buffer count | DMA descriptors | `ETH_TX_BUF_COUNT` |
| GPT tick period | HW timer period | Tick-based counter increment |
| FLS sector size | Flash geometry | SHM region size |

## Tooling Approach

### Phase 1: Standalone Extraction Script (this PoC)

A Python script (`tools/arxml2vmcal.py`) that:

1. Parses ARXML files (standard XML with AUTOSAR schema)
2. Extracts the parameters listed above
3. Generates a C header (`VMcal_Cfg.h`) with `#define` constants
4. Generates a YAML summary for `config.yaml` integration

### Phase 2: CI Integration

- Script runs as a CI step after DaVinci generation
- Output is committed alongside generated BSW code
- Diff detection ensures configuration drift is visible

### Phase 3: Full Integration (future)

- Direct integration into DaVinci workflow via post-generation hook
- Validation against Virtual-MCAL capabilities
- Automated mismatch reporting

## Generated Output Format

### VMcal_Cfg.h (example)

```c
/* Auto-generated from ARXML — do not edit manually. */
#ifndef VMCAL_CFG_H
#define VMCAL_CFG_H

/* CAN */
#define VMCAL_CAN_NUM_CONTROLLERS  2u
#define VMCAL_CAN_CTRL_0_BAUDRATE  500000u
#define VMCAL_CAN_CTRL_1_BAUDRATE  250000u
#define VMCAL_CAN_NUM_HOH          8u

/* ETH */
#define VMCAL_ETH_NUM_CONTROLLERS  1u
#define VMCAL_ETH_CTRL_0_MAC       { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 }

/* FR */
#define VMCAL_FR_NUM_CONTROLLERS   1u
#define VMCAL_FR_CYCLE_LENGTH_US   5000u

/* DIO */
#define VMCAL_DIO_NUM_CHANNELS     32u

/* GPT */
#define VMCAL_GPT_NUM_CHANNELS     4u

/* FLS */
#define VMCAL_FLS_TOTAL_SIZE       0x10000u
#define VMCAL_FLS_SECTOR_SIZE      0x1000u

#endif /* VMCAL_CFG_H */
```

### vmcal_config.yaml (example)

```yaml
can:
  controllers:
    - id: 0
      baudrate: 500000
    - id: 1
      baudrate: 250000
  hoh_count: 8

eth:
  controllers:
    - id: 0
      mac: "02:00:00:00:00:01"

flexray:
  controllers:
    - id: 0
      cycle_length_us: 5000

dio:
  channel_count: 32

gpt:
  channel_count: 4

fls:
  total_size: 65536
  sector_size: 4096
```

## License

MIT OR Apache-2.0
