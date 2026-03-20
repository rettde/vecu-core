#!/bin/bash
# Generate a single stub header in platform/
# Usage: gen_stub.sh Header1.h Header2.h ...
PLATFORM_DIR="$(cd "$(dirname "$0")" && pwd)/platform"
for name in "$@"; do
    file="${PLATFORM_DIR}/${name}"
    [ -f "$file" ] && continue
    guard=$(echo "$name" | tr '[:lower:].' '[:upper:]_')
    printf '/* %s - vECU stub. */\n#ifndef %s\n#define %s\n#include "Std_Types.h"\n#endif\n' \
        "$name" "$guard" "$guard" > "$file"
    echo "  created: $name"
done
