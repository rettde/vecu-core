cmake_minimum_required(VERSION 3.16)
project(openbsw_poc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(VECU_BUILD "Build for vECU (Virtual-MCAL + vHsm + OS-Mapping)" ON)
set(OPENBSW_ROOT "" CACHE PATH "Path to Eclipse OpenBSW source tree")

# ── Paths ────────────────────────────────────────────────────────────
set(REPO_ROOT        "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(ABI_INCLUDE_DIR  "${REPO_ROOT}/crates/vecu-abi/include")
set(VMCAL_DIR        "${REPO_ROOT}/vmcal")
set(VHSM_DIR         "${REPO_ROOT}/vhsm_adapter")
set(OS_MAP_DIR        "${REPO_ROOT}/os_mapping")
set(BASELAYER_DIR     "${REPO_ROOT}/baselayer")

# ── Virtual-MCAL (link-time MCAL replacement) ───────────────────────
file(GLOB VMCAL_SOURCES "${VMCAL_DIR}/src/*.c")

add_library(vmcal_poc STATIC ${VMCAL_SOURCES})
target_include_directories(vmcal_poc PUBLIC
    "${VMCAL_DIR}/include"
    "${ABI_INCLUDE_DIR}"
    "${BASELAYER_DIR}/include"
)
target_compile_definitions(vmcal_poc PRIVATE VECU_BUILD)
if(NOT WIN32)
    target_compile_options(vmcal_poc PRIVATE -Wall -Wextra -pedantic)
endif()

# ── vHsm Adapter (Crypto_30_vHsm → vecu-hsm) ───────────────────────
add_library(vhsm_poc STATIC "${VHSM_DIR}/src/Crypto_30_vHsm.c")
target_include_directories(vhsm_poc PUBLIC
    "${VHSM_DIR}/include"
    "${VMCAL_DIR}/include"
    "${ABI_INCLUDE_DIR}"
    "${BASELAYER_DIR}/include"
)
if(NOT WIN32)
    target_compile_options(vhsm_poc PRIVATE -Wall -Wextra -pedantic)
endif()

# ── OS-Semantics Mapping ────────────────────────────────────────────
add_library(osmap_poc STATIC "${OS_MAP_DIR}/src/Os_Mapping.c")
target_include_directories(osmap_poc PUBLIC
    "${OS_MAP_DIR}/include"
    "${BASELAYER_DIR}/include"
)
if(NOT WIN32)
    target_compile_options(osmap_poc PRIVATE -Wall -Wextra -pedantic)
endif()

# ── OpenBSW sources or stubs ────────────────────────────────────────
if(OPENBSW_ROOT AND EXISTS "${OPENBSW_ROOT}")
    message(STATUS "OpenBSW integration: using sources from ${OPENBSW_ROOT}")
    # Real OpenBSW integration:
    # Collect BSW module sources from the OpenBSW tree.
    # Adjust the glob patterns below to match the actual OpenBSW layout.
    file(GLOB_RECURSE OPENBSW_SOURCES
        "${OPENBSW_ROOT}/src/bsw/*.c"
    )
    set(BSW_SOURCES ${OPENBSW_SOURCES})
    set(BSW_INCLUDE_DIRS
        "${OPENBSW_ROOT}/include"
        "${OPENBSW_ROOT}/src/bsw"
    )
else()
    message(STATUS "OpenBSW integration: using built-in stubs (set OPENBSW_ROOT for real sources)")
    set(BSW_SOURCES src/openbsw_stubs.c)
    set(BSW_INCLUDE_DIRS "")
endif()

# ── OpenBSW BaseLayer shared library ────────────────────────────────
add_library(openbsw_base SHARED
    src/openbsw_shim.c
    ${BSW_SOURCES}
)

target_include_directories(openbsw_base PUBLIC
    include
    "${VMCAL_DIR}/include"
    "${VHSM_DIR}/include"
    "${OS_MAP_DIR}/include"
    "${ABI_INCLUDE_DIR}"
    "${BASELAYER_DIR}/include"
    ${BSW_INCLUDE_DIRS}
)

target_compile_definitions(openbsw_base PRIVATE VECU_BUILD)

target_link_libraries(openbsw_base PRIVATE
    vmcal_poc
    vhsm_poc
    osmap_poc
)

if(WIN32)
    target_compile_definitions(openbsw_base PRIVATE BUILDING_DLL)
else()
    target_compile_options(openbsw_base PRIVATE -fvisibility=default -Wall -Wextra -pedantic)
endif()
