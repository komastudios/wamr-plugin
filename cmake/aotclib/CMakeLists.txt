# Build aotclib in its own directory scope so we can override the
# directory-level compile definitions inherited from runtime_lib.cmake.
# The parent sets WASM_ENABLE_FAST_INTERP=1 (fast interpreter), but the
# AOT compiler needs the classic WASMInterpFrame layout (with sp member),
# which requires WASM_ENABLE_FAST_INTERP=0.  Because compilers use
# "last -D wins", and CMake puts directory-level defs after target-level
# defs, target_compile_definitions alone cannot override — hence this
# isolated scope.

# Clear all inherited directory-level compile definitions and set
# exactly what the AOT compiler needs.
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "")

# All variables below (WAMRC_COMPILE_DEFS, IWASM_COMPL_SOURCE,
# LLVM_INCLUDE_DIRS, etc.) are inherited from the parent scope.

add_library(aotclib STATIC ${IWASM_COMPL_SOURCE})
set_target_properties(aotclib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(aotclib PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(aotclib PRIVATE ${WAMRC_COMPILE_DEFS})

# Disable RTTI for LLVM compatibility
if(MSVC)
    target_compile_options(aotclib PRIVATE /GR-)
else()
    target_compile_options(aotclib PRIVATE -fno-rtti)
endif()
