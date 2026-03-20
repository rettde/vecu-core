cmake_minimum_required(VERSION 3.15)
project(vecu-core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(VECU_BUILD "Build for vECU (Virtual-MCAL + vHsm + OS-Mapping)" OFF)

add_subdirectory(vecu-platform)
add_subdirectory(baselayer)

if(VECU_BUILD)
    add_subdirectory(vmcal)
    add_subdirectory(vhsm_adapter)
    add_subdirectory(os_mapping)
endif()

## sample_ecu is built standalone (it defines its own 'base' target).
## Use: cmake -B build -S examples/sample_ecu
