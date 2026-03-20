cmake_minimum_required(VERSION 3.15)
project(vecu_baselayer C)

# ── Options ──────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Include paths ────────────────────────────────────────────────────
# vecu-platform provides Platform_Types.h, Compiler.h, Std_Types.h, Os.h, etc.
set(VECU_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vecu-platform)
if(EXISTS ${VECU_PLATFORM_DIR}/CMakeLists.txt AND NOT TARGET vecu_platform)
    add_subdirectory(${VECU_PLATFORM_DIR} ${CMAKE_CURRENT_BINARY_DIR}/vecu-platform)
endif()

# BaseLayer's own headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ABI headers from vecu-abi (vecu_base_context.h, vecu_frame.h, vecu_status.h)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../crates/vecu-abi/include)

# ── Sources ──────────────────────────────────────────────────────────
set(BASE_SOURCES
    src/Base_Entry.c
    src/EcuM.c
    src/SchM.c
    src/Os.c
    src/Det.c
    src/Rte.c
    src/Com.c
    src/PduR.c
    src/CanIf.c
    src/EthIf.c
    src/LinIf.c
    src/FrIf.c
    src/Cry.c
    src/CryIf.c
    src/Csm.c
    src/NvM.c
    src/Fee.c
    src/MemIf.c
    src/Dem.c
    src/Dcm.c
    src/FiM.c
    src/WdgM.c
    src/CanTp.c
    src/DoIP.c
    src/ComM.c
    src/Nm.c
)

# ── Shared library ───────────────────────────────────────────────────
add_library(base SHARED ${BASE_SOURCES})
if(TARGET vecu_platform)
    target_link_libraries(base PUBLIC vecu_platform)
endif()

# Export symbols on all platforms
if(WIN32)
    target_compile_definitions(base PRIVATE BUILDING_DLL)
else()
    target_compile_options(base PRIVATE -fvisibility=default)
endif()

# Stricter warnings
if(MSVC)
    target_compile_options(base PRIVATE /W4)
else()
    target_compile_options(base PRIVATE -Wall -Wextra -pedantic)
endif()

# ── Install ──────────────────────────────────────────────────────────
install(TARGETS base
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
