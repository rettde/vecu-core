#!/usr/bin/env python3
"""arxml2vmcal.py — Extract Virtual-MCAL configuration from AUTOSAR ARXML.

Parses ECUC module configuration values from one or more ARXML files and
generates:

  1. VMcal_Cfg.h   — C header with #define constants
  2. vmcal_config.yaml — YAML summary for vecu-core config integration

Usage:
    python3 tools/arxml2vmcal.py tools/examples/sample_mcal.arxml -o output/

See docs/arxml-config-concept.md for the full concept description.

SPDX-License-Identifier: MIT OR Apache-2.0
"""

from __future__ import annotations

import argparse
import os
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

NS = {"ar": "http://autosar.org/schema/r4.0"}


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class CanController:
    ctrl_id: int = 0
    baudrate: int = 500000


@dataclass
class EthController:
    ctrl_id: int = 0
    mac: str = "02:00:00:00:00:01"


@dataclass
class FrController:
    ctrl_id: int = 0
    cycle_length_us: int = 5000


@dataclass
class VmcalConfig:
    can_controllers: list[CanController] = field(default_factory=list)
    can_hoh_count: int = 0
    eth_controllers: list[EthController] = field(default_factory=list)
    fr_controllers: list[FrController] = field(default_factory=list)
    dio_max_channel: int = 0
    gpt_max_channel: int = 0
    fls_total_size: int = 0
    fls_sector_size: int = 0


# ---------------------------------------------------------------------------
# ARXML parsing helpers
# ---------------------------------------------------------------------------

def _get_param(container: ET.Element, def_suffix: str) -> str | None:
    """Return the VALUE text of the first parameter whose DEFINITION-REF ends with def_suffix."""
    for pv in container.iter():
        def_ref = pv.find("ar:DEFINITION-REF", NS)
        if def_ref is not None and def_ref.text and def_ref.text.endswith(def_suffix):
            val = pv.find("ar:VALUE", NS)
            if val is not None and val.text:
                return val.text.strip()
    return None


def _parse_can(module: ET.Element, cfg: VmcalConfig) -> None:
    for container in module.findall(".//ar:ECUC-CONTAINER-VALUE", NS):
        name_el = container.find("ar:SHORT-NAME", NS)
        if name_el is None or name_el.text is None:
            continue
        name = name_el.text

        if "CanController" in name:
            ctrl = CanController()
            cid = _get_param(container, "CanControllerId")
            if cid is not None:
                ctrl.ctrl_id = int(cid)
            br = _get_param(container, "CanControllerBaudRate")
            if br is not None:
                ctrl.baudrate = int(br)
            cfg.can_controllers.append(ctrl)

        elif "CanHardwareObject" in name:
            cfg.can_hoh_count += 1


def _parse_eth(module: ET.Element, cfg: VmcalConfig) -> None:
    for container in module.findall(".//ar:ECUC-CONTAINER-VALUE", NS):
        name_el = container.find("ar:SHORT-NAME", NS)
        if name_el is None or name_el.text is None:
            continue
        name = name_el.text

        if "EthCtrlConfig" in name:
            ctrl = EthController()
            idx = _get_param(container, "EthCtrlIdx")
            if idx is not None:
                ctrl.ctrl_id = int(idx)
            mac = _get_param(container, "EthCtrlPhyAddress")
            if mac is not None:
                ctrl.mac = mac
            cfg.eth_controllers.append(ctrl)


def _parse_fr(module: ET.Element, cfg: VmcalConfig) -> None:
    for container in module.findall(".//ar:ECUC-CONTAINER-VALUE", NS):
        name_el = container.find("ar:SHORT-NAME", NS)
        if name_el is None or name_el.text is None:
            continue
        name = name_el.text

        if "FrController" in name:
            ctrl = FrController()
            idx = _get_param(container, "FrCtrlIdx")
            if idx is not None:
                ctrl.ctrl_id = int(idx)
            cyc = _get_param(container, "FrCycleLengthUs")
            if cyc is not None:
                ctrl.cycle_length_us = int(cyc)
            cfg.fr_controllers.append(ctrl)


