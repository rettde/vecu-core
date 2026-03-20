#!/usr/bin/env python3
"""generate_memmap_stubs.py -- Generate empty *_MemMap.h stubs for vECU host builds.

Scans SIP and GenData directories for #include "Xxx_MemMap.h" directives,
then generates empty no-op stub files for each unique MemMap header found.

Usage:
    python3 tools/generate_memmap_stubs.py \
        --scan /path/to/SIP_Appl /path/to/GenData \
        --output /path/to/vecu_build/memmap_stubs/

    # Or scan the entire workspace:
    python3 tools/generate_memmap_stubs.py \
        --scan /path/to/workspace \
        --output /path/to/vecu_build/memmap_stubs/

The generated stubs are one-line no-op headers:
    /* Xxx_MemMap.h -- vECU no-op MemMap stub. SPDX-License-Identifier: MIT OR Apache-2.0 */

These shadow the SIP's target-specific MemMap headers (which contain
#pragma section directives for the target linker) via include-path priority.

SPDX-License-Identifier: MIT OR Apache-2.0
"""

import argparse
import os
import re
import sys
from pathlib import Path

MEMMAP_PATTERN = re.compile(r'#\s*include\s*[<"]([A-Za-z0-9_]+_MemMap\.h)[>"]')

HEADER_TEMPLATE = (
    '/* {name} -- vECU no-op MemMap stub. '
    'SPDX-License-Identifier: MIT OR Apache-2.0 */\n'
)


def scan_directory(scan_path: Path) -> set:
    """Scan all .c and .h files for MemMap include directives."""
    memmap_names = set()
    extensions = {'.c', '.h'}

    for root, _dirs, files in os.walk(scan_path):
        for fname in files:
            if Path(fname).suffix.lower() not in extensions:
                continue
            fpath = Path(root) / fname
            try:
                text = fpath.read_text(encoding='utf-8', errors='ignore')
            except (OSError, PermissionError):
                continue
            for match in MEMMAP_PATTERN.finditer(text):
                memmap_names.add(match.group(1))

    return memmap_names


def generate_stubs(memmap_names: set, output_dir: Path, force: bool) -> int:
    """Generate empty MemMap stub files."""
    output_dir.mkdir(parents=True, exist_ok=True)
    created = 0

    for name in sorted(memmap_names):
        out_path = output_dir / name
        if out_path.exists() and not force:
            continue
        out_path.write_text(HEADER_TEMPLATE.format(name=name))
        created += 1

    return created


def main():
    parser = argparse.ArgumentParser(
        description='Generate empty *_MemMap.h stubs for vECU host builds.'
    )
    parser.add_argument(
        '--scan', nargs='+', required=True, type=Path,
        help='Directories to scan for MemMap includes (SIP, GenData, BSW source)'
    )
    parser.add_argument(
        '--output', required=True, type=Path,
        help='Output directory for generated MemMap stubs'
    )
    parser.add_argument(
        '--force', action='store_true',
        help='Overwrite existing stub files'
    )
    parser.add_argument(
        '--dry-run', action='store_true',
        help='Print what would be generated without writing files'
    )

    args = parser.parse_args()

    all_names = set()
    for scan_path in args.scan:
        if not scan_path.is_dir():
            print(f'WARNING: {scan_path} is not a directory, skipping',
                  file=sys.stderr)
            continue
        names = scan_directory(scan_path)
        print(f'Found {len(names)} unique MemMap headers in {scan_path}')
        all_names.update(names)

    print(f'\nTotal unique MemMap headers: {len(all_names)}')

    if args.dry_run:
        for name in sorted(all_names):
            print(f'  {name}')
        return

    created = generate_stubs(all_names, args.output, args.force)
    print(f'Generated {created} stub files in {args.output}')
    if created < len(all_names):
        print(f'  ({len(all_names) - created} already existed, use --force to overwrite)')


if __name__ == '__main__':
    main()
