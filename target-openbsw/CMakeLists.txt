cmake_minimum_required(VERSION 3.28 FATAL_ERROR)
project(vecu-openbsw C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    openbsw
    GIT_REPOSITORY https://github.com/esrlabs/openbsw.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)

set(BUILD_TARGET_PLATFORM "POSIX" CACHE STRING "" FORCE)
set(BUILD_TARGET_RTOS     "FREERTOS" CACHE STRING "" FORCE)
set(BUILD_EXECUTABLE      "referenceApp" CACHE STRING "" FORCE)

FetchContent_GetProperties(openbsw)
if(NOT openbsw_POPULATED)
    FetchContent_Populate(openbsw)

    set(OPENBSW_SOURCE_DIR ${openbsw_SOURCE_DIR})
    set(OPENBSW_BINARY_DIR ${openbsw_BINARY_DIR})

    add_subdirectory(${OPENBSW_SOURCE_DIR}/platforms ${OPENBSW_BINARY_DIR}/platforms EXCLUDE_FROM_ALL)
    add_subdirectory(${OPENBSW_SOURCE_DIR}/libs      ${OPENBSW_BINARY_DIR}/libs      EXCLUDE_FROM_ALL)
endif()

set(VECU_ABI_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../crates/vecu-abi/include")
set(VECU_BASELAYER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../baselayer/include")
set(VECU_VMCAL_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../vmcal/include")

add_library(vecu_openbsw_shim SHARED
    src/vecu_openbsw_shim.cpp
    src/vecu_openbsw_transport.cpp
)

target_include_directories(vecu_openbsw_shim PUBLIC
    include
    ${VECU_ABI_INCLUDE}
    ${VECU_BASELAYER_INCLUDE}
    ${VECU_VMCAL_INCLUDE}
)

target_include_directories(vecu_openbsw_shim PRIVATE
    ${OPENBSW_SOURCE_DIR}/libs/bsw/lifecycle/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/async/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/common/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/cpp2can/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/cpp2ethernet/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/transport/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/uds/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/docan/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/doip/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/estd/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/util/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/logger/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/io/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/timer/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/runtime/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/bsp/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/platform/include
    ${OPENBSW_SOURCE_DIR}/libs/bsw/storage/include
    ${OPENBSW_SOURCE_DIR}/platforms/posix/include
)

target_compile_definitions(vecu_openbsw_shim PRIVATE
    VECU_BUILD=1
    SUPPORT_FREERTOS
    BUILD_TARGET_PLATFORM_POSIX
)

if(WIN32)
    target_compile_definitions(vecu_openbsw_shim PRIVATE BUILDING_DLL)
else()
    target_compile_options(vecu_openbsw_shim PRIVATE -fvisibility=default)
endif()

if(MSVC)
    target_compile_options(vecu_openbsw_shim PRIVATE /W4)
else()
    target_compile_options(vecu_openbsw_shim PRIVATE -Wall -Wextra)
endif()

install(TARGETS vecu_openbsw_shim
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