def _parse_dio(module: ET.Element, cfg: VmcalConfig) -> None:
    for container in module.findall(".//ar:ECUC-CONTAINER-VALUE", NS):
        cid = _get_param(container, "DioChannelId")
        if cid is not None:
            cfg.dio_max_channel = max(cfg.dio_max_channel, int(cid))


def _parse_gpt(module: ET.Element, cfg: VmcalConfig) -> None:
    for container in module.findall(".//ar:ECUC-CONTAINER-VALUE", NS):
        cid = _get_param(container, "GptChannelId")
        if cid is not None:
            cfg.gpt_max_channel = max(cfg.gpt_max_channel, int(cid))


def _parse_fls(module: ET.Element, cfg: VmcalConfig) -> None:
    for container in module.findall(".//ar:ECUC-CONTAINER-VALUE", NS):
        ts = _get_param(container, "FlsTotalSize")
        if ts is not None:
            cfg.fls_total_size = int(ts)
        ss = _get_param(container, "FlsSectorSize")
        if ss is not None:
            cfg.fls_sector_size = int(ss)


MODULE_PARSERS = {
    "Can": _parse_can,
    "Eth": _parse_eth,
    "Fr": _parse_fr,
    "Dio": _parse_dio,
    "Gpt": _parse_gpt,
    "Fls": _parse_fls,
}


def parse_arxml(path: Path) -> VmcalConfig:
    """Parse an ARXML file and return the extracted VmcalConfig."""
    tree = ET.parse(path)
    root = tree.getroot()
    cfg = VmcalConfig()

    for module in root.findall(".//ar:ECUC-MODULE-CONFIGURATION-VALUES", NS):
        name_el = module.find("ar:SHORT-NAME", NS)
        if name_el is None or name_el.text is None:
            continue
        parser = MODULE_PARSERS.get(name_el.text)
        if parser:
            parser(module, cfg)

    return cfg


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------

def generate_c_header(cfg: VmcalConfig) -> str:
    """Generate VMcal_Cfg.h content."""
    lines = [
        "/* VMcal_Cfg.h — Auto-generated from ARXML by arxml2vmcal.py.",
        " * DO NOT EDIT MANUALLY.",
        " *",
        " * SPDX-License-Identifier: MIT OR Apache-2.0",
        " */",
        "",
        "#ifndef VMCAL_CFG_H",
        "#define VMCAL_CFG_H",
        "",
    ]

    lines.append("/* CAN */")
    lines.append(f"#define VMCAL_CAN_NUM_CONTROLLERS  {len(cfg.can_controllers)}u")
    for c in cfg.can_controllers:
        lines.append(f"#define VMCAL_CAN_CTRL_{c.ctrl_id}_BAUDRATE  {c.baudrate}u")
    lines.append(f"#define VMCAL_CAN_NUM_HOH          {cfg.can_hoh_count}u")
    lines.append("")

    lines.append("/* ETH */")
    lines.append(f"#define VMCAL_ETH_NUM_CONTROLLERS  {len(cfg.eth_controllers)}u")
    for c in cfg.eth_controllers:
        mac_bytes = c.mac.replace("-", ":").split(":")
        mac_init = ", ".join(f"0x{b}" for b in mac_bytes)
        lines.append(f"#define VMCAL_ETH_CTRL_{c.ctrl_id}_MAC       {{ {mac_init} }}")
    lines.append("")

    lines.append("/* FR */")
    lines.append(f"#define VMCAL_FR_NUM_CONTROLLERS   {len(cfg.fr_controllers)}u")
    for c in cfg.fr_controllers:
        lines.append(f"#define VMCAL_FR_CTRL_{c.ctrl_id}_CYCLE_US   {c.cycle_length_us}u")
    lines.append("")

    lines.append("/* DIO */")
    lines.append(f"#define VMCAL_DIO_NUM_CHANNELS     {cfg.dio_max_channel + 1}u")
    lines.append("")

    lines.append("/* GPT */")
    lines.append(f"#define VMCAL_GPT_NUM_CHANNELS     {cfg.gpt_max_channel + 1}u")
    lines.append("")

    lines.append("/* FLS */")
    lines.append(f"#define VMCAL_FLS_TOTAL_SIZE       0x{cfg.fls_total_size:X}u")
    lines.append(f"#define VMCAL_FLS_SECTOR_SIZE      0x{cfg.fls_sector_size:X}u")
    lines.append("")

    lines.append("#endif /* VMCAL_CFG_H */")
    lines.append("")
    return "\n".join(lines)


