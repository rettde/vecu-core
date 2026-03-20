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

generate_stub "BswInit_Compiler_Cfg.h"
generate_stub "CANoeEmuProcessor.h"
generate_stub "Eth_30_RSwt23_GenTypes.h"
generate_stub "Eth_30_Vtt_Int.h"
generate_stub "FiM_Lcfg.h"
generate_stub "Lin_30_Core_Lcfg.h"
generate_stub "Mcu_30_Vtt_Ctrl.h"
generate_stub "SchM_Eth_30_Wrapper.h"
generate_stub "SchM_MemAcc.h"
generate_stub "SchM_Sbc_30_Tcan114x.h"
generate_stub "SchM_Sbc_30_Tle9278.h"
generate_stub "SchM_Wdg_30_Sbc.h"
generate_stub "SwcDiag_Lcfg.h"
generate_stub "VttCntrl_Eth.h"
generate_stub "VttCntrl_Gpt.h"
generate_stub "vMem_30_Blocking_PrivateCfg.h"
generate_stub "vMem_30_Fls_Cfg.h"

echo "Done: $(ls -1 ${PLATFORM_DIR}/*.h | wc -l) total headers"
