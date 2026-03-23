#ifndef HOST_NATIVE_LZ4_H
#define HOST_NATIVE_LZ4_H

#include <stdbool.h>

/* Initialize LZ4 host natives and register with WAMR.
 * lib_path: path or name of the LZ4 shared library (NULL = platform default).
 * For static builds (WAMR_HOST_NATIVES_STATIC), lib_path is ignored. */
bool host_native_lz4_init(const char *lib_path);

#endif