def generate_yaml(cfg: VmcalConfig) -> str:
    """Generate vmcal_config.yaml content."""
    lines = [
        "# vmcal_config.yaml — Auto-generated from ARXML by arxml2vmcal.py.",
        "# DO NOT EDIT MANUALLY.",
        "",
        "can:",
        "  controllers:",
    ]
    for c in cfg.can_controllers:
        lines.append(f"    - id: {c.ctrl_id}")
        lines.append(f"      baudrate: {c.baudrate}")
    lines.append(f"  hoh_count: {cfg.can_hoh_count}")
    lines.append("")

    lines.append("eth:")
    lines.append("  controllers:")
    for c in cfg.eth_controllers:
        lines.append(f'    - id: {c.ctrl_id}')
        lines.append(f'      mac: "{c.mac}"')
    lines.append("")

    lines.append("flexray:")
    lines.append("  controllers:")
    for c in cfg.fr_controllers:
        lines.append(f"    - id: {c.ctrl_id}")
        lines.append(f"      cycle_length_us: {c.cycle_length_us}")
    lines.append("")

    lines.append(f"dio:")
    lines.append(f"  channel_count: {cfg.dio_max_channel + 1}")
    lines.append("")

    lines.append(f"gpt:")
    lines.append(f"  channel_count: {cfg.gpt_max_channel + 1}")
    lines.append("")

    lines.append(f"fls:")
    lines.append(f"  total_size: {cfg.fls_total_size}")
    lines.append(f"  sector_size: {cfg.fls_sector_size}")
    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract Virtual-MCAL configuration from AUTOSAR ARXML."
    )
    parser.add_argument("arxml", nargs="+", type=Path, help="ARXML input file(s)")
    parser.add_argument(
        "-o", "--output-dir", type=Path, default=Path("."),
        help="Output directory for generated files (default: current dir)",
    )
    args = parser.parse_args()

    cfg = VmcalConfig()
    for path in args.arxml:
        if not path.exists():
            print(f"ERROR: {path} not found", file=sys.stderr)
            return 1
        partial = parse_arxml(path)
        cfg.can_controllers.extend(partial.can_controllers)
        cfg.can_hoh_count += partial.can_hoh_count
        cfg.eth_controllers.extend(partial.eth_controllers)
        cfg.fr_controllers.extend(partial.fr_controllers)
        cfg.dio_max_channel = max(cfg.dio_max_channel, partial.dio_max_channel)
        cfg.gpt_max_channel = max(cfg.gpt_max_channel, partial.gpt_max_channel)
        if partial.fls_total_size:
            cfg.fls_total_size = partial.fls_total_size
        if partial.fls_sector_size:
            cfg.fls_sector_size = partial.fls_sector_size

    os.makedirs(args.output_dir, exist_ok=True)

    header_path = args.output_dir / "VMcal_Cfg.h"
    header_path.write_text(generate_c_header(cfg))
    print(f"Generated: {header_path}")

    yaml_path = args.output_dir / "vmcal_config.yaml"
    yaml_path.write_text(generate_yaml(cfg))
    print(f"Generated: {yaml_path}")

    print(f"\nSummary:")
    print(f"  CAN controllers: {len(cfg.can_controllers)}, HOH: {cfg.can_hoh_count}")
    print(f"  ETH controllers: {len(cfg.eth_controllers)}")
    print(f"  FR  controllers: {len(cfg.fr_controllers)}")
    print(f"  DIO channels:    {cfg.dio_max_channel + 1}")
    print(f"  GPT channels:    {cfg.gpt_max_channel + 1}")
    print(f"  FLS total:       {cfg.fls_total_size} B, sector: {cfg.fls_sector_size} B")

    return 0


if __name__ == "__main__":
    sys.exit(main())
