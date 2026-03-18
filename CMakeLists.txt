cmake_minimum_required(VERSION 3.21)
project(wamr-unity VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED "Build shared library" ON)
option(USE_MSVC_STATIC_RUNTIME "Use MSVC static runtime" OFF)

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
endif()

# ---------- Platform-specific settings ----------

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(BUILD_SHARED OFF)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -g1 -DNDEBUG")
    add_compile_options(-fsigned-char)
    add_link_options("-Wl,-z,max-page-size=16384")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
    set(BUILD_SHARED OFF)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -g1 -DNDEBUG")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(BUILD_SHARED OFF)
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

# ---------- Binaryen (desktop only) ----------
# IMPORTANT: add_subdirectory MUST happen BEFORE the WAMR include() below,
# because WAMR's runtime_lib.cmake calls include_directories() globally and
# its wasm.h would shadow binaryen's own wasm.h header.

if(WAMR_DESKTOP_BUILD)
    enable_language(CXX)

    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIB OFF CACHE BOOL "" FORCE)
    set(BUILD_LLVM_DWARF ON CACHE BOOL "" FORCE)
    set(BYN_ENABLE_LTO OFF CACHE BOOL "" FORCE)
    set(ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    add_subdirectory(binaryen EXCLUDE_FROM_ALL)

    install(TARGETS binaryen
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
endif()

# ---------- WAMR source (git submodule at wamr/, pinned to WAMR-2.4.4) ----------

set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wamr)

# ---------- WAMR build configuration ----------
# Fast interpreter, builtin libc, no filesystem, no JIT/AOT, no threading.

set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_SHARED_MEMORY 0)
set(WAMR_BUILD_THREAD_MGR 0)
set(WAMR_BUILD_BULK_MEMORY 1)
set(WAMR_BUILD_REF_TYPES 0)
set(WAMR_BUILD_SIMD 0)
set(WAMR_BUILD_TAIL_CALL 0)

# ---------- Build WAMR library targets ----------

# Emscripten: WAMR cannot run inside WASM — build stubs only.
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(WAMR_UNITY_SOURCES src/wamr_unity_api.c)
    add_compile_definitions(WAMR_STUB_ONLY)
else()
    # WAMR's runtime_lib.cmake requires WAMR_BUILD_PLATFORM to be set.
    # (Auto-detection only exists in WAMR's root CMakeLists.txt, which we skip.)
    if(NOT DEFINED WAMR_BUILD_PLATFORM)
        if(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
            set(WAMR_BUILD_PLATFORM "darwin")
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
        elseif(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES)
            if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
                set(WAMR_BUILD_TARGET "X86_64")
            elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
                set(WAMR_BUILD_TARGET "AARCH64")
            endif()
        endif()
    endif()

    # NOTE: This include() calls include_directories() globally — that's why
    # binaryen's add_subdirectory must come before this point.
    include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
    set(WAMR_UNITY_SOURCES src/wamr_unity_api.c ${WAMR_RUNTIME_LIB_SOURCE})
    find_package(Threads REQUIRED)
endif()

function(add_wamr_library TARGET_NAME LIB_TYPE)
    add_library(${TARGET_NAME} ${LIB_TYPE} ${WAMR_UNITY_SOURCES})

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

    if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        target_include_directories(${TARGET_NAME} PRIVATE
            ${WAMR_ROOT_DIR}/core/iwasm/include)
    endif()

    set_property(TARGET ${TARGET_NAME} PROPERTY C_VISIBILITY_PRESET hidden)

    if(LIB_TYPE STREQUAL "STATIC")
        set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "wamr-static")
    else()
        set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "wamr")
        target_compile_definitions(${TARGET_NAME} PRIVATE WAMR_UNITY_EXPORT)
        if(WIN32)
            target_compile_definitions(${TARGET_NAME} PUBLIC WAMR_UNITY_IMPORT)
        endif()

        if(TARGET Threads::Threads)
            target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads)
        endif()
        if(UNIX)
            target_link_libraries(${TARGET_NAME} PRIVATE m)
        endif()
        if(WIN32)
            target_link_libraries(${TARGET_NAME} PRIVATE ntdll)
        endif()
    endif()

    install(TARGETS ${TARGET_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
endfunction()

add_wamr_library(wamr-static STATIC)

install(FILES src/wamr_unity_api.h DESTINATION include)

if(BUILD_SHARED)
    add_wamr_library(wamr SHARED)
endif()

# ---------- Test WASM module assembly ----------
# Use binaryen's wasm-as to compile tests/add.wat, then embed the resulting
# binary as a C header via cmake/embed_file.cmake.

if(WAMR_DESKTOP_BUILD)
    set(ADD_WASM_BIN ${CMAKE_CURRENT_BINARY_DIR}/add.wasm)
    set(ADD_WASM_HDR ${CMAKE_CURRENT_BINARY_DIR}/generated_add_wasm.h)

    add_custom_command(
        OUTPUT  ${ADD_WASM_BIN}
        COMMAND $<TARGET_FILE:wasm-as>
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/add.wat
                -o ${ADD_WASM_BIN}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/add.wat wasm-as
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
endif()

# ---------- Tests ----------

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND WAMR_DESKTOP_BUILD)
    add_executable(test_wamr tests/test_wamr.c ${ADD_WASM_HDR})
    target_include_directories(test_wamr PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR})

    target_link_libraries(test_wamr PRIVATE wamr-static)

    if(TARGET Threads::Threads)
        target_link_libraries(test_wamr PRIVATE Threads::Threads)
    endif()
    if(UNIX)
        target_link_libraries(test_wamr PRIVATE m)
    endif()

    add_test(NAME test_wamr COMMAND test_wamr)
endif()
