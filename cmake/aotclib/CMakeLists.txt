# Build the AOT compiler libraries in an isolated directory scope.
#
# The parent's runtime_lib.cmake sets directory-level compile definitions
# (e.g. WASM_ENABLE_FAST_INTERP=1) that conflict with the AOT compiler.
# Building here gives us a clean scope with compiler-specific defines.
#
# We build two targets:
#   vmlib-compiler — WAMR runtime compiled with compiler-specific defines
#   aotclib        — AOT compilation sources (LLVM codegen)
#
# This mirrors the upstream wamr-compiler/CMakeLists.txt structure.

# Clear all inherited directory-level compile definitions.
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "")

# All variables below (WAMRC_COMPILE_DEFS, WAMR_ROOT_DIR, WAMR_BUILD_PLATFORM,
# LLVM_INCLUDE_DIRS, etc.) are inherited from the parent scope.

# ---------- Include WAMR component cmake files ----------
# These set source-list variables (e.g. IWASM_COMMON_SOURCE) without
# add_definitions — safe to include here.

set(SHARED_DIR ${WAMR_ROOT_DIR}/core/shared)
set(IWASM_DIR ${WAMR_ROOT_DIR}/core/iwasm)

set(WAMR_BUILD_WAMR_COMPILER 1)
set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 0)
set(WAMR_BUILD_AOT 1)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_SHARED_MEMORY 1)
set(WAMR_BUILD_THREAD_MGR 1)
set(WAMR_BUILD_BULK_MEMORY 1)
set(WAMR_BUILD_REF_TYPES 1)
set(WAMR_BUILD_SIMD 1)
set(WAMR_BUILD_TAIL_CALL 1)
set(WAMR_BUILD_GC 1)
set(WAMR_BUILD_STRINGREF 1)
set(WAMR_STRINGREF_IMPL_SOURCE "STUB")

include(${SHARED_DIR}/platform/${WAMR_BUILD_PLATFORM}/shared_platform.cmake)
include(${SHARED_DIR}/mem-alloc/mem_alloc.cmake)
include(${SHARED_DIR}/utils/shared_utils.cmake)
include(${SHARED_DIR}/utils/uncommon/shared_uncommon.cmake)
include(${IWASM_DIR}/libraries/thread-mgr/thread_mgr.cmake)
include(${IWASM_DIR}/common/iwasm_common.cmake)
include(${IWASM_DIR}/common/gc/iwasm_gc.cmake)
include(${IWASM_DIR}/interpreter/iwasm_interp.cmake)
include(${IWASM_DIR}/aot/iwasm_aot.cmake)
include(${IWASM_DIR}/compilation/iwasm_compl.cmake)
include(${WAMR_ROOT_DIR}/build-scripts/version.cmake)
include(${IWASM_DIR}/libraries/shared-heap/shared_heap.cmake)

if(WAMR_BUILD_LIBC_BUILTIN EQUAL 1)
    include(${IWASM_DIR}/libraries/libc-builtin/libc_builtin.cmake)
endif()

# ---------- vmlib-compiler: WAMR runtime with compiler defines ----------

add_library(vmlib-compiler STATIC
    ${PLATFORM_SHARED_SOURCE}
    ${MEM_ALLOC_SHARED_SOURCE}
    ${UTILS_SHARED_SOURCE}
    ${UNCOMMON_SHARED_SOURCE}
    ${THREAD_MGR_SOURCE}
    ${LIBC_BUILTIN_SOURCE}
    ${LIB_SHARED_HEAP_SOURCE}
    ${IWASM_COMMON_SOURCE}
    ${IWASM_INTERP_SOURCE}
    ${IWASM_AOT_SOURCE}
    ${IWASM_GC_SOURCE})

set_target_properties(vmlib-compiler PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(vmlib-compiler PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(vmlib-compiler PRIVATE ${WAMRC_COMPILE_DEFS})

# ---------- aotclib: AOT compilation sources (LLVM codegen) ----------

add_library(aotclib STATIC ${IWASM_COMPL_SOURCE})
set_target_properties(aotclib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(aotclib PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(aotclib PRIVATE ${WAMRC_COMPILE_DEFS})

# Disable RTTI and exceptions to match LLVM's build settings
if(MSVC)
    target_compile_options(aotclib PRIVATE /GR- /EHs-c-)
else()
    target_compile_options(aotclib PRIVATE -fno-rtti -fno-exceptions)
endif()
