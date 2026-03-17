cmake_minimum_required(VERSION 3.15)
project(vhsm_adapter C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ABI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../crates/vecu-abi/include")
set(BASELAYER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../baselayer/include")
set(VMCAL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vmcal/include")

add_library(vhsm_adapter STATIC
    src/Crypto_30_vHsm.c
)

target_include_directories(vhsm_adapter PUBLIC
    include
    ${VMCAL_INCLUDE_DIR}
    ${ABI_INCLUDE_DIR}
    ${BASELAYER_INCLUDE_DIR}
)

if(WIN32)
    target_compile_definitions(vhsm_adapter PRIVATE BUILDING_DLL)
else()
    target_compile_options(vhsm_adapter PRIVATE -Wall -Wextra -pedantic)
endif()
