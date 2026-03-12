cmake_minimum_required(VERSION 3.16)
project(HotVM VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Detect platform
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(HOTVM_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(HOTVM_ARCH "x86_64")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "HotVM target architecture: ${HOTVM_ARCH}")

# ── Core library ──────────────────────────────────────────────

set(HOTVM_CORE_SOURCES
    src/vm/interpreter.cpp
    src/runtime/runtime.cpp
    src/runtime/wrapper_table.cpp
    src/runtime/module.cpp
    src/runtime/type_registry.cpp
    src/patch/function_patch.cpp
    src/patch/vtable_patch.cpp
    src/patch/patch_manifest.cpp
    src/patch/patch_manager.cpp
    src/eh/exception_bridge.cpp
)

# Platform-specific assembly
if(HOTVM_ARCH STREQUAL "arm64")
    list(APPEND HOTVM_CORE_SOURCES
        src/abi/arm64/wrappers.S
        src/abi/arm64/adapter.S
    )
elseif(HOTVM_ARCH STREQUAL "x86_64")
    list(APPEND HOTVM_CORE_SOURCES
        src/abi/x86_64/wrappers.S
        src/abi/x86_64/adapter.S
    )
endif()

add_library(hotvm_core STATIC ${HOTVM_CORE_SOURCES})
target_include_directories(hotvm_core PUBLIC include)
target_compile_definitions(hotvm_core PUBLIC
    HOTVM_ARCH_${HOTVM_ARCH}=1
)

# ── Compiler (Phase 2) ───────────────────────────────────────

option(HOTVM_BUILD_COMPILER "Build the HotVM compiler (requires libclang)" OFF)

if(HOTVM_BUILD_COMPILER)
    find_package(Clang REQUIRED CONFIG)

    set(HOTVM_COMPILER_SOURCES
        compiler/frontend/ast_visitor.cpp
        compiler/codegen/codegen.cpp
        compiler/codegen/module_writer.cpp
        compiler/driver/main.cpp
    )

    add_executable(hotvm-compile ${HOTVM_COMPILER_SOURCES})
    target_include_directories(hotvm-compile PRIVATE include compiler)
    target_link_libraries(hotvm-compile PRIVATE hotvm_core clang-cpp LLVM)
endif()

# ── Diff tool (Phase 5) ──────────────────────────────────────

set(HOTVM_DIFF_SOURCES
    tools/hotdiff/diff.cpp
    tools/hotdiff/main.cpp
)

add_executable(hotvm-diff ${HOTVM_DIFF_SOURCES})
target_include_directories(hotvm-diff PRIVATE include)
target_link_libraries(hotvm-diff PRIVATE hotvm_core)

# ── Tests ─────────────────────────────────────────────────────

option(HOTVM_BUILD_TESTS "Build tests" ON)

if(HOTVM_BUILD_TESTS)
    enable_testing()

    file(GLOB TEST_SOURCES tests/*.cpp)
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE hotvm_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

# ── Examples ──────────────────────────────────────────────────

option(HOTVM_BUILD_EXAMPLES "Build examples" ON)

if(HOTVM_BUILD_EXAMPLES)
    file(GLOB EXAMPLE_SOURCES examples/*.cpp)
    foreach(ex_src ${EXAMPLE_SOURCES})
        get_filename_component(ex_name ${ex_src} NAME_WE)
        add_executable(example_${ex_name} ${ex_src})
        target_link_libraries(example_${ex_name} PRIVATE hotvm_core)
    endforeach()
endif()
