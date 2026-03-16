cmake_minimum_required(VERSION 3.16)
project(sample_ecu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Paths ────────────────────────────────────────────────────────────
set(BASELAYER_DIR   ${CMAKE_SOURCE_DIR}/../../baselayer)
set(ABI_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/../../crates/vecu-abi/include)

# ── BaseLayer sources ────────────────────────────────────────────────
file(GLOB BASE_SOURCES ${BASELAYER_DIR}/src/*.c)

add_library(base SHARED ${BASE_SOURCES})
target_include_directories(base PUBLIC
    ${BASELAYER_DIR}/include
    ${ABI_INCLUDE_DIR}
)
if(WIN32)
    target_compile_definitions(base PRIVATE BUILDING_DLL)
else()
    target_compile_options(base PRIVATE -fvisibility=default)
endif()

# ── Sample ECU application ───────────────────────────────────────────
set(APPL_SOURCES
    src/Appl_Entry.c
    src/SwcSensor.c
    src/SwcActuator.c
    src/SwcDiag.c
)

add_library(appl_ecu SHARED ${APPL_SOURCES})
target_include_directories(appl_ecu PUBLIC
    include
    ${BASELAYER_DIR}/include
    ${ABI_INCLUDE_DIR}
)
if(WIN32)
    target_compile_definitions(appl_ecu PRIVATE BUILDING_DLL)
else()
    target_compile_options(appl_ecu PRIVATE -fvisibility=default)
endif()

# appl_ecu calls functions from the base library (Com_ReceiveSignal, etc.)
# via inline RTE headers.  On macOS, ld64 requires all symbols to be
# resolved at link time for shared libraries, so we must link explicitly.
target_link_libraries(appl_ecu PRIVATE base)
