cmake_minimum_required(VERSION 3.15)
project(vecu-microsar C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MICROSAR_ROOT "" CACHE PATH
    "Path to the Vector MICROSAR delivery (contains GenData/, BSW/, MCAL/ etc.)")

if(NOT MICROSAR_ROOT OR NOT EXISTS "${MICROSAR_ROOT}")
    message(FATAL_ERROR
        "MICROSAR_ROOT must point to a valid Vector MICROSAR delivery.\n"
        "Usage: cmake -S target-microsar -B build/target-microsar "
        "-DMICROSAR_ROOT=/path/to/microsar/delivery")
endif()

set(VECU_ABI_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../crates/vecu-abi/include")
set(VECU_BASELAYER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../baselayer/include")
set(VECU_VMCAL_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../vmcal/include")
set(VECU_VHSM_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../vhsm_adapter/include")
set(VECU_OS_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../os_mapping/include")

file(GLOB MICROSAR_BSW_SOURCES "${MICROSAR_ROOT}/BSW/*/*.c")
file(GLOB MICROSAR_GENDATA_SOURCES "${MICROSAR_ROOT}/GenData/*.c")

set(MICROSAR_INCLUDE_DIRS "")
file(GLOB MICROSAR_BSW_DIRS LIST_DIRECTORIES true "${MICROSAR_ROOT}/BSW/*")
foreach(dir ${MICROSAR_BSW_DIRS})
    if(IS_DIRECTORY "${dir}")
        list(APPEND MICROSAR_INCLUDE_DIRS "${dir}")
    endif()
endforeach()

if(EXISTS "${MICROSAR_ROOT}/GenData")
    list(APPEND MICROSAR_INCLUDE_DIRS "${MICROSAR_ROOT}/GenData")
endif()
if(EXISTS "${MICROSAR_ROOT}/Include")
    list(APPEND MICROSAR_INCLUDE_DIRS "${MICROSAR_ROOT}/Include")
endif()

add_library(vecu_microsar_shim SHARED
    src/vecu_microsar_shim.c
    src/vecu_microsar_mcal_bridge.c
    ${MICROSAR_BSW_SOURCES}
    ${MICROSAR_GENDATA_SOURCES}
)

target_include_directories(vecu_microsar_shim PUBLIC
    include
    ${VECU_ABI_INCLUDE}
    ${VECU_BASELAYER_INCLUDE}
    ${VECU_VMCAL_INCLUDE}
    ${VECU_VHSM_INCLUDE}
    ${VECU_OS_INCLUDE}
    ${MICROSAR_INCLUDE_DIRS}
)

target_compile_definitions(vecu_microsar_shim PRIVATE
    VECU_BUILD=1
    VECU_MICROSAR=1
)

if(WIN32)
    target_compile_definitions(vecu_microsar_shim PRIVATE BUILDING_DLL)
    target_compile_options(vecu_microsar_shim PRIVATE /W3)
else()
    target_compile_options(vecu_microsar_shim PRIVATE
        -fvisibility=default -Wall -Wextra
        -Wno-unused-parameter -Wno-missing-field-initializers
    )
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vmcal
                 ${CMAKE_CURRENT_BINARY_DIR}/vmcal)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vhsm_adapter
                 ${CMAKE_CURRENT_BINARY_DIR}/vhsm_adapter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../os_mapping
                 ${CMAKE_CURRENT_BINARY_DIR}/os_mapping)

target_link_libraries(vecu_microsar_shim PRIVATE vmcal vhsm_adapter os_mapping)

install(TARGETS vecu_microsar_shim
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
