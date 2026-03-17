cmake_minimum_required(VERSION 3.15)
project(vmcal C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ABI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../crates/vecu-abi/include")
set(BASELAYER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../baselayer/include")

add_library(vmcal STATIC
    src/VMcal_Context.c
    src/Can.c
    src/Eth.c
    src/Fr.c
    src/Dio.c
    src/Port.c
    src/Spi.c
    src/Gpt.c
    src/Mcu.c
    src/Fls.c
    src/Adc.c
    src/Pwm.c
    src/Wdg.c
    src/Lin.c
)

target_include_directories(vmcal PUBLIC
    include
    ${ABI_INCLUDE_DIR}
    ${BASELAYER_INCLUDE_DIR}
)

if(WIN32)
    target_compile_definitions(vmcal PRIVATE BUILDING_DLL)
else()
    target_compile_options(vmcal PRIVATE -Wall -Wextra -pedantic)
endif()
