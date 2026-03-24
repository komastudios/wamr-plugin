cmake_minimum_required(VERSION 3.21)
project(wamr-unity VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED "Build shared library" ON)
option(USE_MSVC_STATIC_RUNTIME "Use MSVC static runtime" OFF)
option(BUILD_AOT_COMPILER "Build AOT compiler (requires LLVM)" OFF)
option(WAMR_ENABLE_AOT_MONOLITH "Link AOT compiler into main wamr shared library" ON)
option(BUILD_LIBHOST_NATIVE "Build host_native shared library for iwasm --native-lib" ON)

# Execution mode options (see https://bytecodealliance.github.io/wamr.dev/blog/introduction-to-wamr-running-modes/)
option(BUILD_INTERP      "Enable interpreter"                             ON)
option(BUILD_FAST_INTERP "Use fast interpreter (incompatible with fast JIT)" ON)
option(BUILD_FAST_JIT    "Enable fast JIT"                                OFF)
option(BUILD_JIT         "Enable LLVM JIT (requires LLVM)"                OFF)
option(BUILD_AOT         "Enable AOT execution"                           OFF)

option(BUILD_LAUNCHER "Build iwasm executable"                            ON)

set(WAMR_LIB_SUFFIX "" CACHE STRING "Variant suffix for output library name (e.g. -fast-interp)")

# ---------- Deterministic build flags (matches lz4/xxHash/BLAKE3 pattern) ----------

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
    add_compile_options("-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "AppleClang")
    add_compile_options(-Wno-shorten-64-to-32)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options("/Brepro")
    add_link_options("/Brepro")
    add_compile_options("/pathmap:${CMAKE_CURRENT_SOURCE_DIR}=.")
    # Suppress noisy warnings from third-party code (WAMR, LLVM, Zydis)
    add_compile_options(
        /wd4100  # unreferenced formal parameter
        /wd4127  # conditional expression is constant
        /wd4146  # unary minus applied to unsigned type
        /wd4152  # nonstandard extension, function/data pointer conversion
        /wd4244  # conversion, possible loss of data
        /wd4245  # signed/unsigned mismatch in conversion
        /wd4267  # conversion from 'size_t', possible loss of data
        /wd4273  # inconsistent dll linkage
        /wd4324  # structure was padded due to alignment specifier
        /wd4458  # declaration hides class member
        /wd4624  # destructor was implicitly defined as deleted
    )
endif()

# ---------- Platform-specific settings ----------

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(BUILD_SHARED OFF)
    set(BUILD_LAUNCHER OFF)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -g1 -DNDEBUG")
    add_compile_options(-fsigned-char)
    add_link_options("-Wl,-z,max-page-size=16384")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
    set(BUILD_SHARED OFF)
    set(BUILD_LAUNCHER OFF)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -g1 -DNDEBUG")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(BUILD_SHARED OFF)
    set(BUILD_LAUNCHER OFF)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -g1 -DNDEBUG")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    if(USE_MSVC_STATIC_RUNTIME)
        message(STATUS "Using MSVC static runtime")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        message(STATUS "Using MSVC dynamic runtime")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

# ---------- Desktop vs mobile/embedded ----------

set(WAMR_DESKTOP_BUILD FALSE)
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Android|iOS|Emscripten")
    set(WAMR_DESKTOP_BUILD TRUE)
endif()

# ---------- Third-party static libraries (for tests / static builds) ----------

function(add_third_party_library dir)
    set(BUILD_SHARED OFF)
    set(BUILD_TESTING OFF)
    add_subdirectory(${dir} EXCLUDE_FROM_ALL)
endfunction()

# ---------- LZ4 / xxHash / BLAKE3 (for host_native shared library) ----------

if(BUILD_LIBHOST_NATIVE AND BUILD_SHARED AND WAMR_DESKTOP_BUILD)
    add_third_party_library(third_party/lz4)
    add_third_party_library(third_party/xxhash)
    # blake3 detects SIMD via CMAKE_SYSTEM_PROCESSOR which is the host arch.
    # Override for macOS cross-compilation where the target arch differs.
    if(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES)
        if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
            set(BLAKE3_SIMD_TYPE "amd64-asm" CACHE STRING "" FORCE)
        elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
            set(BLAKE3_SIMD_TYPE "neon-intrinsics" CACHE STRING "" FORCE)
        endif()
    endif()
    add_third_party_library(third_party/blake3/c)
endif()

# ---------- Binaryen (desktop only) ----------
# IMPORTANT: add_subdirectory MUST happen BEFORE the WAMR include() below,
# because WAMR's runtime_lib.cmake calls include_directories() globally and
# its wasm.h would shadow binaryen's own wasm.h header.

if(WAMR_DESKTOP_BUILD)
    enable_language(CXX)

    if(BINARYEN_ROOT)
        # External pre-built binaryen (static library)
        find_library(BINARYEN_LIB binaryen PATHS "${BINARYEN_ROOT}/lib" NO_DEFAULT_PATH)
        if(NOT BINARYEN_LIB)
            message(FATAL_ERROR "Cannot find binaryen library in ${BINARYEN_ROOT}")
        endif()

        add_library(binaryen STATIC IMPORTED)
        set_target_properties(binaryen PROPERTIES IMPORTED_LOCATION "${BINARYEN_LIB}")

        find_program(WASM_AS_EXECUTABLE wasm-as PATHS "${BINARYEN_ROOT}/bin" NO_DEFAULT_PATH)
        if(NOT WASM_AS_EXECUTABLE)
            message(FATAL_ERROR "Cannot find wasm-as in ${BINARYEN_ROOT}/bin")
        endif()
    else()
        # Build binaryen from source
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
        set(BUILD_LLVM_DWARF ON CACHE BOOL "" FORCE)
        set(BYN_ENABLE_LTO OFF CACHE BOOL "" FORCE)
        set(ENABLE_WERROR OFF CACHE BOOL "" FORCE)
        add_subdirectory(binaryen EXCLUDE_FROM_ALL)
    endif()
endif()

# ---------- WAMR source (git submodule at wamr/, pinned to WAMR-2.4.4) ----------

set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wamr)

