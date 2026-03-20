#!/bin/bash
PLATFORM_DIR="$(cd "$(dirname "$0")" && pwd)/platform"

generate_stub() {
    local name="$1"
    local file="${PLATFORM_DIR}/${name}"
    [ -f "$file" ] && return
    local guard=$(echo "$name" | tr '[:lower:].' '[:upper:]_')
    printf '/* %s - vECU stub. */\n#ifndef %s\n#define %s\n#include "Std_Types.h"\n#endif\n' \
        "$name" "$guard" "$guard" > "$file"
}

generate_stub "Eth_30_RSwt23_Lcfg.h"
generate_stub "Lin_30_Core_PBcfg.h"
generate_stub "Os_Hal_Processor.h"
generate_stub "SchM_FiM.h"

echo "Done: $(ls -1 ${PLATFORM_DIR}/*.h | wc -l) total headers"
