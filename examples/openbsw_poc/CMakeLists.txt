cmake_minimum_required(VERSION 3.16)

set(OPENBSW_ROOT "" CACHE PATH "Path to Eclipse OpenBSW source tree (github.com/eclipse-openbsw/openbsw)")

# Enable C++ when building with real OpenBSW (it is a C++14 SDK).
if(OPENBSW_ROOT AND EXISTS "${OPENBSW_ROOT}")
    project(openbsw_poc C CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    project(openbsw_poc C)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(VECU_BUILD "Build for vECU (Virtual-MCAL + vHsm + OS-Mapping)" ON)

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
    message(STATUS "OpenBSW integration: using real sources from ${OPENBSW_ROOT}")

    # OpenBSW BSW library sources (C++).
    # Collect all .cpp and .c from the BSW modules that map to our needs:
    #   lifecycle  → EcuM equivalent
    #   cpp2can    → CAN abstraction (CanIf equivalent)
    #   docan      → CAN transport (CanTp equivalent)
    #   transport  → Transport layer
    #   uds        → Diagnostics (Dcm equivalent)
    #   doip       → DoIP
    #   runtime    → Runtime / scheduler
    #   storage    → NvM equivalent
    set(OPENBSW_BSW_MODULES
        lifecycle cpp2can docan transport uds doip runtime storage
        async asyncImpl common estd util io middleware platform logger timer
    )
    set(OPENBSW_BSW_SOURCES "")
    foreach(mod ${OPENBSW_BSW_MODULES})
        file(GLOB_RECURSE _mod_src
            "${OPENBSW_ROOT}/libs/bsw/${mod}/src/*.cpp"
            "${OPENBSW_ROOT}/libs/bsw/${mod}/src/*.c"
        )
        list(APPEND OPENBSW_BSW_SOURCES ${_mod_src})
    endforeach()

    # OpenBSW include paths — each BSW module has include/ under its dir.
    set(OPENBSW_BSW_INCLUDE_DIRS "")
    foreach(mod ${OPENBSW_BSW_MODULES})
        if(EXISTS "${OPENBSW_ROOT}/libs/bsw/${mod}/include")
            list(APPEND OPENBSW_BSW_INCLUDE_DIRS
                "${OPENBSW_ROOT}/libs/bsw/${mod}/include")
        endif()
    endforeach()

    # POSIX platform includes (FreeRTOS POSIX port, BSP, etc.)
    list(APPEND OPENBSW_BSW_INCLUDE_DIRS
        "${OPENBSW_ROOT}/platforms/posix/bsp/bspMcu/include"
        "${OPENBSW_ROOT}/platforms/posix/bsp/bspSystemTime/include"
        "${OPENBSW_ROOT}/platforms/posix/bsp/bspStdio/include"
        "${OPENBSW_ROOT}/platforms/posix/bsp/socketCanTransceiver/include"
    )

    # 3rd-party: ETL (Embedded Template Library)
    if(EXISTS "${OPENBSW_ROOT}/libs/3rdparty/etl/include")
        list(APPEND OPENBSW_BSW_INCLUDE_DIRS
            "${OPENBSW_ROOT}/libs/3rdparty/etl/include")
    endif()

    # FreeRTOS POSIX port headers
    if(EXISTS "${OPENBSW_ROOT}/platforms/posix/3rdparty/freeRtosPosix/include")
        list(APPEND OPENBSW_BSW_INCLUDE_DIRS
            "${OPENBSW_ROOT}/platforms/posix/3rdparty/freeRtosPosix/include")
    endif()

    # BSP common includes
    foreach(_bsp_mod bsp bspCharInputOutput bspDynamicClient bspInputManager
            bspInterrupts bspOutputManager bspOutputPwm)
        if(EXISTS "${OPENBSW_ROOT}/libs/bsp/${_bsp_mod}/include")
            list(APPEND OPENBSW_BSW_INCLUDE_DIRS
                "${OPENBSW_ROOT}/libs/bsp/${_bsp_mod}/include")
        endif()
    endforeach()

    set(BSW_SOURCES
        src/openbsw_bridge.cpp
        src/VecuCanTransceiver.cpp
        ${OPENBSW_BSW_SOURCES}
    )
    set(BSW_INCLUDE_DIRS ${OPENBSW_BSW_INCLUDE_DIRS})

else()
    message(STATUS "OpenBSW integration: using built-in stubs (set OPENBSW_ROOT for real sources)")
    set(BSW_SOURCES
        src/openbsw_shim.c
        src/openbsw_stubs.c
    )
    set(BSW_INCLUDE_DIRS "")
endif()

# ── OpenBSW BaseLayer shared library ────────────────────────────────
add_library(openbsw_base SHARED
    ${BSW_SOURCES}
)

target_include_directories(openbsw_base PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
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
    target_compile_options(openbsw_base PRIVATE -fvisibility=default -Wall -Wextra)
endif()
