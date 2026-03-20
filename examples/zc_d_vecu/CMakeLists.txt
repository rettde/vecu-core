cmake_minimum_required(VERSION 3.15)
project(zc_d_c0_vecu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ===========================================================================
# Path configuration — adapt these to your workspace layout
# ===========================================================================
# Full workspace assembled by `repo sync` (see examples/zc_d_workspace/)
set(WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../zc_d_workspace"
    CACHE PATH "Root of the repo-synced ZC_D workspace")

# vecu-core repository root
set(VECU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    CACHE PATH "Root of vecu-core")

# GenData directory (VTT variant — obtain from Windows DaVinci/Bazel build)
set(GENDATA_DIR "${WORKSPACE_ROOT}/ZC_D_C0_Appl/GenData/ZC_D_C0_Appl_Vtt"
    CACHE PATH "Path to DaVinci-generated VTT GenData")

# Project paths derived from workspace
set(PROJECT_ROOT    "${WORKSPACE_ROOT}/ZC_D_C0_Appl")
set(SIP_PATH        "${WORKSPACE_ROOT}/SIP_Appl")
set(BSW_DIR         "${SIP_PATH}/Components")
set(BSW_DEMO_DIR    "${SIP_PATH}/DemoComponents")
set(SHARED_DIR      "${WORKSPACE_ROOT}/Shared")

# vecu-core layer paths
set(ABI_INCLUDE         "${VECU_ROOT}/crates/vecu-abi/include")
set(VECU_PLATFORM_INC   "${VECU_ROOT}/vecu-platform/include")
set(VMCAL_INC           "${VECU_ROOT}/vmcal/include")
set(VMCAL_SRC           "${VECU_ROOT}/vmcal/src")
set(VHSM_INC           "${VECU_ROOT}/vhsm_adapter/include")
set(VHSM_SRC           "${VECU_ROOT}/vhsm_adapter/src")
set(OSMAP_INC           "${VECU_ROOT}/os_mapping/include")
set(OSMAP_SRC           "${VECU_ROOT}/os_mapping/src")
set(PROJECT_PLATFORM_INC "${CMAKE_CURRENT_SOURCE_DIR}/platform")

# ===========================================================================
# Sanity checks
# ===========================================================================
if(NOT EXISTS "${SIP_PATH}/Components")
    message(FATAL_ERROR
        "SIP_Appl not found at ${SIP_PATH}.\n"
        "Run 'repo sync' in examples/zc_d_workspace/ first.")
endif()

if(NOT EXISTS "${GENDATA_DIR}")
    message(WARNING
        "GenData directory not found: ${GENDATA_DIR}\n"
        "The build will fail until GenData is provided from a Windows DaVinci build.\n"
        "See README.md for instructions.")
endif()

# ===========================================================================
# Compile definitions for vECU host compilation
# ===========================================================================
add_compile_definitions(
    VECU_BUILD=1
    # Single-core vECU — map everything to Core0
    OS_CORE_ID_MASTER=0
    OS_NUMBER_OF_CORES=1
    # Disable RH850-specific features
    MCAL_RH850_DISABLED=1
    # VTT-compatible mode (reuse VTT GenData)
    PROJECT_VTT_BUILD=1
)

# ===========================================================================
# Include path priority (highest wins)
# ===========================================================================
# Force-include vecu_prefix.h (absolute path → resolves Platform_Types.h,
# Compiler.h, Std_Types.h from vecu-platform directory first, setting their
# include guards before any SIP header can pull in target-specific versions).
#
# 1. GenData         — real VTT configuration from DaVinci
# 2. SIP _Common     — complete AUTOSAR type headers (ComStack, Eth, Lin, …)
# 3. vecu-platform   — host overlay for headers SIP doesn't provide (Os, MemMap, …)
# 4. Project stubs   — project-specific empty headers (VttCntrl, *_Cfg, MemMap stubs)
# 5. vecu-core layers — vmcal, vhsm_adapter, os_mapping, ABI
# 6. SIP BSW + App   — auto-discovered below
set(VECU_INCLUDE_DIRS
    # Priority 1: GenData (real VTT configuration)
    ${GENDATA_DIR}
    ${GENDATA_DIR}/Components
    ${GENDATA_DIR}/src
    # Priority 2: SIP _Common (complete AUTOSAR type headers — Eth, Lin, ComStack, …)
    ${BSW_DIR}/_Common/Implementation
    # Priority 3: vecu-platform (Os.h, MemMap.h, VX1000.h, Compiler_Cfg.h, Crypto_GeneralTypes.h, …)
    ${VECU_PLATFORM_INC}
    # Priority 4: Project-specific stubs (VttCntrl*.h, *_Cfg.h, MemMap stubs)
    ${PROJECT_PLATFORM_INC}
    # Priority 5: vecu-core virtualization layers
    ${VMCAL_INC}
    ${VHSM_INC}
    ${OSMAP_INC}
    ${ABI_INCLUDE}
)

# ===========================================================================
# Auto-discover SIP BSW component include directories
# Exclude Vtt_Common, VttOs, VttCntrl (Windows-only Platform_Types/Compiler)
# ===========================================================================
file(GLOB BSW_COMPONENT_DIRS "${BSW_DIR}/*/Implementation")
set(BSW_INC_EXCLUDE_PATTERNS
    "Vtt_Common" "VttOs" "VttCntrl" "VttCANoeEmu"
    "VttCan" "VttEth" "VttFr" "VttDio" "VttPort" "VttSpi"
    "VttGpt" "VttMcu" "VttFls" "VttAdc" "VttPwm" "VttWdg"
    "VttLin" "VttIcu" "VttEep" "VttMem" "VttCrypto_30_Vtt"
    "VttEthSwt_30_Vtt" "VttEthTrcv_30_Vtt" "VttvMem_30_Vtt"
    "VX1000"
)
foreach(_dir ${BSW_COMPONENT_DIRS})
    set(_skip FALSE)
    foreach(_pat ${BSW_INC_EXCLUDE_PATTERNS})
        string(FIND "${_dir}" "/${_pat}/" _pos)
        if(NOT _pos EQUAL -1)
            set(_skip TRUE)
            break()
        endif()
    endforeach()
    if(NOT _skip)
        list(APPEND VECU_INCLUDE_DIRS "${_dir}")
    endif()
endforeach()
list(APPEND VECU_INCLUDE_DIRS "${BSW_DIR}/_Common/Implementation")

# ===========================================================================
# Project application include directories
# ===========================================================================
file(GLOB APP_DIRS "${PROJECT_ROOT}/Code/App/*/Implementation")
list(APPEND VECU_INCLUDE_DIRS ${APP_DIRS})
file(GLOB APPHELLA_DIRS "${PROJECT_ROOT}/Code/AppHella/*/Implementation")
list(APPEND VECU_INCLUDE_DIRS ${APPHELLA_DIRS})

list(APPEND VECU_INCLUDE_DIRS
    ${PROJECT_ROOT}/Code/Int
    ${PROJECT_ROOT}/Code/IntVTT
    ${PROJECT_ROOT}/Code/IntVTT/common
    ${PROJECT_ROOT}/Code/IntVTT/VttEtherSwt
    ${PROJECT_ROOT}/Code/IntVTT/VttEtherTrcv
    ${PROJECT_ROOT}/Code/IntVTT/VttTraceLog
    ${PROJECT_ROOT}/Code/IntVTT/VttSbc
    ${PROJECT_ROOT}/Code/IntVTT/VTT_Stubs_Hella
    ${PROJECT_ROOT}/Code/IntVTT/VttFlsvMem
    ${GENDATA_DIR}/VersionToml
    ${SHARED_DIR}/InputFiles/zc_d_c0
)

# ===========================================================================
# Virtual-MCAL sources (replacing real RH850 MCAL)
# ===========================================================================
set(VMCAL_SOURCES
    ${VMCAL_SRC}/VMcal_Context.c
    ${VMCAL_SRC}/Can.c
    ${VMCAL_SRC}/Eth.c
    ${VMCAL_SRC}/Fr.c
    ${VMCAL_SRC}/Dio.c
    ${VMCAL_SRC}/Port.c
    ${VMCAL_SRC}/Spi.c
    ${VMCAL_SRC}/Gpt.c
    ${VMCAL_SRC}/Mcu.c
    ${VMCAL_SRC}/Fls.c
    ${VMCAL_SRC}/Adc.c
    ${VMCAL_SRC}/Pwm.c
    ${VMCAL_SRC}/Wdg.c
    ${VMCAL_SRC}/Lin.c
    ${VMCAL_SRC}/Icu.c
)

# ===========================================================================
# vHsm adapter sources (replacing real Crypto_30_vHsm IPC to HSM core)
# ===========================================================================
set(VHSM_SOURCES
    ${VHSM_SRC}/Crypto_30_vHsm.c
)

# ===========================================================================
# OS-Mapping sources (replacing MICROSAR OS with deterministic scheduler)
# ===========================================================================
set(OSMAP_SOURCES
    ${OSMAP_SRC}/Os_Mapping.c
)

# ===========================================================================
# GenData sources (auto-discovered via GLOB, then filtered)
# ===========================================================================
file(GLOB_RECURSE GENDATA_ALL_C "${GENDATA_DIR}/*.c")
set(GENDATA_SOURCES ${GENDATA_ALL_C})

# Exclude VTT controller configs, hooks, RH850-specific, TSC stubs
set(GENDATA_EXCLUDE_PATTERNS
    # VTT infrastructure (replaced by vecu-core)
    "VttCntrl_"      "Vtt_Hook"       "Vtt_CustomVars"
    "VttPlatformEmu" "VttTraceLog"    "VttEthSwt"
    "Stubs_vtt"      "TSC_"
    "_30_Vtt_PBcfg"  "_30_Vtt_Lcfg"   "Wdg_30_Vtt"
    # OS GenData (replaced by os_mapping)
    "Os_"
    # RH850-specific MCAL drivers (replaced by vmcal)
    "vCan_30_Rscanfd" "vMem_30_Rh850"
    "Spi_Cfg"         "Spi_PBcfg"
    # Hardware-specific BSW (no HW on host)
    "Wdg_30_Sbc"      "Sbc_30_"
    "Can_30_Core"      "Lin_30_Core"
    "Eth_30_RSwt23"    "Eth_30_Wrapper"
    "vMem_30_XXspi01"  "vMem_30_Blocking"  "vMem_30_MmFlashRO"
    "vMem_30_XXRam"    "vMem_30_Fls"       "vMem_30_vRpcProxy"
    "vMemAccM"         "vLinkGen"
    # Application callback stubs (project provides its own)
    "_Appl_"  "_Cdd_"  "_XcpAppl"  "TestControl"
    # vRpcProxy (no RPC on host)
    "vRpcProxy"
    # Ethernet stack (replaced by vmcal at driver level)
    "SoAd"  "TcpIp"  "EthIf"  "EthTSyn"  "IpBase"  "DoIP"
    "Eth_30_Vtt"  "EthSwt_30"  "EthTrcv_30"
    # Non-essential modules
    "IdsM"  "Dlt_"  "MemAcc"  "Mem_30"  "VdpCmRemote"
    "CddDetLogging"  "Xcp"  "TcpIpXcp"
    # Board support & HW-specific
    "BrsHw"  "BrsMain"  "EthSM"  "Rtm_"  "FblBmHdr"
    "EthSwt_30"  "Fls_30"  "Port_30"  "Dio_30"  "Mcu_30"
    "Adc_30"  "Icu_30"  "Gpt_30"  "Pwm_30"  "Spi_30"
    # Application callbacks (provided by project or stubbed)
    "Appl_"  "CustomHardwareModel"  "Stubs_"
    "VttTraceLog"  "Ssa_"  "veGwM"  "Sd_"
    # MCAL PBcfg (replaced by vmcal)
    "Adc_PBcfg"  "Gpt_PBcfg"  "Icu_PBcfg"  "Pwm_PBcfg"  "Fls_Lcfg"
    # More non-essential
    "Fee_30"  "WdgM"  "WdgIf"  "MemIf"  "UdpNm"
    "CanTSyn"  "StbM"  "CddEthWul"  "DiagMdw"  "LeanScheduler"
    "Sbc_30"  "NvM_MemIf"  "Mem_30_Legacy"  "Mem_30_vRpcProxy"
    "TcpIp"
    # MCAL driver GenData (vmcal replaces)
    "Can_Lcfg"  "Can_PBcfg"  "Lin_LCfg"  "Lin_PBcfg"
    # Crypto GenData (vhsm_adapter replaces full BSW Crypto_30_vHsm)
    "CryIf_Cfg"  "Crypto_30_vHsm"  "CanIf_Lcfg"
    # NvM_Cfg (compile-time block length checks reference app variables)
    "NvM_Cfg"
    # RteAnalyzer stubs (duplicate real BSW symbols)
    "RteAnalyzer"
)
foreach(_pat ${GENDATA_EXCLUDE_PATTERNS})
    list(FILTER GENDATA_SOURCES EXCLUDE REGEX "/${_pat}")
endforeach()
list(LENGTH GENDATA_SOURCES _gendata_count)
message(STATUS "  GenData .c files: ${_gendata_count} (after filtering)")

# ===========================================================================
# BSW sources from SIP Components
# Generated by discover_bsw_sources.sh — only configured modules, excluding
# those replaced by vecu-core (OS, MCAL, Crypto_30_vHsm, VTT controllers).
# Re-run: bash discover_bsw_sources.sh
# ===========================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/bsw_sources.cmake)
list(LENGTH BSW_SOURCES _bsw_count)
message(STATUS "  BSW .c files:     ${_bsw_count}")

# ===========================================================================
# Application sources (from application.cmake — common + VTT-specific)
# ===========================================================================
set(APPL_SOURCES
    # vECU plugin bridge (Base_Init/Step/Shutdown → MICROSAR lifecycle)
    ${CMAKE_CURRENT_SOURCE_DIR}/vecu_microsar_bridge.c
)

# ===========================================================================
# Build target: shared library (loaded by vecu-loader as APPL plugin)
# ===========================================================================
add_library(zc_d_c0_vecu SHARED
    ${VMCAL_SOURCES}
    ${VHSM_SOURCES}
    ${OSMAP_SOURCES}
    ${BSW_SOURCES}
    ${GENDATA_SOURCES}
    ${APPL_SOURCES}
)

target_include_directories(zc_d_c0_vecu PRIVATE ${VECU_INCLUDE_DIRS})

# Symbol visibility
if(WIN32)
    target_compile_definitions(zc_d_c0_vecu PRIVATE BUILDING_DLL)
else()
    target_compile_options(zc_d_c0_vecu PRIVATE
        -fvisibility=default
        # Force-include host platform prefix header before anything else
        "SHELL:-include ${VECU_PLATFORM_INC}/vecu_prefix.h"
        # Warnings
        -Wall -Wextra
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-missing-field-initializers
        -Wno-implicit-int
        -Wno-implicit-function-declaration
        -Wno-typedef-redefinition
        -Wno-incompatible-pointer-types
        -Wno-nonportable-include-path
        -ferror-limit=0
    )
endif()

# Export vecu plugin entry point
if(APPLE)
    set_target_properties(zc_d_c0_vecu PROPERTIES
        SUFFIX ".dylib"
        LINK_FLAGS "-undefined dynamic_lookup"
    )
elseif(UNIX)
    set_target_properties(zc_d_c0_vecu PROPERTIES
        SUFFIX ".so"
    )
endif()

message(STATUS "=== ZC_D_C0 vECU Build Configuration ===")
message(STATUS "  Workspace:  ${WORKSPACE_ROOT}")
message(STATUS "  GenData:    ${GENDATA_DIR}")
message(STATUS "  Platform:   ${VECU_PLATFORM_INC}")
message(STATUS "  Proj stubs: ${PROJECT_PLATFORM_INC}")
message(STATUS "  vmcal:      ${VMCAL_INC}")
message(STATUS "  vHsm:       ${VHSM_INC}")
message(STATUS "  OS-Mapping: ${OSMAP_INC}")
