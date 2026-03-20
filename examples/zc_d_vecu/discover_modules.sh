#!/bin/bash
# Discover which BSW modules are configured in GenData (have *_Cfg.h).
# Output: a CMake-includable file with the BSW_CONFIGURED_MODULES list.
#
# Usage: ./discover_modules.sh [gendata_dir]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GENDATA_DIR="${1:-$SCRIPT_DIR/../zc_d_workspace/ZC_D_C0_Appl/GenData/ZC_D_C0_Appl_Vtt}"

if [ ! -d "$GENDATA_DIR" ]; then
    echo "ERROR: GenData directory not found: $GENDATA_DIR"
    exit 1
fi

OUT="$SCRIPT_DIR/configured_modules.cmake"

echo "# Auto-generated list of configured BSW modules (from GenData *_Cfg.h)." > "$OUT"
echo "# Re-run discover_modules.sh to regenerate." >> "$OUT"
echo "set(BSW_CONFIGURED_MODULES" >> "$OUT"

count=0
for cfg in "$GENDATA_DIR"/*_Cfg.h; do
    [ -f "$cfg" ] || continue
    base=$(basename "$cfg")
    module="${base%_Cfg.h}"
    echo "    $module" >> "$OUT"
    count=$((count + 1))
done

echo ")" >> "$OUT"
echo ""
echo "Found $count configured modules in GenData."
echo "Written to: $OUT"
