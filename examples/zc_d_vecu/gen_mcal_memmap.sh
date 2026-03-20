#!/bin/bash
# Generate MCAL-specific MemMap stubs not covered by BSW list.
DIR="$(dirname "$0")/platform"
for f in ADC_MemMap.h Adc_MemMap.h Can_MemMap.h Dio_MemMap.h Eth_MemMap.h \
         Fee_MemMap.h FlsTst_MemMap.h Fls_MemMap.h Fr_59_Renesas_MemMap.h \
         Gpt_MemMap.h Gpy_Mapping.h Icu_Mapping.h Icu_MemMap.h Lin_MemMap.h \
         Mcu_MemMap.h Mem_59_Renesas_MemMap.h Ocu_MemMap.h Port_MemMap.h \
         Pwm_MemMap.h RamTst_MemMap.h Spi_MemMap.h Wdg_Mapping.h Wdg_MemMap.h \
         Icu_59_Inst0_MemMap.h Icu_59_Inst1_MemMap.h Icu_59_Inst2_MemMap.h \
         Icu_59_Inst3_MemMap.h Icu_59_Inst4_MemMap.h Icu_59_Inst5_MemMap.h \
         Icu_59_Inst6_MemMap.h Icu_59_Inst7_MemMap.h; do
    target="$DIR/$f"
    if [ ! -f "$target" ]; then
        echo "/* $f -- vECU no-op MemMap stub. SPDX-License-Identifier: MIT OR Apache-2.0 */" > "$target"
        echo "  created $f"
    fi
done
echo "Done."
