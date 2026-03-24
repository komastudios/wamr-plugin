# shared_platform.cmake — Emscripten platform config for WAMR.
#
# Based on the linux platform but uses a custom platform_internal.h that
# avoids WASI type redefinitions and unsupported POSIX features.

set(PLATFORM_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR})

add_definitions(-DBH_PLATFORM_EMSCRIPTEN)
add_definitions(-DBH_PLATFORM_LINUX)

include_directories(${PLATFORM_SHARED_DIR})
include_directories(${PLATFORM_SHARED_DIR}/../../../wamr/core/shared/platform/include)

include(${CMAKE_CURRENT_LIST_DIR}/../../../wamr/core/shared/platform/common/posix/platform_api_posix.cmake)

file(GLOB_RECURSE source_all ${PLATFORM_SHARED_DIR}/*.c)

set(PLATFORM_SHARED_SOURCE ${source_all} ${PLATFORM_COMMON_POSIX_SOURCE})

file(GLOB header ${PLATFORM_SHARED_DIR}/../../../wamr/core/shared/platform/include/*.h)
list(APPEND RUNTIME_LIB_HEADER_LIST ${header})
