#!/bin/bash
# Generate empty MemMap stub headers for vECU host compilation.
# Each MICROSAR module includes its own *_MemMap.h to map code/data
# to linker sections.  On the host all sections are no-ops.
#
# Usage: ./generate_memmap_stubs.sh
# Run from the zc_d_vecu directory.

PLATFORM_DIR="$(dirname "$0")/platform"
LIST="$PLATFORM_DIR/memmap_list.txt"

if [ ! -f "$LIST" ]; then
    echo "ERROR: $LIST not found"
    exit 1
fi

while IFS= read -r filename; do
    [ -z "$filename" ] && continue
    target="$PLATFORM_DIR/$filename"
    if [ ! -f "$target" ]; then
        echo "/* $filename -- vECU no-op MemMap stub. SPDX-License-Identifier: MIT OR Apache-2.0 */" > "$target"
        echo "  created $filename"
    fi
done < "$LIST"

echo "Done. $(wc -l < "$LIST" | tr -d ' ') MemMap stubs checked."
