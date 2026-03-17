cmake_minimum_required(VERSION 3.15)
project(os_mapping C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(BASELAYER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../baselayer/include")

add_library(os_mapping STATIC
    src/Os_Mapping.c
)

target_include_directories(os_mapping PUBLIC
    include
    ${BASELAYER_INCLUDE_DIR}
)

if(WIN32)
    target_compile_definitions(os_mapping PRIVATE BUILDING_DLL)
else()
    target_compile_options(os_mapping PRIVATE -Wall -Wextra -pedantic)
endif()
