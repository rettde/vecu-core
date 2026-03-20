#!/bin/bash
# Generate stub headers for all missing files identified during vECU compilation.
# These replace VTT-specific, MCAL-specific, and unconfigured module headers.

PLATFORM_DIR="$(cd "$(dirname "$0")" && pwd)/platform"

generate_stub() {
    local name="$1"
    local desc="$2"
    local file="${PLATFORM_DIR}/${name}"
    if [ -f "$file" ]; then
        return
    fi
    local guard=$(echo "$name" | tr '[:lower:].' '[:upper:]_')
    cat > "$file" << ENDSTUB
/* ${name} — vECU stub. ${desc}
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */
#ifndef ${guard}
#define ${guard}
#include "Std_Types.h"
#endif
ENDSTUB
}

# VTT Controller stubs (replaced by vmcal)
generate_stub "VttCntrl.h" "VTT control infrastructure — not used in vECU."
generate_stub "VttCntrl_Adc.h" "VTT ADC controller — replaced by vmcal Adc."
generate_stub "VttCntrl_Can.h" "VTT CAN controller — replaced by vmcal Can."
generate_stub "VttCntrl_Dio.h" "VTT DIO controller — replaced by vmcal Dio."
generate_stub "VttCntrl_Fls.h" "VTT Flash controller — replaced by vmcal Fls."
generate_stub "VttCntrl_Icu.h" "VTT ICU controller — replaced by vmcal Icu."
generate_stub "VttCntrl_Pwm.h" "VTT PWM controller — replaced by vmcal Pwm."
generate_stub "VttCntrl_SysVar.h" "VTT SysVar controller — not used in vECU."
generate_stub "VttCntrl_vMem.h" "VTT vMem controller — not used in vECU."
generate_stub "VttSpi.h" "VTT SPI — replaced by vmcal Spi."
generate_stub "CANoeApi.h" "CANoe simulation API — not used in vECU."

# VTT MCAL driver stubs (replaced by vmcal)
generate_stub "Eth_30_Vtt.h" "VTT Ethernet — replaced by vmcal Eth."
generate_stub "Eth_30_Vtt_Types.h" "VTT Ethernet types."
generate_stub "Eth_30_Vtt_IrqHandler_Ifc_Int.h" "VTT Ethernet IRQ handler."
generate_stub "Eth_30_Vtt_LL_IrqHandler_Ifc_Int.h" "VTT Ethernet LL IRQ handler."
generate_stub "Wdg_30_Vtt.h" "VTT Watchdog — replaced by vmcal Wdg."
generate_stub "Mcu_30_Vtt.h" "VTT MCU — replaced by vmcal Mcu."
generate_stub "Dio_30_Vtt.h" "VTT DIO — replaced by vmcal Dio."
generate_stub "Port_30_Vtt.h" "VTT Port — replaced by vmcal Port."

# BSW module config stubs (modules not configured or skipped)
generate_stub "Can_30_Core_Cfg.h" "Can_30_Core config — vmcal replaces."
generate_stub "Lin_30_Core_Cfg.h" "Lin_30_Core config — vmcal replaces."
generate_stub "Eth_30_RSwt23_Cfg.h" "Eth_30_RSwt23 config."
generate_stub "Eth_30_Wrapper_Lcfg.h" "Eth_30_Wrapper link-time config."
generate_stub "FiM_Cfg.h" "FiM (Function Inhibition Manager) config."
generate_stub "LinTrcv_30_Generic_Cfg.h" "LinTrcv_30_Generic config."
generate_stub "MemAcc_Cfg.h" "MemAcc config."
generate_stub "MemAcc_MemCfg.h" "MemAcc memory config."
generate_stub "Mem_30_LegacyAdapter_Cfg.h" "Mem_30_LegacyAdapter config."
generate_stub "Mem_30_vRpcProxy_Cfg.h" "Mem_30_vRpcProxy config."
generate_stub "Sbc_30_Tcan114x_Cfg.h" "Sbc_30_Tcan114x config."
generate_stub "Sbc_30_Tle9278_Cfg.h" "Sbc_30_Tle9278 config."
generate_stub "Wdg_30_Sbc_Cfg.h" "Wdg_30_Sbc config."
generate_stub "vMem_30_Blocking_Cfg.h" "vMem_30_Blocking config."
generate_stub "vMem_30_MmFlashRO_Cfg.h" "vMem_30_MmFlashRO config."
generate_stub "vMem_30_XXRam_Cfg.h" "vMem_30_XXRam config."
generate_stub "vMem_30_vRpcProxy_Cfg.h" "vMem_30_vRpcProxy config."
generate_stub "SwcDiag_Cfg.h" "SwcDiag config."
generate_stub "vRpcProxy_Mem_30_vRpcProxy.h" "vRpcProxy Mem adapter."
generate_stub "vRpcProxy_vMem_30_vRpcProxy.h" "vRpcProxy vMem adapter."

# Application stubs
generate_stub "Appl_DoIP.h" "Application DoIP callbacks."
generate_stub "Appl_Time.h" "Application time callbacks."
generate_stub "BswInit_Cfg.h" "BSW initialization config."
generate_stub "Rte_Ssa_Proxy.h" "RTE SSA Proxy interface."
generate_stub "Cdd_SomeIpTp.h" "CDD SomeIpTp interface."

echo "Generated stub headers in ${PLATFORM_DIR}/"
ls -1 "$PLATFORM_DIR"/*.h | wc -l | xargs echo "Total platform headers:"
