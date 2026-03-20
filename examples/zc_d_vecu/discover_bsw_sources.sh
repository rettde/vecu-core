#!/bin/bash
# Discover which SIP BSW component directories should be compiled for vECU.
# Matches GenData *_Cfg.h modules against SIP_Appl/Components/*/Implementation/
# and excludes modules replaced by vecu-core layers.
#
# Output: bsw_sources.cmake with BSW_SOURCES list

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE="${SCRIPT_DIR}/../zc_d_workspace"
GENDATA_DIR="${WORKSPACE}/ZC_D_C0_Appl/GenData/ZC_D_C0_Appl_Vtt"
BSW_DIR="${WORKSPACE}/SIP_Appl/Components"
OUT="${SCRIPT_DIR}/bsw_sources.cmake"
TMPMOD="${SCRIPT_DIR}/.tmp_configured_modules.txt"
TMPEXCL="${SCRIPT_DIR}/.tmp_excluded.txt"

# Modules replaced by vecu-core (will NOT be compiled from SIP)
cat > "$TMPEXCL" << 'EOF'
Os
VttOs
Mcal_Rh850X2x
Crypto_30_vHsm
VttCan
VttEth
VttFr
VttDio
VttPort
VttSpi
VttGpt
VttMcu
VttFls
VttAdc
VttPwm
VttWdg
VttLin
VttIcu
VttEep
VttMem
VttCntrl
Vtt_Common
VttCANoeEmu
VttCrypto_30_Vtt
VttEthSwt_30_Vtt
VttEthTrcv_30_Vtt
VttvMem_30_Vtt
Crypto_30_LibCv
Crypto_30_Hwa
CanXcp
FrXcp
Dbg
_Common
RteAnalyzer
Wdg_30_Sbc
Sbc_30_Tcan114x
Sbc_30_Tle9278
vMem_30_Rh850Faci01
vCan_30_Rscanfd
Can_30_Core
Lin_30_Core
Eth_30_RSwt23
Eth_30_Wrapper
CanTrcv_30_Sbc
CanTrcv_30_Tja1043
FrTrcv_30_Tja1080
FrTrcv_30_Tja1085
LinTrcv_30_Generic
LinTrcv_30_Tle7259
EthTrcv_30_88Q1010
EthTrcv_30_88Qx2xx
EthTrcv_30_Dp83Tc811
EthTrcv_30_Ethmii
EthTrcv_30_Mvq324x
EthSwt_30_RSwt23
vMem_30_XXspi01
vMem_30_Blocking
vMem_30_MmFlashRO
vMem_30_XXRam
vMem_30_Fls
vMem_30_vRpcProxy
vMemAccM
vRpcProxy
FiM
SwcDiag
Ssa
veGwM
TestControl
Sd
SoAd
TcpIp
TcpIpXcp
EthIf
EthTSyn
IpBase
DoIP
IdsM
Dlt
MemAcc
Mem_30_ExFls
VdpCmRemote
CddDetLogging
Xcp
BrsHw
BrsMain
EthSM
Rtm
Fls_30_Rh850
Port_30_Rh850
Dio_30_Rh850
Mcu_30_Rh850
Fls_30_TriCore
Port_30_TriCore
Dio_30_TriCore
Mcu_30_TriCore
Adc_30
Icu_30
Gpt_30
Pwm_30
Spi_30
FblBmHdr
EthSwt_30
Lin_30_Core
Lin
Mem_30_vRpcProxy
Mem_30_LegacyAdapter
Fee_30_FlexNor
CanSM
SomeIpTp
Crypto_30_vHsm
CryIf
EOF

# Also exclude Unity build files (they #include sub-.c files → duplicates)
UNITY_PATTERN="_Unity\\.c$"

# Build list of configured module prefixes from GenData
: > "$TMPMOD"
for cfg in "$GENDATA_DIR"/*_Cfg.h; do
    [ -f "$cfg" ] || continue
    base=$(basename "$cfg")
    module="${base%_Cfg.h}"
    echo "$module" >> "$TMPMOD"
done
for cfg in "$GENDATA_DIR"/*_Lcfg.c; do
    [ -f "$cfg" ] || continue
    base=$(basename "$cfg")
    module="${base%_Lcfg.c}"
    echo "$module" >> "$TMPMOD"
done
sort -u "$TMPMOD" -o "$TMPMOD"

echo "# Auto-generated BSW source list for vECU build." > "$OUT"
echo "# Re-run discover_bsw_sources.sh to regenerate." >> "$OUT"
echo "set(BSW_SOURCES" >> "$OUT"

included=0
excluded=0
skipped=0

for comp_dir in "$BSW_DIR"/*/Implementation; do
    [ -d "$comp_dir" ] || continue
    comp_name=$(basename "$(dirname "$comp_dir")")

    # Check exclusion
    if grep -qx "$comp_name" "$TMPEXCL"; then
        excluded=$((excluded + 1))
        continue
    fi

    # Check if GenData has config for this component
    found=0
    # Direct match: component name appears in configured modules
    if grep -q "^${comp_name}" "$TMPMOD" 2>/dev/null; then
        found=1
    fi
    # Reverse: a configured module name starts with component name
    if [ $found -eq 0 ] && grep -q "^${comp_name}" "$TMPMOD" 2>/dev/null; then
        found=1
    fi
    # Short name: Can_30_Core -> Can, Lin_30_Core -> Lin, Eth_30_Wrapper -> Eth
    if [ $found -eq 0 ]; then
        short="${comp_name%%_30_*}"
        if [ "$short" != "$comp_name" ] && grep -q "^${short}" "$TMPMOD" 2>/dev/null; then
            found=1
        fi
    fi
    # Known cross-mappings
    case "$comp_name" in
        E2E|E2EXf) found=1 ;;
        VStdLib) found=1 ;;
        EcuC) found=1 ;;
        FiM) found=1 ;;
        WdgIf) found=1 ;;
        LinTrcv_30_Generic) if grep -q "^Lin" "$TMPMOD" 2>/dev/null; then found=1; fi ;;
        EthSwt_30_RSwt23) if grep -q "^Eth" "$TMPMOD" 2>/dev/null; then found=1; fi ;;
        Eth_30_RSwt23) if grep -q "^Eth" "$TMPMOD" 2>/dev/null; then found=1; fi ;;
        SwcDiag) found=1 ;;
        MemAcc|Mem_30_LegacyAdapter|Mem_30_vRpcProxy) found=1 ;;
        Tp_Iso10681) found=0 ;;
    esac

    if [ $found -eq 0 ]; then
        skipped=$((skipped + 1))
        echo "  skip: $comp_name" >&2
        continue
    fi

    # Add all .c files from this component (skip Unity build files)
    for src in "$comp_dir"/*.c; do
        [ -f "$src" ] || continue
        echo "$src" | grep -qE "$UNITY_PATTERN" && continue
        echo "    $src" >> "$OUT"
        included=$((included + 1))
    done
done

echo ")" >> "$OUT"
rm -f "$TMPMOD" "$TMPEXCL"

echo ""
echo "BSW sources: $included files from SIP Components"
echo "Excluded (vecu-replaced): $excluded components"
echo "Skipped (no GenData config): $skipped components"
echo "Written to: $OUT"