# ---------- WAMR build configuration ----------
# Resolve running mode compatibility constraints:
#   - Fast JIT requires classic interpreter (not fast-interp)
#   - LLVM JIT requires AOT support
#   - Multi-tier JIT (both JITs) requires both fast JIT + LLVM JIT

# Resolve compatibility constraints
if(BUILD_FAST_JIT AND BUILD_FAST_INTERP)
    message(STATUS "Fast JIT enabled — disabling fast-interp (incompatible)")
    set(BUILD_FAST_INTERP OFF)
endif()

if(BUILD_JIT AND NOT BUILD_AOT)
    message(STATUS "LLVM JIT enabled — enabling AOT (required)")
    set(BUILD_AOT ON)
endif()

if(BUILD_FAST_JIT AND NOT BUILD_INTERP)
    message(STATUS "Fast JIT enabled — enabling interpreter (required)")
    set(BUILD_INTERP ON)
endif()

# Map to WAMR's expected integer variables (1/0)
macro(bool_to_int var_in var_out)
    if(${var_in})
        set(${var_out} 1)
    else()
        set(${var_out} 0)
    endif()
endmacro()
bool_to_int(BUILD_INTERP      WAMR_BUILD_INTERP)
bool_to_int(BUILD_FAST_INTERP  WAMR_BUILD_FAST_INTERP)
bool_to_int(BUILD_FAST_JIT     WAMR_BUILD_FAST_JIT)
bool_to_int(BUILD_JIT          WAMR_BUILD_JIT)
bool_to_int(BUILD_AOT          WAMR_BUILD_AOT)

set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_SHARED_MEMORY 0)
set(WAMR_BUILD_THREAD_MGR 0)
set(WAMR_BUILD_BULK_MEMORY 1)
set(WAMR_BUILD_REF_TYPES 0)
set(WAMR_BUILD_SIMD 0)
set(WAMR_BUILD_TAIL_CALL 0)

# ---------- LLVM (required for AOT compiler and LLVM JIT) ----------

