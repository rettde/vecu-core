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

generate_stub "FiM_Cfg_General.h"
generate_stub "FiM_Cfg_InhStatHdl.h"
generate_stub "VttCntrl_Crypto_vHsm.h"

echo "Done: $(ls -1 ${PLATFORM_DIR}/*.h | wc -l) total headers"
