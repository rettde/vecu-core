# vecu-platform -- AUTOSAR-compatible host platform headers.
#
# This is a HEADER-ONLY library. It provides clean-room AUTOSAR types,
# compiler abstraction, and OS stubs for host (vECU) compilation.
#
# Usage:
#   add_subdirectory(vecu-platform)
#   target_link_libraries(my_target PRIVATE vecu_platform)
#
# SPDX-License-Identifier: MIT OR Apache-2.0

cmake_minimum_required(VERSION 3.15)
project(vecu_platform LANGUAGES C)

add_library(vecu_platform INTERFACE)

target_include_directories(vecu_platform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(vecu_platform INTERFACE
    VECU_BUILD=1
)