if(BUILD_AOT_COMPILER AND WAMR_DESKTOP_BUILD)
    set(CMAKE_CXX_STANDARD 17)

    set(LLVM_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/wamr/core/deps/llvm")
    if(NOT EXISTS "${LLVM_SRC_ROOT}/build")
        message(FATAL_ERROR "Cannot find LLVM dir: ${LLVM_SRC_ROOT}/build")
    endif()
    set(CMAKE_PREFIX_PATH "${LLVM_SRC_ROOT}/build;${CMAKE_PREFIX_PATH}")

    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

    if(LLVM_LINK_LLVM_DYLIB)
        set(WAMR_LLVM_LIBS LLVM)
    else()
        set(WAMR_LLVM_LIBS ${LLVM_AVAILABLE_LIBS})
    endif()
endif()

# ---------- Build WAMR library targets ----------

# WAMR's runtime_lib.cmake requires WAMR_BUILD_PLATFORM to be set.
# (Auto-detection only exists in WAMR's root CMakeLists.txt, which we skip.)
if(NOT DEFINED WAMR_BUILD_PLATFORM)
    if(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
        set(WAMR_BUILD_PLATFORM "darwin")
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        # Custom platform layer: avoids WASI type conflicts and unsupported
        # signal/pthread features.  See cmake/platform/emscripten/.
        set(WAMR_BUILD_PLATFORM "emscripten")
        set(SHARED_PLATFORM_CONFIG
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/emscripten/shared_platform.cmake)
    else()
        string(TOLOWER ${CMAKE_SYSTEM_NAME} WAMR_BUILD_PLATFORM)
    endif()
endif()

# WAMR's target arch auto-detection only checks arm64|aarch64 and then
# falls back to pointer size, which picks X86_64/X86_32 when cross-
# compiling for ARM. On macOS with CMAKE_OSX_ARCHITECTURES, the host
# CMAKE_SYSTEM_PROCESSOR (arm64 on Apple Silicon) overrides the target.
# Set WAMR_BUILD_TARGET explicitly for all cross-compilation scenarios.
if(NOT DEFINED WAMR_BUILD_TARGET)
    if(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
        set(WAMR_BUILD_TARGET "AARCH64")
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
        if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
            set(WAMR_BUILD_TARGET "AARCH64")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
            set(WAMR_BUILD_TARGET "ARM")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
            set(WAMR_BUILD_TARGET "X86_64")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
            set(WAMR_BUILD_TARGET "X86_32")
        endif()
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set(WAMR_BUILD_TARGET "X86_32")
    elseif(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES)
        if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
            set(WAMR_BUILD_TARGET "X86_64")
        elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
            set(WAMR_BUILD_TARGET "AARCH64")
        endif()
    endif()
endif()

# Emscripten: disable features unavailable on wasm32 (signal handling,
# native arch assembly).  These CMake variables are consumed by WAMR's
# config_common.cmake and iwasm_common.cmake.
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(WAMR_DISABLE_HW_BOUND_CHECK 1)
    set(WAMR_DISABLE_WAKEUP_BLOCKING_OP 1)
    set(WAMR_BUILD_INVOKE_NATIVE_GENERAL 1)
endif()

# NOTE: This include() calls include_directories() globally — that's why
# binaryen's add_subdirectory must come before this point.
include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

include(${WAMR_ROOT_DIR}/core/shared/utils/shared_utils.cmake)
include(${WAMR_ROOT_DIR}/core/shared/utils/uncommon/shared_uncommon.cmake)

set(WAMR_PLUGIN_SOURCES src/wamr_plugin_api.c src/host_natives/host_natives.c src/host_natives/host_native_lz4.c src/host_natives/host_native_xxhash.c src/host_natives/host_native_blake3.c)
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    find_package(Threads REQUIRED)
endif()

function(set_install_target TARGET_NAME)
    install(TARGETS ${TARGET_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
endfunction()

function(add_wamr_library TARGET_NAME LIB_TYPE)
    add_library(${TARGET_NAME} ${LIB_TYPE} ${WAMR_RUNTIME_LIB_SOURCE})

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_include_directories(${TARGET_NAME} PRIVATE
        ${WAMR_ROOT_DIR}/core/iwasm/include)

    # LLVM JIT: add LLVM include dirs and link libs
    if(WAMR_BUILD_JIT)
        target_include_directories(${TARGET_NAME} PRIVATE ${LLVM_INCLUDE_DIRS})
        target_compile_definitions(${TARGET_NAME} PRIVATE ${LLVM_DEFINITIONS})
    endif()

    set_property(TARGET ${TARGET_NAME} PROPERTY C_VISIBILITY_PRESET hidden)
endfunction()

if(BUILD_SHARED)
    add_wamr_library(libiwasm SHARED)
    set_target_properties(libiwasm PROPERTIES OUTPUT_NAME "iwasm")

    if(TARGET Threads::Threads)
        target_link_libraries(libiwasm PRIVATE Threads::Threads)
    endif()
    if(UNIX)
        target_link_libraries(libiwasm PRIVATE m dl)
    endif()
    if(WIN32)
        target_link_libraries(libiwasm PRIVATE ntdll)
    endif()
    if(WAMR_BUILD_JIT)
        target_link_libraries(libiwasm PRIVATE ${WAMR_LLVM_LIBS})
    endif()

    add_library(wamr SHARED ${WAMR_PLUGIN_SOURCES})
    set_property(TARGET wamr PROPERTY C_VISIBILITY_PRESET hidden)

    set_target_properties(wamr PROPERTIES OUTPUT_NAME "wamr")
    target_compile_definitions(wamr PRIVATE WAMR_PLUGIN_EXPORT)
    if(WIN32)
        target_compile_definitions(wamr PUBLIC WAMR_PLUGIN_IMPORT)
    endif()
    target_link_libraries(wamr PRIVATE libiwasm)

    set_install_target(libiwasm)
    set_install_target(wamr)

    if (BUILD_LAUNCHER)
        add_executable(iwasm ${WAMR_ROOT_DIR}/product-mini/platforms/${WAMR_BUILD_PLATFORM}/main.c 
            ${UNCOMMON_SHARED_SOURCE}
            ${PLATFORM_SHARED_SOURCE}
            ${UTILS_SHARED_SOURCE}
            )
        target_link_libraries(iwasm PRIVATE libiwasm)
        if(WIN32)
            target_link_libraries(iwasm PRIVATE ntdll)
        endif()
        install(TARGETS iwasm DESTINATION bin)
    endif()
else()
    add_wamr_library(wamr-static STATIC)

    target_sources(wamr-static PRIVATE ${WAMR_PLUGIN_SOURCES})

    target_compile_definitions(wamr-static PRIVATE WAMR_HOST_NATIVES_STATIC)
    set_target_properties(wamr-static PROPERTIES OUTPUT_NAME "wamr-static")

    set_install_target(wamr-static)
endif()

install(FILES src/wamr_plugin_api.h DESTINATION include)

# ---------- host_native shared library (for iwasm --native-lib) ----------

if(BUILD_LIBHOST_NATIVE AND BUILD_SHARED AND WAMR_DESKTOP_BUILD)
    set(HOST_NATIVE_SOURCES
        src/host_natives/host_native_lib.c
        src/host_natives/host_native_lz4.c
        src/host_natives/host_native_xxhash.c
        src/host_natives/host_native_blake3.c)

    add_library(host_native SHARED ${HOST_NATIVE_SOURCES})
    set_target_properties(host_native PROPERTIES OUTPUT_NAME "host_native")
    target_include_directories(host_native PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(host_native PRIVATE WAMR_HOST_NATIVES_STATIC HOST_NATIVE_EXPORT_SYMBOLS)
    set_property(TARGET host_native PROPERTY C_VISIBILITY_PRESET hidden)
    target_link_libraries(host_native PRIVATE lz4-static xxhash-static blake3 libiwasm)

    # Only export get_native_lib — hide all statically-linked lz4/xxhash/blake3 symbols.
    if(APPLE)
        target_link_options(host_native PRIVATE
            "LINKER:-exported_symbol,_get_native_lib")
    elseif(UNIX)
        set(_HOST_NATIVE_VERSION_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/host_native.map)
        file(WRITE ${_HOST_NATIVE_VERSION_SCRIPT}
            "{ global: get_native_lib; local: *; };\n")
        target_link_options(host_native PRIVATE
            "LINKER:--version-script,${_HOST_NATIVE_VERSION_SCRIPT}")
    endif()

    set_install_target(host_native)
endif()

# ---------- AOT compiler shared library (desktop only, requires LLVM) ----------

if(BUILD_AOT_COMPILER AND WAMR_DESKTOP_BUILD)
    # Compiler defines matching wamr-compiler/CMakeLists.txt.
    # Scoped to aotclib/wamr_compiler only (NOT global, to avoid leaking into the runtime).
    # WASM_ENABLE_INTERP=1 + WASM_ENABLE_FAST_INTERP=0 override the runtime's
    # fast-interp setting — the AOT compiler needs the classic WASMInterpFrame (with sp).
    set(WAMRC_COMPILE_DEFS
        WASM_ENABLE_WAMR_COMPILER=1
        WASM_ENABLE_INTERP=1
        WASM_ENABLE_FAST_INTERP=0
        WASM_ENABLE_BULK_MEMORY=1
        WASM_ENABLE_BULK_MEMORY_OPT=1
        WASM_DISABLE_HW_BOUND_CHECK=1
        WASM_ENABLE_SHARED_MEMORY=1
        WASM_ENABLE_THREAD_MGR=1
        WASM_ENABLE_TAIL_CALL=1
        WASM_ENABLE_SIMD=1
        WASM_ENABLE_REF_TYPES=1
        WASM_ENABLE_CALL_INDIRECT_OVERLONG=1
        WASM_ENABLE_CUSTOM_NAME_SECTION=1
        WASM_ENABLE_AOT_STACK_FRAME=1
        WASM_ENABLE_DUMP_CALL_STACK=1
        WASM_ENABLE_LOAD_CUSTOM_SECTION=1
        WASM_ENABLE_MODULE_INST_CONTEXT=1
        WASM_ENABLE_MEMORY64=1
        WASM_ENABLE_EXTENDED_CONST_EXPR=1
        WASM_ENABLE_GC=1
        ZYDIS_STATIC_BUILD=1
        ZYCORE_STATIC_BUILD=1
        ${LLVM_DEFINITIONS}
    )

    # Add BUILD_TARGET_* define for aotclib (its CMakeLists.txt clears
    # directory-level definitions, and MSVC lacks the predefined macros
    # that config.h uses for auto-detection).
    if(WAMR_BUILD_TARGET STREQUAL "X86_64")
        list(APPEND WAMRC_COMPILE_DEFS BUILD_TARGET_X86_64)
    elseif(WAMR_BUILD_TARGET STREQUAL "AMD_64")
        list(APPEND WAMRC_COMPILE_DEFS BUILD_TARGET_AMD_64)
    elseif(WAMR_BUILD_TARGET STREQUAL "X86_32")
        list(APPEND WAMRC_COMPILE_DEFS BUILD_TARGET_X86_32)
    elseif(WAMR_BUILD_TARGET MATCHES "AARCH64")
        list(APPEND WAMRC_COMPILE_DEFS BUILD_TARGET_AARCH64)
    endif()

    if(WAMR_BUILD_PLATFORM STREQUAL "windows")
        list(APPEND WAMRC_COMPILE_DEFS COMPILING_WASM_RUNTIME_API=1)
    endif()

    # AOT compilation sources — built in a subdirectory to isolate from
    # directory-level compile definitions set by runtime_lib.cmake.
    set(WAMR_BUILD_WAMR_COMPILER 1)
    include(${WAMR_ROOT_DIR}/core/iwasm/compilation/iwasm_compl.cmake)
    add_subdirectory(cmake/aotclib)

    if(WAMR_ENABLE_AOT_MONOLITH)
        # Monolith mode: link AOT compiler into libiwasm (runtime) and add
        # the plugin API wrapper to wamr.  LLVM must live in libiwasm to
        # avoid duplicate LLVM registration when wamr links libiwasm.
        if(NOT BUILD_SHARED)
            message(FATAL_ERROR "WAMR_ENABLE_AOT_MONOLITH=ON requires BUILD_SHARED=ON")
        endif()
        target_link_libraries(libiwasm PRIVATE aotclib vmlib-compiler ${WAMR_LLVM_LIBS})
        target_sources(wamr PRIVATE src/wamrc_plugin_api.c)
    else()
        # Separate wamr_compiler shared library
        add_library(wamr_compiler SHARED src/wamrc_plugin_api.c)
        target_include_directories(wamr_compiler PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${WAMR_ROOT_DIR}/core/iwasm/include
            ${LLVM_INCLUDE_DIRS})
        target_compile_definitions(wamr_compiler PRIVATE WAMR_PLUGIN_EXPORT ${WAMRC_COMPILE_DEFS})
        set_property(TARGET wamr_compiler PROPERTY C_VISIBILITY_PRESET hidden)
        set_target_properties(wamr_compiler PROPERTIES POSITION_INDEPENDENT_CODE ON)

        if(WIN32)
            target_compile_definitions(wamr_compiler PUBLIC WAMR_PLUGIN_IMPORT)
        endif()

        target_link_libraries(wamr_compiler PRIVATE aotclib vmlib-compiler ${WAMR_LLVM_LIBS})

        if(NOT MSVC)
            target_link_libraries(wamr_compiler PRIVATE -lm -lpthread -ldl)
        endif()
        if(WIN32)
            target_link_libraries(wamr_compiler PRIVATE ntdll)
        endif()

        install(TARGETS wamr_compiler
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin)
    endif()

    install(FILES src/wamrc_plugin_api.h DESTINATION include)
endif()

# ---------- Test WASM module assembly ----------
# Use binaryen's wasm-as to compile tests/add.wat, then embed the resulting
# binary as a C header via cmake/embed_file.cmake.

if(WAMR_DESKTOP_BUILD)
    set(ADD_WASM_BIN ${CMAKE_CURRENT_BINARY_DIR}/add.wasm)
    set(ADD_WASM_HDR ${CMAKE_CURRENT_BINARY_DIR}/generated_add_wasm.h)

    if(WASM_AS_EXECUTABLE)
        set(_WASM_AS_CMD ${WASM_AS_EXECUTABLE})
        set(_WASM_AS_DEP "")
    else()
        set(_WASM_AS_CMD $<TARGET_FILE:wasm-as>)
        set(_WASM_AS_DEP wasm-as)
    endif()

    add_custom_command(
        OUTPUT  ${ADD_WASM_BIN}
        COMMAND ${_WASM_AS_CMD}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/add.wat
                -o ${ADD_WASM_BIN}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/add.wat ${_WASM_AS_DEP}
        COMMENT "Assembling add.wat -> add.wasm"
    )

    add_custom_command(
        OUTPUT  ${ADD_WASM_HDR}
        COMMAND ${CMAKE_COMMAND}
                -DINPUT_FILE=${ADD_WASM_BIN}
                -DOUTPUT_FILE=${ADD_WASM_HDR}
                -DVAR_NAME=add_wasm
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake
        DEPENDS ${ADD_WASM_BIN}
        COMMENT "Embedding add.wasm as C header"
    )

    if(BUILD_LIBHOST_NATIVE AND BUILD_SHARED)
        set(NATIVE_LIB_TEST_WASM ${CMAKE_CURRENT_BINARY_DIR}/native_lib_test.wasm)
        add_custom_command(
            OUTPUT  ${NATIVE_LIB_TEST_WASM}
            COMMAND ${_WASM_AS_CMD}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_lib_test.wat
                    -o ${NATIVE_LIB_TEST_WASM}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_lib_test.wat ${_WASM_AS_DEP}
            COMMENT "Assembling native_lib_test.wat -> native_lib_test.wasm"
        )
        add_custom_target(native_lib_test_wasm ALL DEPENDS ${NATIVE_LIB_TEST_WASM})
    endif()
endif()

# ---------- Tests ----------

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND WAMR_DESKTOP_BUILD)
    add_executable(test_wamr tests/test_wamr.c ${ADD_WASM_HDR})
    target_include_directories(test_wamr PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR})

    if(BUILD_SHARED)
        target_link_libraries(test_wamr PRIVATE wamr)
    else()
        target_link_libraries(test_wamr PRIVATE wamr-static)
        if(TARGET Threads::Threads)
            target_link_libraries(test_wamr PRIVATE Threads::Threads)
        endif()
        if(UNIX)
            target_link_libraries(test_wamr PRIVATE m)
        endif()
    endif()

    add_test(NAME test_wamr COMMAND test_wamr)

    if(BUILD_LIBHOST_NATIVE AND BUILD_SHARED AND TARGET iwasm AND NOT WIN32)
        add_test(
            NAME test_native_lib
            COMMAND $<TARGET_FILE:iwasm>
                    --native-lib=$<TARGET_FILE:host_native>
                    ${NATIVE_LIB_TEST_WASM})
    endif()

    if(BUILD_AOT_COMPILER)
        if(WAMR_ENABLE_AOT_MONOLITH)
            # Monolith: wamr shared lib already contains the AOT compiler
            add_executable(test_wamr_compiler
                tests/test_wamr_compiler.c
                ${ADD_WASM_HDR})
            target_include_directories(test_wamr_compiler PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_BINARY_DIR})
            target_link_libraries(test_wamr_compiler PRIVATE wamr)
        else()
            add_executable(test_wamr_compiler
                tests/test_wamr_compiler.c
                src/wamrc_plugin_api.c
                ${ADD_WASM_HDR})
            target_include_directories(test_wamr_compiler PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_BINARY_DIR}
                ${WAMR_ROOT_DIR}/core/iwasm/include
                ${LLVM_INCLUDE_DIRS})
            target_link_libraries(test_wamr_compiler PRIVATE
                aotclib vmlib-compiler ${WAMR_LLVM_LIBS})
            if(TARGET Threads::Threads)
                target_link_libraries(test_wamr_compiler PRIVATE Threads::Threads)
            endif()
            if(NOT MSVC)
                target_link_libraries(test_wamr_compiler PRIVATE -lm -lpthread -ldl)
            endif()
            if(WIN32)
                target_link_libraries(test_wamr_compiler PRIVATE ntdll)
            endif()
        endif()

        add_test(NAME test_wamr_compiler COMMAND test_wamr_compiler)
    endif()
endif()
