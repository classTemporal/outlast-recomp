cmake_minimum_required(VERSION 3.25)
project(outlast_recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Optimization and performance flags
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

# Directories
set(RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gen)

# Dependencies
include(cmake/softfloat.cmake)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RUNTIME_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${RUNTIME_DIR}/core
)

# 1. Core Runtime Library (x86 CPU semantics, SSE, x87, Memory, Dispatch)
file(GLOB RUNTIME_CORE_SOURCES
    ${RUNTIME_DIR}/core/*.cpp
    ${RUNTIME_DIR}/core/x86/*.cpp
)
# Exclude main.cpp and lifted_table.cpp
list(FILTER RUNTIME_CORE_SOURCES EXCLUDE REGEX "main\\.cpp$")
list(FILTER RUNTIME_CORE_SOURCES EXCLUDE REGEX "lifted_table\\.cpp$")

add_library(recomp_runtime STATIC
    ${RUNTIME_CORE_SOURCES}
)
target_link_libraries(recomp_runtime PUBLIC softfloat)

# 2. Lifted Code Object Library (Batched for link performance)
file(GLOB LIFTED_SOURCES
    ${GEN_DIR}/gen_*.cpp
    ${GEN_DIR}/table.cpp
    ${RUNTIME_DIR}/core/x86/lifted_table.cpp
)

if(EXISTS ${GEN_DIR}/natives.cpp)
    list(APPEND LIFTED_SOURCES ${GEN_DIR}/natives.cpp)
endif()

add_library(outlast_lifted STATIC
    ${LIFTED_SOURCES}
)
target_link_libraries(outlast_lifted PRIVATE recomp_runtime softfloat)

# 3. Main Executable (Desktop Host Bring-Up)
add_executable(outlast_host
    ${RUNTIME_DIR}/core/main.cpp
)

target_link_libraries(outlast_host PRIVATE
    outlast_lifted
    recomp_runtime
    softfloat
    pthread
    dl
)

message(STATUS "Outlast Recomp configured successfully.")
