cmake_minimum_required(VERSION 3.25)
project(outlast_recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -O2
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-unused-label
        -Wno-parentheses-equality
    )
endif()

set(RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gen)

include(cmake/softfloat.cmake)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RUNTIME_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${RUNTIME_DIR}/core
    ${RUNTIME_DIR}/win32
    ${CMAKE_CURRENT_SOURCE_DIR}/dxvk-build/dxvk-3.1.1/include/native/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/dxvk-build/dxvk-3.1.1/include/native/directx
)
if(NOT ANDROID)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/host_compat)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/include/host_compat/android/log.h>)
endif()

# 1. Core Runtime Library (x86 CPU semantics, SSE, x87, Memory, Dispatch)
file(GLOB RUNTIME_CORE_SOURCES
    ${RUNTIME_DIR}/core/*.cpp
    ${RUNTIME_DIR}/core/x86/*.cpp
)
list(FILTER RUNTIME_CORE_SOURCES EXCLUDE REGEX "main\\.cpp$")
list(FILTER RUNTIME_CORE_SOURCES EXCLUDE REGEX "lifted_table\\.cpp$")

add_library(recomp_runtime STATIC
    ${RUNTIME_CORE_SOURCES}
)
target_link_libraries(recomp_runtime PUBLIC softfloat)

# 2. Win32 Platform Emulation Library
file(GLOB RUNTIME_WIN32_SOURCES
    ${RUNTIME_DIR}/win32/hle.cpp
    ${RUNTIME_DIR}/win32/kernel32.cpp
    ${RUNTIME_DIR}/win32/kernel32_file.cpp
    ${RUNTIME_DIR}/win32/kernel32_misc.cpp
    ${RUNTIME_DIR}/win32/kernel32_seh.cpp
    ${RUNTIME_DIR}/win32/kernel32_sync.cpp
    ${RUNTIME_DIR}/win32/msvcr100.cpp
    ${RUNTIME_DIR}/win32/msvcr100_fs.cpp
    ${RUNTIME_DIR}/win32/xaudio_shims.cpp
    ${RUNTIME_DIR}/win32/wsock32_shims.cpp
    ${RUNTIME_DIR}/win32/user32_shims.cpp
    ${RUNTIME_DIR}/win32/shell_advapi_shims.cpp
    ${RUNTIME_DIR}/win32/pe_image.cpp
    ${RUNTIME_DIR}/win32/process.cpp
    ${RUNTIME_DIR}/win32/seh64.cpp
    ${RUNTIME_DIR}/win32/unwind.cpp
    ${RUNTIME_DIR}/win32/steam_shims.cpp
    ${RUNTIME_DIR}/win32/com.cpp
    ${RUNTIME_DIR}/win32/d3d9_bridge.cpp
    ${RUNTIME_DIR}/win32/game_anticipatory_shims.cpp
    ${RUNTIME_DIR}/win32/vcruntime_eh.cpp
)

add_library(recomp_win32 STATIC
    ${RUNTIME_WIN32_SOURCES}
)
target_link_libraries(recomp_win32 PUBLIC recomp_runtime softfloat)

# 3. Lifted Code Object Library (Batched for link performance)
file(GLOB LIFTED_SOURCES
    ${GEN_DIR}/gen_*.cpp
    ${GEN_DIR}/table.cpp
    ${RUNTIME_DIR}/core/x86/lifted_table.cpp
)

if(EXISTS ${GEN_DIR}/natives.cpp)
    list(APPEND LIFTED_SOURCES ${GEN_DIR}/natives.cpp)
endif()

# Hand-written overrides (repo rule: gen/ is never edited by hand). The lifter
# emits gen/natives.cpp from lifter.manifest, wrapping the named functions and
# calling recomp::native::<name>(c, original); these files implement them.
file(GLOB OVERRIDE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/overrides/*.cpp)
if(OVERRIDE_SOURCES)
    list(APPEND LIFTED_SOURCES ${OVERRIDE_SOURCES})
endif()

add_library(outlast_lifted STATIC
    ${LIFTED_SOURCES}
)
target_link_libraries(outlast_lifted PRIVATE recomp_runtime recomp_win32 softfloat)

# 4. Main Executable (Desktop Host Bring-Up)
add_executable(outlast_host
    ${RUNTIME_DIR}/core/main.cpp
)

target_link_libraries(outlast_host PRIVATE -Wl,--whole-archive recomp_win32 -Wl,--no-whole-archive
    outlast_lifted
    recomp_win32
    recomp_runtime
    softfloat
    dl
)
if(ANDROID)
    target_link_libraries(outlast_host PRIVATE log android)
else()
    target_link_libraries(outlast_host PRIVATE pthread)
endif()

message(STATUS "Outlast Recomp configured successfully.")
